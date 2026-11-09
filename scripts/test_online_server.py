#!/usr/bin/env python3
"""End-to-end smoke test for a running Pokegochi ESP32 relay."""

from __future__ import annotations

import argparse
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
INVITE = 1
INVITE_ACCEPT = 2
ACTIVITY_REQUEST = 5
ACTIVITY_ACCEPT = 6
ACTIVITY_REJECT = 7
BATTLE_PARTY_MEMBER = 13
BATTLE_PARTY_READY = 14
BATTLE_LEAD = 15
BATTLE_START = 16
BATTLE_COMMAND = 18
BATTLE_SYNC_POKEMON = 19
BATTLE_SYNC_CORE = 21
BATTLE_SYNC_ACK = 22


class RelayClient:
    def __init__(self, host: str, port: int, player_id: int,
                 protocol_version: int = VERSION) -> None:
        self.player_id = player_id
        self.protocol_version = protocol_version
        self.socket = socket.create_connection((host, port), timeout=5.0)
        self.socket.settimeout(5.0)

    def close(self) -> None:
        self.socket.close()

    def send(
        self,
        message_type: int,
        *,
        target: int = 0,
        session: int = 0,
        payload: bytes = b"",
    ) -> None:
        self.socket.sendall(
            FRAME.pack(
                MAGIC,
                self.protocol_version,
                message_type,
                len(payload),
                self.player_id,
                target,
                session,
            )
            + payload
        )

    def receive(self) -> tuple[int, int, int, int, bytes]:
        header = self._receive_exact(FRAME.size)
        magic, version, message_type, length, source, target, session = FRAME.unpack(
            header
        )
        if magic != MAGIC or version != self.protocol_version:
            raise AssertionError("invalid PGON response header")
        return message_type, source, target, session, self._receive_exact(length)

    def _receive_exact(self, length: int) -> bytes:
        data = bytearray()
        while len(data) < length:
            block = self.socket.recv(length - len(data))
            if not block:
                raise ConnectionError("relay closed the connection")
            data.extend(block)
        return bytes(data)


def expect(client: RelayClient, expected_type: int) -> tuple[int, int, int, bytes]:
    message_type, source, target, session, payload = client.receive()
    if message_type != expected_type:
        raise AssertionError(
            f"expected message {expected_type}, received {message_type}"
        )
    return source, target, session, payload


def game_packet(kind: int, source: int, payload: bytes = b"", transaction: int = 0) -> bytes:
    return GAME_PACKET.pack(
        GAME_MAGIC, GAME_VERSION, kind, source, transaction, len(payload)
    ) + payload


def send_game(
    client: RelayClient,
    target: int,
    session: int,
    kind: int,
    payload: bytes = b"",
    transaction: int = 0,
) -> None:
    client.send(
        11,
        target=target,
        session=session,
        payload=game_packet(kind, client.player_id, payload, transaction),
    )


def expect_game(
    client: RelayClient, expected_kind: int
) -> tuple[int, int, bytes]:
    source, _, session, packet = expect(client, 11)
    magic, version, kind, packet_source, transaction, length = GAME_PACKET.unpack_from(
        packet
    )
    payload = packet[GAME_PACKET.size :]
    if (
        magic != GAME_MAGIC
        or version != GAME_VERSION
        or kind != expected_kind
        or packet_source != source
        or len(payload) != length
    ):
        raise AssertionError(
            f"invalid bot PGCH packet kind={kind}, expected={expected_kind}"
        )
    return source, transaction, payload


def peer_list(client: RelayClient) -> dict[int, int]:
    client.send(3)
    peers: dict[int, int] = {}
    while True:
        message_type, source, _, _, payload = client.receive()
        if message_type == 5:
            return peers
        if message_type != 4:
            raise AssertionError(f"unexpected peer-list message {message_type}")
        if client.protocol_version == 1:
            if payload:
                raise AssertionError("legacy peer entry unexpectedly has a payload")
            peers[source] = 0
        else:
            if len(payload) != PEER_ENTRY.size:
                raise AssertionError("peer entry omitted the Balanced victory counter")
            peers[source] = PEER_ENTRY.unpack(payload)[0]


def pokemon_with_hp(record: bytes, hp: int) -> bytes:
    if len(record) != 64:
        raise AssertionError("OwnedPokemon ABI changed")
    changed = bytearray(record)
    struct.pack_into("<H", changed, 16, hp)
    return bytes(changed)


