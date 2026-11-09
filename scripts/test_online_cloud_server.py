#!/usr/bin/env python3
"""End-to-end PGON smoke test through the public MQTT NAT bridge."""

from __future__ import annotations

import argparse
import secrets
import socket
import struct


FRAME = struct.Struct("<IBBHIII")
HELLO = struct.Struct("<B3xII")
HELLO_V1 = struct.Struct("<B3xI")
PEER_ENTRY = struct.Struct("<I")
GAME_PACKET = struct.Struct("<HBBIIH")
MAGIC = 0x4E4F4750
VERSION = 2
GAME_MAGIC = 0x4750
GAME_VERSION = 10
BOT_PLAYER_ID = 99999
ROOT = "pokegochi/online/v1/8f4d1bd86a58468fbbcf120a6f63bd79"
INVITE = 1
INVITE_ACCEPT = 2
ACTIVITY_REQUEST = 5
ACTIVITY_REJECT = 7


def mqtt_string(value: str) -> bytes:
    encoded = value.encode()
    return struct.pack(">H", len(encoded)) + encoded


def remaining_length(length: int) -> bytes:
    encoded = bytearray()
    while True:
        byte = length % 128
        length //= 128
        if length:
            byte |= 0x80
        encoded.append(byte)
        if not length:
            return bytes(encoded)


class CloudClient:
    def __init__(self, broker: str, player_id: int,
                 protocol_version: int = VERSION) -> None:
        self.player_id = player_id
        self.protocol_version = protocol_version
        self.nonce = secrets.randbits(32) or 1
        route = f"{player_id:05d}/{self.nonce:08x}"
        self.publish_topic = f"{ROOT}/server/{route}"
        self.subscribe_topic = f"{ROOT}/client/{route}"
        self.socket = socket.create_connection((broker, 1883), timeout=8)
        self.socket.settimeout(8)
        client_id = f"pgt-{player_id:05d}-{self.nonce:08x}"
        variable = mqtt_string("MQTT") + bytes((4, 2, 0, 20)) + mqtt_string(client_id)
        self._mqtt_send(0x10, variable)
        if self._mqtt_receive()[0] != 0x20:
            raise AssertionError("MQTT connection rejected")
        self._mqtt_send(0x82, b"\0\1" + mqtt_string(self.subscribe_topic) + b"\0")
        if self._mqtt_receive()[0] != 0x90:
            raise AssertionError("MQTT subscription rejected")

    def close(self) -> None:
        try:
            self.socket.sendall(b"\xe0\0")
        finally:
            self.socket.close()

    def send(self, message_type: int, *, target: int = 0, session: int = 0,
             payload: bytes = b"") -> None:
        frame = FRAME.pack(MAGIC, self.protocol_version, message_type, len(payload),
                           self.player_id, target, session) + payload
        self._mqtt_send(0x30, mqtt_string(self.publish_topic) + frame)

    def receive(self) -> tuple[int, int, int, int, bytes]:
        while True:
            control, body = self._mqtt_receive()
            if control & 0xF0 == 0xD0:
                continue
            if control & 0xF0 != 0x30:
                continue
            topic_length = struct.unpack_from(">H", body)[0]
            frame = body[2 + topic_length:]
            magic, version, kind, length, source, target, session = FRAME.unpack_from(frame)
            if (magic != MAGIC or version != self.protocol_version or
                    len(frame) != FRAME.size + length):
                raise AssertionError("invalid PGON frame from MQTT")
            return kind, source, target, session, frame[FRAME.size:]

    def _mqtt_send(self, control: int, body: bytes) -> None:
        self.socket.sendall(bytes((control,)) + remaining_length(len(body)) + body)

    def _mqtt_receive(self) -> tuple[int, bytes]:
        control = self._exact(1)[0]
        length = 0
        multiplier = 1
        while True:
            encoded = self._exact(1)[0]
            length += (encoded & 127) * multiplier
            if not encoded & 128:
                break
            multiplier *= 128
        return control, self._exact(length)

    def _exact(self, length: int) -> bytes:
        data = bytearray()
        while len(data) < length:
            block = self.socket.recv(length - len(data))
            if not block:
                raise ConnectionError("MQTT broker closed")
            data.extend(block)
        return bytes(data)


def expect(client: CloudClient, message_type: int) -> tuple[int, int, int, bytes]:
    kind, source, target, session, payload = client.receive()
    if kind != message_type:
        raise AssertionError(f"expected PGON {message_type}, got {kind}")
    return source, target, session, payload


