#!/usr/bin/env python3
"""Run a deterministic Pokegochi v9 battle peer through the ESP32 relay.

This is a hardware regression peer, not a shortcut around the game protocol.
The physical Pokegochi discovers it as an ordinary online player, negotiates
the invitation/rules/party/lead packets and remains the authoritative host.
The synthetic roster opens with Moongeist Beam (which also covers a Mimikyu
lead), then uses Explosion so local, remote and simultaneous forced-switch
barriers all exercise the no-PSRAM renderer.
"""

from __future__ import annotations

import argparse
import socket
import struct
import time
from dataclasses import dataclass


ONLINE_HEADER = struct.Struct("<IBBHIII")
HELLO = struct.Struct("<B3xI")
GAME_HEADER = struct.Struct("<HBBIIH")
COMMAND = struct.Struct("<HBB")

ONLINE_MAGIC = 0x4E4F4750
ONLINE_VERSION = 1
GAME_MAGIC = 0x4750
GAME_VERSION = 10

WELCOME = 2
REFRESH_PEERS = 3
PEER_ENTRY = 4
PEER_LIST_COMPLETE = 5
OPEN_SESSION = 6
SESSION_READY = 7
CLOSE_SESSION = 9
SESSION_CLOSED = 10
RELAY = 11
PING = 12
PONG = 13
ERROR = 14

INVITE = 1
INVITE_ACCEPT = 2
SESSION_CLOSE = 4
ACTIVITY_REQUEST = 5
ACTIVITY_ACCEPT = 6
BATTLE_PARTY_MEMBER = 13
BATTLE_PARTY_READY = 14
BATTLE_LEAD = 15
BATTLE_START = 16
ACTIVITY_CANCEL = 17
BATTLE_COMMAND = 18
BATTLE_SYNC_POKEMON = 19
BATTLE_SYNC_CORE = 21
BATTLE_SYNC_ACK = 22

ACTIVITY_BATTLE = 1
RULES_NORMAL = 1
COMMAND_MOVE = 1
COMMAND_SWITCH = 2
COMMAND_WAIT = 4

OWNED_POKEMON_BYTES = 64
MOONGEIST_BEAM = 668
EXPLOSION = 153
TACKLE = 33
STRUGGLE = 165


@dataclass(frozen=True)
class GamePacket:
    kind: int
    source: int
    transaction: int
    payload: bytes


def make_test_pokemon(slot: int, primary_move: int = MOONGEIST_BEAM) -> bytes:
    """Create a legal-size disposable Deoxys-Speed battle snapshot."""
    record = bytearray(OWNED_POKEMON_BYTES)
    uid = 0x7A000001 + slot
    personality = 0x5EED0001 + slot
    struct.pack_into("<I", record, 0, uid)
    struct.pack_into("<I", record, 4, personality)
    struct.pack_into("<H", record, 8, 386)  # Deoxys
    struct.pack_into("<I", record, 12, 1_250_000)
    # The multiplayer roster is already a battle-local snapshot. Keeping a
    # deliberately large HP pool keeps the remote lead alive for the second
    # forced-switch scenario even when the board's selected move resolves.
    struct.pack_into("<H", record, 16, 999)
    struct.pack_into("<H", record, 18, 999)
    record[20] = 100
    record[22] = 3  # Speed Forme, so Final Gambit normally acts first.
    record[23] = 70
    struct.pack_into("<4H", record, 36,
                     primary_move, EXPLOSION, TACKLE, STRUGGLE)
    record[44:48] = bytes((5, 5, 35, 1))
    record[48] = 46  # Pressure
    record[49:55] = bytes((31,) * 6)
    record[55] = 10  # Timid
    # Keep EV total legal while maximizing speed.
    record[58:64] = bytes((252, 0, 0, 0, 4, 254))
    if len(record) != OWNED_POKEMON_BYTES:
        raise AssertionError("OwnedPokemon ABI changed")
    return bytes(record)


def pack_game(kind: int, source: int, payload: bytes = b"",
              transaction: int = 0) -> bytes:
    return GAME_HEADER.pack(GAME_MAGIC, GAME_VERSION, kind, source,
                            transaction, len(payload)) + payload


def parse_game(raw: bytes) -> GamePacket:
    if len(raw) < GAME_HEADER.size:
        raise ValueError("short PGCH packet")
    magic, version, kind, source, transaction, length = GAME_HEADER.unpack_from(raw)
    if magic != GAME_MAGIC or version != GAME_VERSION or len(raw) != GAME_HEADER.size + length:
        raise ValueError("invalid PGCH packet")
    return GamePacket(kind, source, transaction, raw[GAME_HEADER.size:])