def pokemon_at_full_hp(record: bytes) -> bytes:
    return pokemon_with_hp(record, struct.unpack_from("<H", record, 18)[0])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="pokegochi-server.local")
    parser.add_argument("--port", type=int, default=32145)
    parser.add_argument("--access-code", type=int, required=True)
    arguments = parser.parse_args()

    first = RelayClient(arguments.host, arguments.port, 71001)
    second = RelayClient(arguments.host, arguments.port, 71002)
    legacy = RelayClient(arguments.host, arguments.port, 71003, 1)
    recovered = None
    try:
        for client, balanced_wins in ((first, 11), (second, 22)):
            client.send(
                1,
                payload=HELLO.pack(
                    GAME_VERSION, arguments.access_code, balanced_wins
                ),
            )
            expect(client, 2)

        legacy.send(
            1, payload=HELLO_V1.pack(GAME_VERSION, arguments.access_code)
        )
        expect(legacy, 2)

        available = peer_list(first)
        if (second.player_id not in available or legacy.player_id not in available or
                BOT_PLAYER_ID not in available):
            raise AssertionError("peer discovery omitted a real player or the server bot")
        if available[second.player_id] != 22:
            raise AssertionError("relay changed a real player's Balanced win counter")
        initial_bot_wins = available[BOT_PLAYER_ID]
        legacy_available = peer_list(legacy)
        if first.player_id not in legacy_available or BOT_PLAYER_ID not in legacy_available:
            raise AssertionError("PGON v1 client cannot discover current players or bot")

        first.send(6, target=second.player_id)
        source, _, first_session, outbound = expect(first, 7)
        if source != second.player_id or outbound != b"\x01":
            raise AssertionError("outbound session negotiation is invalid")
        source, _, second_session, outbound = expect(second, 7)
        if (
            source != first.player_id
            or second_session != first_session
            or outbound != b"\x00"
        ):
            raise AssertionError("inbound session negotiation is invalid")

        # A delayed close from an older room must be rejected without
        # touching the current room.
        first.send(9, target=second.player_id, session=first_session ^ 0x55AA55AA)
        _, _, error_session, error_payload = expect(first, 14)
        if error_session != first_session or error_payload != b"\x07":
            raise AssertionError("stale close did not return SESSION_MISMATCH")

        game_packet = GAME_PACKET.pack(
            GAME_MAGIC, GAME_VERSION, 1, first.player_id, 0x12345678, 0
        )
        first.send(
            11,
            target=second.player_id,
            session=first_session,
            payload=game_packet,
        )
        source, _, relayed_session, payload = expect(second, 11)
        if (
            source != first.player_id
            or relayed_session != first_session
            or payload != game_packet
        ):
            raise AssertionError("game packet changed while crossing the relay")

        # Abruptly losing either endpoint must atomically close the room for
        # its peer. Then the same player id can reconnect and create a fresh
        # room; no ghost session may remain on the relay.
        second.close()
        source, _, closed_session, _ = expect(first, 10)
        if source != second.player_id or closed_session != first_session:
            raise AssertionError("abrupt disconnect closed the wrong room")

        # CLOSE is idempotent after the server has already cleared the room.
        first.send(9, target=second.player_id, session=first_session)
        expect(first, 10)

        recovered = RelayClient(arguments.host, arguments.port, second.player_id)
        recovered.send(
            1, payload=HELLO.pack(GAME_VERSION, arguments.access_code, 22)
        )
        expect(recovered, 2)
        first.send(6, target=recovered.player_id)
        _, _, replacement_session, _ = expect(first, 7)
        _, _, recovered_session, _ = expect(recovered, 7)
        if replacement_session != recovered_session or replacement_session == first_session:
            raise AssertionError("relay did not create a clean replacement room")
        first.send(9, target=recovered.player_id, session=replacement_session)
        expect(first, 10)
        expect(recovered, 10)

        # The server bot is a single virtual player. It accepts the invitation,
        # refuses trade, then accepts a Balanced battle and publishes exactly
        # three legal disposable Pokemon snapshots.
        first.send(6, target=BOT_PLAYER_ID)
        source, _, bot_session, outbound = expect(first, 7)
        if source != BOT_PLAYER_ID or outbound != b"\x01":
            raise AssertionError("bot session negotiation is invalid")
        send_game(first, BOT_PLAYER_ID, bot_session, INVITE, transaction=0xB001)
        source, transaction, _ = expect_game(first, INVITE_ACCEPT)
        if source != BOT_PLAYER_ID or transaction != 0xB001:
            raise AssertionError("bot did not accept the connection invitation")

        send_game(first, BOT_PLAYER_ID, bot_session, ACTIVITY_REQUEST, b"\x02\x00")
        _, _, rejection = expect_game(first, ACTIVITY_REJECT)
        if rejection != b"\x02\x00":
            raise AssertionError("bot did not explicitly refuse trade")

        send_game(first, BOT_PLAYER_ID, bot_session, ACTIVITY_REQUEST, b"\x01\x02")
        _, _, acceptance = expect_game(first, ACTIVITY_ACCEPT)
        if acceptance != b"\x01\x02":
            raise AssertionError("bot did not accept the Balanced battle")
        bot_team: list[bytes] = []
        for expected_slot in range(3):
            _, _, member = expect_game(first, BATTLE_PARTY_MEMBER)
            if len(member) != 65 or member[0] != expected_slot:
                raise AssertionError("bot sent a malformed party member")
            pokemon = member[1:]
            if pokemon[20] != 70:
                raise AssertionError("bot did not normalize a Balanced team to Lv70")
            bot_team.append(pokemon)
        if len({struct.unpack_from("<H", pokemon, 8)[0] for pokemon in bot_team}) != 3:
            raise AssertionError("bot selected a duplicate species in one team")
        _, _, ready = expect_game(first, BATTLE_PARTY_READY)
        _, _, lead = expect_game(first, BATTLE_LEAD)
        if ready != b"\x03" or lead != b"\x00":
            raise AssertionError("bot party readiness/lead is invalid")

        # While this room exists the bot neither appears to another player nor
        # permits a second room to be opened against its reserved id.
        if BOT_PLAYER_ID in peer_list(recovered):
            raise AssertionError("busy bot remained visible in discovery")
        recovered.send(6, target=BOT_PLAYER_ID)
        source, _, _, _ = expect(recovered, 8)
        if source != BOT_PLAYER_ID:
            raise AssertionError("busy bot rejection identified the wrong peer")

        send_game(
            first,
            BOT_PLAYER_ID,
            bot_session,
            BATTLE_START,
            struct.pack("<IBB", 0x12345678, 0, 0),
        )
        send_game(
            first,
            BOT_PLAYER_ID,
            bot_session,
            BATTLE_COMMAND,
            struct.pack("<HBB", 0, 1, 0),
            transaction=0,
        )
        _, _, command = expect_game(first, BATTLE_COMMAND)
        turn, kind, value = struct.unpack("<HBB", command)
        if turn != 0 or kind != 1 or (value & 0x07) >= 4:
            raise AssertionError("bot did not answer the first battle turn")

        # Feed an authoritative host snapshot in which the bot's lead fainted.
        # It must ACK the snapshot and choose a living replacement exactly once.
        host = bytearray(bot_team[0])
        struct.pack_into("<I", host, 0, 0x71001001)
        struct.pack_into("<H", host, 16, 100)
        for side, slot, pokemon in (
            (0, 0, bytes(host)),
            (1, 0, pokemon_with_hp(bot_team[0], 0)),
            (1, 1, pokemon_at_full_hp(bot_team[1])),
            (1, 2, pokemon_at_full_hp(bot_team[2])),
        ):
            send_game(
                first,
                BOT_PLAYER_ID,
                bot_session,
                BATTLE_SYNC_POKEMON,
                struct.pack("<HBB", 1, side, slot) + pokemon,
                transaction=1,
            )
        core = bytearray(32)
        struct.pack_into("<HI", core, 0, 1, 0xA55A1234)
        core[10] = 1
        core[16:20] = bytes((1, 3, 0, 0))
        send_game(
            first,
            BOT_PLAYER_ID,
            bot_session,
            BATTLE_SYNC_CORE,
            bytes(core),
            transaction=1,
        )
        _, _, acknowledgement = expect_game(first, BATTLE_SYNC_ACK)
        if acknowledgement != struct.pack("<HI", 1, 0xA55A1234):
            raise AssertionError("bot acknowledged the wrong authoritative snapshot")
        _, _, forced = expect_game(first, BATTLE_COMMAND)
        forced_turn, forced_kind, replacement = struct.unpack("<HBB", forced)
        if (forced_turn, forced_kind, replacement) not in ((1, 2, 1), (1, 2, 2)):
            raise AssertionError("bot did not choose a living forced replacement")

        # Forced replacement does not increment BattleState::turn. Confirm the
        # switched active slot with a different same-turn checksum, then send
        # the next ordinary move on turn 1. A turn-only duplicate guard used
        # to discard this legal command and freeze the battle after the bot's
        # first faint.
        for side, slot, pokemon in (
            (0, 0, bytes(host)),
            (1, 0, pokemon_with_hp(bot_team[0], 0)),
            (1, 1, pokemon_at_full_hp(bot_team[1])),
            (1, 2, pokemon_at_full_hp(bot_team[2])),
        ):
            send_game(
                first,
                BOT_PLAYER_ID,
                bot_session,
                BATTLE_SYNC_POKEMON,
                struct.pack("<HBB", 1, side, slot) + pokemon,
                transaction=1,
            )
        switched_core = bytearray(core)
        struct.pack_into("<HI", switched_core, 0, 1, 0xA55A5678)
        switched_core[16:20] = bytes((1, 3, 0, replacement))
        send_game(
            first,
            BOT_PLAYER_ID,
            bot_session,
            BATTLE_SYNC_CORE,
            bytes(switched_core),
            transaction=1,
        )
        _, _, switched_ack = expect_game(first, BATTLE_SYNC_ACK)
        if switched_ack != struct.pack("<HI", 1, 0xA55A5678):
            raise AssertionError("bot did not acknowledge its switched snapshot")
        send_game(
            first,
            BOT_PLAYER_ID,
            bot_session,
            BATTLE_COMMAND,
            struct.pack("<HBB", 1, 1, 0),
            transaction=1,
        )
        _, _, resumed = expect_game(first, BATTLE_COMMAND)
        resumed_turn, resumed_kind, resumed_value = struct.unpack("<HBB", resumed)
        resumed_move = resumed_kind == 1 and (resumed_value & 0x07) < 4
        resumed_revive = (
            resumed_kind == 5 and (resumed_value & 0x1F) == 15 and
            ((resumed_value >> 5) & 0x03) == 0
        )
        if resumed_turn != 1 or not (resumed_move or resumed_revive):
            raise AssertionError("bot did not resume on the same turn after switching")

        # Exercise the complete ITEM path on the physical relay. Three
        # authoritative critical-HP snapshots must spend the standardized
        # three Hyper Potions on the active roster slot. A fourth identical
        # decision must fall back to MOVE, proving the server inventory is
        # finite and decremented only after a transmitted command.
        for item_turn in range(2, 6):
            for slot, pokemon in enumerate(bot_team):
                hp = 1 if slot == replacement else 100
                send_game(
                    first,
                    BOT_PLAYER_ID,
                    bot_session,
                    BATTLE_SYNC_POKEMON,
                    struct.pack("<HBB", item_turn, 1, slot) +
                    pokemon_with_hp(pokemon, hp),
                    transaction=item_turn,
                )
            send_game(
                first,
                BOT_PLAYER_ID,
                bot_session,
                BATTLE_SYNC_POKEMON,
                struct.pack("<HBB", item_turn, 0, 0) + bytes(host),
                transaction=item_turn,
            )
            item_core = bytearray(switched_core)
            checksum = 0xB4000000 | item_turn
            struct.pack_into("<HI", item_core, 0, item_turn, checksum)
            item_core[16:20] = bytes((1, 3, 0, replacement))
            send_game(
                first,
                BOT_PLAYER_ID,
                bot_session,
                BATTLE_SYNC_CORE,
                bytes(item_core),
                transaction=item_turn,
            )
            _, _, item_ack = expect_game(first, BATTLE_SYNC_ACK)
            if item_ack != struct.pack("<HI", item_turn, checksum):
                raise AssertionError("bot acknowledged the wrong item snapshot")
            send_game(
                first,
                BOT_PLAYER_ID,
                bot_session,
                BATTLE_COMMAND,
                struct.pack("<HBB", item_turn, 1, 0),
                transaction=item_turn,
            )
            _, _, item_command = expect_game(first, BATTLE_COMMAND)
            command_turn, command_kind, command_value = struct.unpack(
                "<HBB", item_command
            )
            if command_turn != item_turn:
                raise AssertionError("bot answered the wrong item turn")
            if item_turn < 5:
                item = command_value & 0x1F
                target = (command_value >> 5) & 0x03
                if command_kind != 5 or item != 2 or target != replacement:
                    raise AssertionError(
                        "bot did not use Hyper Potion on its critical active slot"
                    )
            elif command_kind != 1 or (command_value & 0x07) >= 4:
                raise AssertionError("bot reused an exhausted Hyper Potion stock")

        first.send(9, target=BOT_PLAYER_ID, session=bot_session)
        source, _, closed_session, _ = expect(first, 10)
        if source != BOT_PLAYER_ID or closed_session != bot_session:
            raise AssertionError("bot session did not close cleanly")
        returned = peer_list(recovered)
        if BOT_PLAYER_ID not in returned:
            raise AssertionError("bot did not return to discovery after the battle")
        if returned[BOT_PLAYER_ID] != initial_bot_wins:
            raise AssertionError("unfinished battle changed the bot's Balanced wins")
    finally:
        first.close()
        try:
            second.close()
        except OSError:
            pass
        if recovered is not None:
            recovered.close()
        legacy.close()

    print(
        "PASS: discovery hostname, authentication, peer list, session, "
        "byte-exact PGCH relay, stale-close defense, abrupt-drop cleanup, "
        "idempotent close, clean reconnect, singleton bot presence, trade "
        "refusal, battle acceptance, competitive trio, Balanced win counters "
        "forced switch, same-turn resume, finite contextual Bag and simultaneous "
        "PGON v1/v2 compatibility"
    )


if __name__ == "__main__":
    main()
