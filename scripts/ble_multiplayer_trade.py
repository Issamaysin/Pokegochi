#!/usr/bin/env python3
"""Act as a Pokegochi multiplayer peer and deliver Pokemon over BLE.

The Android app remains authoritative for its own save. This tool performs the
same invitation, trade confirmation and two-phase commit used by a real device;
it reports success only after the app acknowledges that its received Pokemon
was committed durably.
"""

from __future__ import annotations

import argparse
import asyncio
import struct
import time
from dataclasses import dataclass

from bleak import BleakClient, BleakScanner


PROTOCOL_VERSION = 10
PACKET_MAGIC = 0x4750
MANUFACTURER_ID = 0x4750
LEGACY_MANUFACTURER_ID = 0xFFFF
SERVICE_UUID = "70564743-4849-4d50-8000-504f4b45474f"
RX_UUID = "70564743-4849-4d50-8001-504f4b45474f"
TX_UUID = "70564743-4849-4d50-8002-504f4b45474f"

INVITE = 1
INVITE_ACCEPT = 2
INVITE_REJECT = 3
SESSION_CLOSE = 4
ACTIVITY_REQUEST = 5
ACTIVITY_ACCEPT = 6
ACTIVITY_REJECT = 7
TRADE_OFFER = 8
TRADE_CONFIRM = 9
TRADE_REJECT = 10
TRADE_COMMIT_REQUEST = 11
TRADE_COMMIT_APPLIED = 12
ACTIVITY_TRADE = 2

HEADER = struct.Struct("<HBBIIH")
OWNED_POKEMON_BYTES = 64


@dataclass(frozen=True)
class Advertisement:
    player_id: int


@dataclass(frozen=True)
class Packet:
    kind: int
    source_id: int
    transaction: int
    payload: bytes


def parse_advertisement(advertisement) -> Advertisement | None:
    for company_id, value in advertisement.manufacturer_data.items():
        payload = bytes(value)
        if company_id == MANUFACTURER_ID:
            raw = struct.pack("<H", company_id) + payload
        elif company_id == LEGACY_MANUFACTURER_ID:
            raw = payload
            marker = raw.find(b"PGCH")
            if marker >= 0:
                raw = raw[marker:]
        else:
            continue
        if len(raw) != 9 or raw[:4] != b"PGCH" or raw[4] != PROTOCOL_VERSION:
            continue
        player_id = struct.unpack_from("<I", raw, 5)[0]
        if player_id:
            return Advertisement(player_id)
    return None