class RelayPeer:
    def __init__(self, host: str, port: int, access_code: int,
                 player_id: int, primary_move: int) -> None:
        self.player_id = player_id
        self.socket = socket.create_connection((host, port), timeout=8.0)
        self.socket.settimeout(1.0)
        self.remote_id = 0
        self.session = 0
        self.roster_sent = False
        self.lead_sent = False
        self.last_move_response_turn = -1
        self.last_forced_turn = -1
        self.combat_commands_sent = 0
        self.host_hp = [0, 0, 0]
        self.guest_hp = [999, 999, 999]
        self.activities_completed = 0
        self.primary_move = primary_move
        self.send_online(1, payload=HELLO.pack(GAME_VERSION, access_code))
        kind, *_ = self.receive_online(8.0)
        if kind != WELCOME:
            raise RuntimeError(f"relay rejected HELLO with PGON type {kind}")
        print(f"[SIM] online player {player_id:05d}; waiting for the board", flush=True)
        self.send_online(REFRESH_PEERS)

    def close(self) -> None:
        try:
            if self.session and self.remote_id:
                self.send_online(CLOSE_SESSION, target=self.remote_id,
                                 session=self.session)
        except OSError:
            pass
        self.socket.close()

    def _exact(self, length: int, deadline: float) -> bytes:
        data = bytearray()
        while len(data) < length:
            if time.monotonic() >= deadline:
                raise TimeoutError("relay frame timed out")
            try:
                block = self.socket.recv(length - len(data))
            except socket.timeout:
                continue
            if not block:
                raise ConnectionError("relay closed the connection")
            data.extend(block)
        return bytes(data)

    def send_online(self, kind: int, *, target: int = 0, session: int = 0,
                    payload: bytes = b"") -> None:
        frame = ONLINE_HEADER.pack(ONLINE_MAGIC, ONLINE_VERSION, kind,
                                   len(payload), self.player_id, target,
                                   session) + payload
        self.socket.sendall(frame)

    def receive_online(self, timeout: float = 1.0) -> tuple[int, int, int, int, bytes]:
        deadline = time.monotonic() + timeout
        header = self._exact(ONLINE_HEADER.size, deadline)
        magic, version, kind, length, source, target, session = ONLINE_HEADER.unpack(header)
        if magic != ONLINE_MAGIC or version != ONLINE_VERSION:
            raise ValueError("invalid PGON header")
        payload = self._exact(length, deadline)
        return kind, source, target, session, payload

    def send_game(self, kind: int, payload: bytes = b"",
                  transaction: int = 0) -> None:
        if not self.remote_id or not self.session:
            raise RuntimeError("PGCH session is not ready")
        game = pack_game(kind, self.player_id, payload, transaction)
        self.send_online(RELAY, target=self.remote_id, session=self.session,
                         payload=game)
        print(f"[SIM] SEND PGCH type={kind} tx={transaction:04X} bytes={len(payload)}",
              flush=True)

    def send_roster(self) -> None:
        if self.roster_sent:
            return
        for slot in range(3):
            self.send_game(BATTLE_PARTY_MEMBER,
                           bytes((slot,)) +
                           make_test_pokemon(slot, self.primary_move))
        self.send_game(BATTLE_PARTY_READY, b"\x03")
        self.roster_sent = True
        print("[SIM] three-member faint/switch roster is READY", flush=True)

    def send_lead(self) -> None:
        if self.lead_sent:
            return
        self.send_game(BATTLE_LEAD, b"\x00")
        self.lead_sent = True

    def reset_activity(self) -> None:
        """Release battle-local state while preserving the PGON room."""
        self.roster_sent = False
        self.lead_sent = False
        self.last_move_response_turn = -1
        self.last_forced_turn = -1
        self.combat_commands_sent = 0
        self.host_hp = [0, 0, 0]
        self.guest_hp = [999, 999, 999]

    def send_command(self, turn: int, kind: int, value: int = 0) -> None:
        self.send_game(BATTLE_COMMAND, COMMAND.pack(turn, kind, value), turn)

    def handle_game(self, packet: GamePacket) -> None:
        print(f"[SIM] RECV PGCH type={packet.kind} tx={packet.transaction:04X} "
              f"bytes={len(packet.payload)}", flush=True)
        if packet.kind == INVITE:
            self.send_game(INVITE_ACCEPT, transaction=packet.transaction)
            return
        if packet.kind == ACTIVITY_REQUEST:
            if packet.payload != bytes((ACTIVITY_BATTLE, RULES_NORMAL)):
                raise RuntimeError(f"unexpected activity proposal {packet.payload!r}")
            self.reset_activity()
            self.send_game(ACTIVITY_ACCEPT, packet.payload)
            self.send_roster()
            self.send_lead()
            return
        if packet.kind == ACTIVITY_CANCEL:
            self.activities_completed += 1
            self.reset_activity()
            print(f"[SIM] logical activity ended; room retained; "
                  f"completed={self.activities_completed}", flush=True)
            return
        if packet.kind == BATTLE_PARTY_READY:
            self.send_lead()
            return
        if packet.kind == BATTLE_COMMAND:
            turn, kind, value = COMMAND.unpack(packet.payload)
            print(f"[SIM] board command turn={turn} kind={kind} value={value}", flush=True)
            # Mimikyu's Disguise consumes the first hit on the connected beta
            # save. Repeat Moongeist Beam once so the second turn exercises
            # the board's own forced replacement, then use Explosion to cover
            # the remote replacement path as well.
            if kind == COMMAND_MOVE:
                if turn == self.last_move_response_turn:
                    return
                self.last_move_response_turn = turn
                move_slot = 0 if self.combat_commands_sent < 4 else 1
                self.combat_commands_sent += 1
                self.send_command(turn, COMMAND_MOVE, move_slot)
            return
        if packet.kind == BATTLE_SYNC_POKEMON:
            if len(packet.payload) != 4 + OWNED_POKEMON_BYTES:
                raise RuntimeError("BattleSyncPokemon ABI changed")
            turn, side, slot = struct.unpack_from("<HBB", packet.payload)
            hp = struct.unpack_from("<H", packet.payload, 4 + 16)[0]
            if slot < 3:
                (self.guest_hp if side else self.host_hp)[slot] = hp
            print(f"[SIM] sync turn={turn} side={side} slot={slot} hp={hp}", flush=True)
            return
        if packet.kind == BATTLE_SYNC_CORE:
            if len(packet.payload) < 22:
                raise RuntimeError("short BattleSyncCore")
            turn = struct.unpack_from("<H", packet.payload)[0]
            snapshot_checksum = struct.unpack_from("<I", packet.payload, 2)[0]
            host_count, guest_count, host_active, guest_active = struct.unpack_from(
                "<BBBB", packet.payload, len(packet.payload) - 16)
            print(f"[SIM] core turn={turn} host={host_active}/{host_count} "
                  f"hp={self.host_hp[host_active]} guest={guest_active}/{guest_count} "
                  f"hp={self.guest_hp[guest_active]}", flush=True)
            self.send_game(BATTLE_SYNC_ACK,
                           struct.pack("<HI", turn, snapshot_checksum), turn)
            host_fainted = self.host_hp[host_active] == 0
            guest_fainted = self.guest_hp[guest_active] == 0
            if (host_fainted or guest_fainted) and turn != self.last_forced_turn:
                self.last_forced_turn = turn
                if guest_fainted:
                    replacement = next((slot for slot in range(guest_count)
                                        if slot != guest_active and self.guest_hp[slot] > 0), None)
                    if replacement is not None:
                        self.send_command(turn, COMMAND_SWITCH, replacement)
                        print(f"[SIM] forced replacement locked: guest slot {replacement}",
                              flush=True)
                elif host_fainted:
                    self.send_command(turn, COMMAND_WAIT)
                    print("[SIM] waiting for the board's forced replacement", flush=True)
            return
        if packet.kind == SESSION_CLOSE:
            raise SystemExit("board ended the logical session")

    def run(self, timeout: float) -> None:
        deadline = time.monotonic() + timeout
        last_ping = time.monotonic()
        while time.monotonic() < deadline:
            if time.monotonic() - last_ping >= 10.0:
                self.send_online(PING, target=self.remote_id,
                                 session=self.session)
                last_ping = time.monotonic()
            try:
                kind, source, _target, session, payload = self.receive_online(1.0)
            except TimeoutError:
                continue
            if kind == PING:
                self.send_online(PONG, target=source, session=session)
            elif kind == SESSION_READY:
                self.remote_id = source
                self.session = session
                print(f"[SIM] session={session:08X} board={source:05d}", flush=True)
            elif kind == PEER_ENTRY:
                print(f"[SIM] relay peer available: {source:05d}", flush=True)
            elif kind == PEER_LIST_COMPLETE:
                print("[SIM] relay peer list complete", flush=True)
            elif kind == RELAY:
                if session != self.session or source != self.remote_id:
                    raise RuntimeError("relay delivered a packet for another session")
                self.handle_game(parse_game(payload))
            elif kind == SESSION_CLOSED:
                print("[SIM] relay session closed", flush=True)
                return
            elif kind == ERROR:
                raise RuntimeError(f"relay error payload={payload.hex()}")
        raise TimeoutError("hardware battle peer timed out")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="pokegochi-server.local")
    parser.add_argument("--port", type=int, default=32145)
    parser.add_argument("--access-code", type=int, default=579856)
    parser.add_argument("--player-id", type=int, default=60000)
    parser.add_argument("--timeout", type=float, default=900.0)
    parser.add_argument("--primary-move", type=int, default=MOONGEIST_BEAM,
                        help="move id used by slot zero in every test Pokemon")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        for slot in range(3):
            pokemon = make_test_pokemon(slot, args.primary_move)
            assert len(pokemon) == OWNED_POKEMON_BYTES
            assert struct.unpack_from("<4H", pokemon, 36)[0] == args.primary_move
        print("PASS: online battle simulator packet fixtures")
        return 0
    peer = RelayPeer(args.host, args.port, args.access_code, args.player_id,
                     args.primary_move)
    try:
        try:
            peer.run(args.timeout)
        except KeyboardInterrupt:
            print("[SIM] stopped by operator", flush=True)
    finally:
        peer.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