def game_packet(kind: int, source: int, payload: bytes = b"",
                transaction: int = 0) -> bytes:
    return GAME_PACKET.pack(
        GAME_MAGIC, GAME_VERSION, kind, source, transaction, len(payload)
    ) + payload


def expect_game(client: CloudClient, expected_kind: int) -> tuple[int, int, bytes]:
    source, _, _, packet = expect(client, 11)
    magic, version, kind, packet_source, transaction, length = GAME_PACKET.unpack_from(
        packet
    )
    payload = packet[GAME_PACKET.size:]
    if (
        magic != GAME_MAGIC
        or version != GAME_VERSION
        or kind != expected_kind
        or packet_source != source
        or len(payload) != length
    ):
        raise AssertionError("invalid bot PGCH packet through MQTT")
    return source, transaction, payload


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--broker", default="broker.emqx.io")
    parser.add_argument("--access-code", type=int, default=579856)
    args = parser.parse_args()
    first = CloudClient(args.broker, 72001)
    second = CloudClient(args.broker, 72002)
    legacy = CloudClient(args.broker, 72003, 1)
    try:
        for client, balanced_wins in ((first, 31), (second, 42)):
            client.send(
                1,
                payload=HELLO.pack(
                    GAME_VERSION, args.access_code, balanced_wins
                ),
            )
            expect(client, 2)
        legacy.send(1, payload=HELLO_V1.pack(GAME_VERSION, args.access_code))
        expect(legacy, 2)
        first.send(3)
        peers = {}
        while True:
            kind, source, _, _, payload = first.receive()
            if kind == 5:
                break
            if kind != 4:
                raise AssertionError(f"unexpected peer-list message {kind}")
            if len(payload) != PEER_ENTRY.size:
                raise AssertionError("peer entry omitted the Balanced victory counter")
            peers[source] = PEER_ENTRY.unpack(payload)[0]
        assert second.player_id in peers and legacy.player_id in peers and BOT_PLAYER_ID in peers
        assert peers[second.player_id] == 42
        legacy.send(3)
        legacy_peers = set()
        while True:
            kind, source, _, _, payload = legacy.receive()
            if kind == 5:
                break
            if kind != 4 or payload:
                raise AssertionError("PGON v1 peer list is not backward compatible")
            legacy_peers.add(source)
        assert first.player_id in legacy_peers and BOT_PLAYER_ID in legacy_peers
        first.send(6, target=second.player_id)
        source, _, session, outbound = expect(first, 7)
        assert source == second.player_id and outbound == b"\1"
        source, _, second_session, outbound = expect(second, 7)
        assert source == first.player_id and second_session == session and outbound == b"\0"
        game = GAME_PACKET.pack(GAME_MAGIC, GAME_VERSION, 1,
                                first.player_id, 0x12345678, 0)
        first.send(11, target=second.player_id, session=session, payload=game)
        source, _, relayed_session, payload = expect(second, 11)
        assert source == first.player_id and relayed_session == session and payload == game
        first.send(9, target=second.player_id, session=session)
        expect(first, 10)
        expect(second, 10)

        # The virtual player must be reachable through the Internet route too,
        # not only through the local TCP shortcut.
        first.send(6, target=BOT_PLAYER_ID)
        source, _, bot_session, outbound = expect(first, 7)
        assert source == BOT_PLAYER_ID and outbound == b"\1"
        first.send(
            11,
            target=BOT_PLAYER_ID,
            session=bot_session,
            payload=game_packet(INVITE, first.player_id, transaction=0xC001),
        )
        source, transaction, _ = expect_game(first, INVITE_ACCEPT)
        assert source == BOT_PLAYER_ID and transaction == 0xC001
        first.send(
            11,
            target=BOT_PLAYER_ID,
            session=bot_session,
            payload=game_packet(
                ACTIVITY_REQUEST, first.player_id, b"\x02\x00", 0xC002
            ),
        )
        source, transaction, refusal = expect_game(first, ACTIVITY_REJECT)
        assert source == BOT_PLAYER_ID and transaction == 0xC002
        assert refusal == b"\x02\x00"
        first.send(9, target=BOT_PLAYER_ID, session=bot_session)
        expect(first, 10)
    finally:
        first.close()
        second.close()
        legacy.close()
    print("PASS: Internet MQTT path authenticated PGON v1/v2 players, exposed Balanced wins, relayed PGCH byte-exactly and reached bot 99999")


if __name__ == "__main__":
    main()