def make_pokemon(species_id: int, uid: int, personality: int) -> bytes:
    """Build the current 64-byte OwnedPokemon ABI with a canonical Lv.70 donor."""
    if species_id not in (791, 792):
        raise ValueError("this delivery tool currently supports Solgaleo and Lunala")
    record = bytearray(OWNED_POKEMON_BYTES)
    level = 70
    experience = 5 * level * level * level // 4  # Both use Slow growth.
    maximum_hp = ((2 * 137 + 31) * level // 100) + level + 10
    ability_id = 231 if species_id == 791 else 232
    struct.pack_into("<I", record, 0, uid)
    struct.pack_into("<I", record, 4, personality)
    struct.pack_into("<H", record, 8, species_id)
    struct.pack_into("<I", record, 12, experience)
    struct.pack_into("<H", record, 16, maximum_hp)
    struct.pack_into("<H", record, 18, maximum_hp)
    record[20] = level
    record[23] = 70  # FireRed friendship default.
    # Empty move slots are intentional: tradeReplace() reconstructs the latest
    # legal four-move level-up kit and its PP from the receiver's generation.
    record[48] = ability_id
    record[49:55] = bytes([31] * 6)
    record[55] = personality % 25
    # heldItem=NONE, shiny=false and all EVs=0 are already zero-initialized.
    return bytes(record)


def parse_packet(raw: bytes) -> Packet | None:
    if len(raw) < HEADER.size:
        return None
    magic, version, kind, source_id, transaction, payload_size = HEADER.unpack_from(raw)
    if (magic != PACKET_MAGIC or version != PROTOCOL_VERSION or
            len(raw) != HEADER.size + payload_size):
        return None
    return Packet(kind, source_id, transaction, raw[HEADER.size:])


def pokemon_identity(payload: bytes) -> tuple[int, int, int]:
    if len(payload) != OWNED_POKEMON_BYTES:
        raise ValueError(f"wrong trade payload size: {len(payload)}")
    uid = struct.unpack_from("<I", payload, 0)[0]
    personality = struct.unpack_from("<I", payload, 4)[0]
    species_id = struct.unpack_from("<H", payload, 8)[0]
    if not uid or not species_id:
        raise ValueError("peer offered an empty Pokemon")
    return uid, personality, species_id


def trade_transaction(first_player: int, second_player: int,
                      first_pokemon: bytes, second_pokemon: bytes) -> int:
    first = pokemon_identity(first_pokemon)
    second = pokemon_identity(second_pokemon)
    low, high = (first, second) if first_player <= second_player else (second, first)
    value = (first_player ^ second_player ^ low[1] ^ high[1]) & 0xFFFFFFFF
    value ^= (low[2] << 16) & 0xFFFFFFFF
    value ^= high[2]
    value ^= value >> 16
    value = (value * 0x7FEB352D) & 0xFFFFFFFF
    value ^= value >> 15
    return value or 1


class DesktopPeer:
    def __init__(self, client: BleakClient, player_id: int, remote_player_id: int):
        self.client = client
        self.player_id = player_id
        self.remote_player_id = remote_player_id
        self.packets: asyncio.Queue[Packet] = asyncio.Queue()

    def on_notification(self, _sender, raw: bytearray) -> None:
        packet = parse_packet(bytes(raw))
        if packet is not None and packet.source_id == self.remote_player_id:
            print(f"RECV type={packet.kind} tx={packet.transaction:08X} "
                  f"bytes={len(packet.payload)}", flush=True)
            self.packets.put_nowait(packet)

    async def send(self, kind: int, payload: bytes = b"", transaction: int = 0) -> None:
        packet = HEADER.pack(PACKET_MAGIC, PROTOCOL_VERSION, kind, self.player_id,
                             transaction, len(payload)) + payload
        await self.client.write_gatt_char(RX_UUID, packet, response=True)
        print(f"SEND type={kind} tx={transaction:08X} bytes={len(payload)}", flush=True)

    async def wait_for(self, expected: set[int], timeout: float = 240.0) -> Packet:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError(f"timeout waiting for packet types {sorted(expected)}")
            packet = await asyncio.wait_for(self.packets.get(), remaining)
            if packet.kind == INVITE_REJECT:
                raise RuntimeError("the app rejected the connection")
            if packet.kind == ACTIVITY_REJECT:
                raise RuntimeError("the app rejected the trade request")
            if packet.kind == TRADE_REJECT:
                raise RuntimeError("the app cancelled or could not save the trade")
            if packet.kind == SESSION_CLOSE:
                raise RuntimeError("the app ended the multiplayer session")
            if packet.kind in expected:
                return packet

    async def invite(self) -> None:
        token = (self.player_id ^ (self.remote_player_id << 1) ^ 0x50474F43) & 0xFFFFFFFF
        print("ACTION: accept the incoming player connection in the app.", flush=True)
        await self.send(INVITE, transaction=token)
        accepted = await self.wait_for({INVITE_ACCEPT})
        if accepted.transaction != token:
            raise RuntimeError("the app accepted a different invitation token")
        print("CONNECTED: logical multiplayer session accepted.", flush=True)

    async def trade(self, species_id: int, name: str, ordinal: int) -> None:
        print(f"ACTION: accept trade {ordinal}/2 in the app, then choose any transferable Pokemon.",
              flush=True)
        await self.send(ACTIVITY_REQUEST, bytes([ACTIVITY_TRADE]))
        accepted = await self.wait_for({ACTIVITY_ACCEPT})
        if accepted.payload != bytes([ACTIVITY_TRADE]):
            raise RuntimeError("the app accepted an unexpected activity")

        fake_uid = 0x70000000 | ordinal
        personality = 0x79000000 | (species_id << 4) | ordinal
        offered = make_pokemon(species_id, fake_uid, personality)
        await self.send(TRADE_OFFER, offered)
        print(f"OFFERED: {name} Lv.70. Select the Pokemon you will send.", flush=True)

        remote_offer_packet = await self.wait_for({TRADE_OFFER})
        remote_uid, _remote_personality, remote_species = pokemon_identity(
            remote_offer_packet.payload)
        transaction = trade_transaction(self.player_id, self.remote_player_id,
                                        offered, remote_offer_packet.payload)
        print(f"REMOTE OFFER: species={remote_species} uid={remote_uid}; "
              f"transaction={transaction:08X}", flush=True)

        # Confirm first so the app only needs the user's explicit confirmation.
        await self.send(TRADE_CONFIRM, transaction=transaction)
        print(f"ACTION: confirm the {name} trade in the app.", flush=True)
        remote_confirmation = await self.wait_for({TRADE_CONFIRM})
        if remote_confirmation.transaction != transaction:
            raise RuntimeError("the app confirmed a different trade transaction")

        if self.player_id >= self.remote_player_id:
            raise RuntimeError("desktop peer must have the lower player id for deterministic commit")
        await self.send(TRADE_COMMIT_REQUEST, transaction=transaction)
        committed = await self.wait_for({TRADE_COMMIT_APPLIED})
        if committed.transaction != transaction:
            raise RuntimeError("the app acknowledged a different trade transaction")
        # This final acknowledgement completes the coordinator's half. The app
        # may already have returned to Connected, exactly like a real peer.
        await self.send(TRADE_COMMIT_APPLIED, transaction=transaction)
        print(f"COMMITTED: app durably accepted {name}.", flush=True)


async def discover(seconds: float, wanted_player: int | None):
    found = await BleakScanner.discover(timeout=seconds, return_adv=True)
    candidates = []
    for device, advertisement in found.values():
        parsed = parse_advertisement(advertisement)
        if parsed is None or (wanted_player is not None and parsed.player_id != wanted_player):
            continue
        candidates.append((advertisement.rssi, device, parsed.player_id))
        print(f"FOUND address={device.address} player={parsed.player_id} "
              f"rssi={advertisement.rssi}", flush=True)
    return sorted(candidates, key=lambda entry: entry[0], reverse=True)


async def run(args) -> int:
    candidates = await discover(args.scan_seconds, args.remote_player)
    if not candidates:
        raise RuntimeError("no Pokegochi multiplayer advertisement found")

    last_error: Exception | None = None
    for _rssi, device, remote_player_id in candidates:
        if args.player_id >= remote_player_id:
            raise RuntimeError("--player-id must be lower than the app player id")
        completed = False
        try:
            async with BleakClient(device, timeout=30.0) as client:
                if not client.is_connected:
                    raise RuntimeError("BLE connection did not become active")
                service = client.services.get_service(SERVICE_UUID)
                if service is None:
                    raise RuntimeError("multiplayer GATT service is missing")
                print(f"GATT CONNECTED address={device.address} remote={remote_player_id} "
                      f"mtu={getattr(client, 'mtu_size', 0)}", flush=True)
                peer = DesktopPeer(client, args.player_id, remote_player_id)
                await client.start_notify(TX_UUID, peer.on_notification)
                try:
                    await peer.invite()
                    await peer.trade(791, "SOLGALEO", 1)
                    await asyncio.sleep(4.0)
                    await peer.trade(792, "LUNALA", 2)
                    completed = True
                    # Android may retire the physical link as soon as the
                    # second durable commit returns to Connected. SessionClose
                    # is therefore cleanup only, never part of trade success.
                    try:
                        await asyncio.sleep(1.0)
                        if client.is_connected:
                            await peer.send(SESSION_CLOSE)
                    except Exception as cleanup_error:
                        print(f"CLEANUP: link already closed ({cleanup_error}).", flush=True)
                    print("DONE: both BLE trades were committed by the app.", flush=True)
                    return 0
                finally:
                    try:
                        await client.stop_notify(TX_UUID)
                    except Exception:
                        pass
        except Exception as error:
            if completed:
                print(f"CLEANUP: post-commit disconnect reported {error}", flush=True)
                print("DONE: both BLE trades were committed by the app.", flush=True)
                return 0
            last_error = error
            print(f"CONNECT ATTEMPT FAILED address={device.address}: {error}", flush=True)
    raise RuntimeError(f"all matching BLE peers failed: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--remote-player", type=int, help="expected app Player ID")
    parser.add_argument("--player-id", type=int, default=1,
                        help="simulated peer Player ID (default: 1)")
    parser.add_argument("--scan-seconds", type=float, default=10.0)
    args = parser.parse_args()
    return asyncio.run(run(args))


if __name__ == "__main__":
    raise SystemExit(main())
