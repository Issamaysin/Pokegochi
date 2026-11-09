#!/usr/bin/env python3
"""Run a local PGON relay advertised exactly like the ESP32 server.

This is a hardware diagnostic relay.  It deliberately implements only the
transport envelope: all invite, activity, trade and battle authority remains
inside the ordinary PGCH clients, exactly as it does through the real server.
"""

from __future__ import annotations

import argparse
import asyncio
import secrets
import socket
import struct
from dataclasses import dataclass

from zeroconf import ServiceInfo, Zeroconf


FRAME = struct.Struct("<IBBHIII")
HELLO_V1 = struct.Struct("<B3xI")
HELLO_V2 = struct.Struct("<B3xII")
PEER_ENTRY = struct.Struct("<I")

MAGIC = 0x4E4F4750
LEGACY_VERSION = 1
VERSION = 2
GAME_PROTOCOL_VERSION = 10
MAXIMUM_PAYLOAD = 174

HELLO = 1
WELCOME = 2
REFRESH_PEERS = 3
PEER_ENTRY_MESSAGE = 4
PEER_LIST_COMPLETE = 5
OPEN_SESSION = 6
SESSION_READY = 7
SESSION_BUSY = 8
CLOSE_SESSION = 9
SESSION_CLOSED = 10
RELAY = 11
PING = 12
PONG = 13
ERROR = 14

BAD_FRAME = 1
WRONG_VERSION = 2
ACCESS_DENIED = 3
DUPLICATE_PLAYER = 4
PEER_UNAVAILABLE = 6
SESSION_MISMATCH = 7


@dataclass(eq=False)
class Client:
    reader: asyncio.StreamReader
    writer: asyncio.StreamWriter
    version: int = 0
    player_id: int = 0
    balanced_wins: int = 0
    peer_id: int = 0
    session_id: int = 0

    @property
    def authenticated(self) -> bool:
        return self.player_id != 0


class Relay:
    def __init__(self, access_code: int) -> None:
        self.access_code = access_code
        self.clients: dict[int, Client] = {}

    async def send(self, client: Client, kind: int, *, source: int = 0,
                   target: int | None = None, session: int = 0,
                   payload: bytes = b"") -> None:
        version = client.version or VERSION
        destination = client.player_id if target is None else target
        client.writer.write(FRAME.pack(MAGIC, version, kind, len(payload),
                                       source, destination, session) + payload)
        await client.writer.drain()

    async def error(self, client: Client, code: int, source: int = 0) -> None:
        await self.send(client, ERROR, source=source,
                        session=client.session_id, payload=bytes((code,)))

    async def clear_session(self, client: Client, notify_peer: bool) -> None:
        peer_id = client.peer_id
        session_id = client.session_id
        client.peer_id = 0
        client.session_id = 0
        peer = self.clients.get(peer_id)
        if (not peer or peer.peer_id != client.player_id or
                peer.session_id != session_id):
            return
        peer.peer_id = 0
        peer.session_id = 0
        if notify_peer:
            await self.send(peer, SESSION_CLOSED, source=client.player_id,
                            session=session_id)

    async def peer_list(self, client: Client) -> None:
        delivered = 0
        for candidate in tuple(self.clients.values()):
            if (candidate is client or candidate.session_id or
                    not candidate.authenticated):
                continue
            payload = (b"" if client.version == LEGACY_VERSION else
                       PEER_ENTRY.pack(candidate.balanced_wins))
            await self.send(client, PEER_ENTRY_MESSAGE,
                            source=candidate.player_id, payload=payload)
            delivered += 1
        await self.send(client, PEER_LIST_COMPLETE)
        print(f"[RELAY] peer-list player={client.player_id:05d} "
              f"entries={delivered}", flush=True)

    async def hello(self, client: Client, version: int, source: int,
                    payload: bytes) -> None:
        expected = HELLO_V1 if version == LEGACY_VERSION else HELLO_V2
        if (client.authenticated or version not in (LEGACY_VERSION, VERSION) or
                len(payload) != expected.size or source < 10000 or
                source > 99999):
            await self.error(client, BAD_FRAME)
            return
        values = expected.unpack(payload)
        game_version, access_code = values[:2]
        balanced_wins = values[2] if version == VERSION else 0
        if game_version != GAME_PROTOCOL_VERSION:
            await self.error(client, WRONG_VERSION)
            return
        if access_code != self.access_code:
            await self.error(client, ACCESS_DENIED)
            return
        if source in self.clients:
            await self.error(client, DUPLICATE_PLAYER)
            return
        client.version = version
        client.player_id = source
        client.balanced_wins = balanced_wins
        self.clients[source] = client
        await self.send(client, WELCOME)
        print(f"[RELAY] online player={source:05d} pgon=v{version}", flush=True)

    async def open_session(self, client: Client, target: int) -> None:
        peer = self.clients.get(target)
        if (client.session_id or not peer or peer.session_id or
                peer is client):
            await self.send(client, SESSION_BUSY, source=target)
            return
        session = secrets.randbits(32) or 1
        client.peer_id = peer.player_id
        client.session_id = session
        peer.peer_id = client.player_id
        peer.session_id = session
        try:
            await self.send(client, SESSION_READY, source=peer.player_id,
                            session=session, payload=b"\x01")
            await self.send(peer, SESSION_READY, source=client.player_id,
                            session=session, payload=b"\x00")
        except (ConnectionError, OSError):
            await self.clear_session(client, True)
            raise
        print(f"[RELAY] session={session:08X} "
              f"players={client.player_id:05d}/{peer.player_id:05d}",
              flush=True)

    async def process(self, client: Client, version: int, kind: int,
                      source: int, target: int, session: int,
                      payload: bytes) -> None:
        if not client.authenticated:
            if kind != HELLO:
                await self.error(client, ACCESS_DENIED)
                return
            await self.hello(client, version, source, payload)
            return
        if version != client.version:
            await self.error(client, WRONG_VERSION)
            return
        if source != client.player_id:
            await self.error(client, BAD_FRAME)
            return
        if kind == REFRESH_PEERS:
            await self.peer_list(client)
        elif kind == OPEN_SESSION:
            await self.open_session(client, target)
        elif kind == CLOSE_SESSION:
            if not client.session_id:
                await self.send(client, SESSION_CLOSED, source=target,
                                session=session)
            elif session == client.session_id and target == client.peer_id:
                old_peer = client.peer_id
                old_session = client.session_id
                await self.clear_session(client, True)
                await self.send(client, SESSION_CLOSED, source=old_peer,
                                session=old_session)
            else:
                await self.error(client, SESSION_MISMATCH, target)
        elif kind == RELAY:
            peer = self.clients.get(client.peer_id)
            if (not client.session_id or session != client.session_id or
                    target != client.peer_id or not peer or
                    peer.session_id != session or peer.peer_id != client.player_id):
                await self.error(client, SESSION_MISMATCH, target)
                return
            await self.send(peer, RELAY, source=client.player_id,
                            session=session, payload=payload)
        elif kind == PING:
            await self.send(client, PONG)
        elif kind != PONG:
            await self.error(client, BAD_FRAME)

    async def connected(self, reader: asyncio.StreamReader,
                        writer: asyncio.StreamWriter) -> None:
        client = Client(reader, writer)
        address = writer.get_extra_info("peername")
        try:
            while True:
                raw = await reader.readexactly(FRAME.size)
                magic, version, kind, length, source, target, session = FRAME.unpack(raw)
                if (magic != MAGIC or version not in (LEGACY_VERSION, VERSION) or
                        kind < HELLO or kind > ERROR or length > MAXIMUM_PAYLOAD):
                    break
                payload = await reader.readexactly(length)
                await self.process(client, version, kind, source, target,
                                   session, payload)
        except (asyncio.IncompleteReadError, ConnectionError, OSError):
            pass
        finally:
            if client.player_id:
                self.clients.pop(client.player_id, None)
                await self.clear_session(client, True)
                print(f"[RELAY] offline player={client.player_id:05d}", flush=True)
            writer.close()
            try:
                await writer.wait_closed()
            except OSError:
                pass
            if not client.player_id:
                print(f"[RELAY] unauthenticated socket closed {address}", flush=True)


def local_ip() -> str:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("192.0.2.1", 9))
        return probe.getsockname()[0]
    finally:
        probe.close()


async def main_async(arguments: argparse.Namespace) -> None:
    address = arguments.advertise or local_ip()
    relay = Relay(arguments.access_code)
    server = await asyncio.start_server(relay.connected, arguments.bind,
                                        arguments.port)
    zeroconf = Zeroconf()
    info = ServiceInfo(
        "_pokegochi._tcp.local.",
        "Pokegochi Local Diagnostics._pokegochi._tcp.local.",
        addresses=[socket.inet_aton(address)],
        port=arguments.port,
        properties={"code": f"{arguments.access_code:06d}",
                    "game-protocol": str(GAME_PROTOCOL_VERSION)},
        server="pokegochi-server.local.",
    )
    # Zeroconf owns a helper loop. Its synchronous facade must not be invoked
    # from this relay's asyncio loop or recent releases correctly flag that as
    # an event-loop deadlock.
    await asyncio.to_thread(zeroconf.register_service, info)
    print(f"[RELAY] listening {arguments.bind}:{arguments.port}; "
          f"advertising {address} code={arguments.access_code:06d}", flush=True)
    try:
        async with server:
            await server.serve_forever()
    finally:
        await asyncio.to_thread(zeroconf.unregister_service, info)
        await asyncio.to_thread(zeroconf.close)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", default="0.0.0.0")
    parser.add_argument("--advertise")
    parser.add_argument("--port", type=int, default=32145)
    parser.add_argument("--access-code", type=int, default=579856)
    arguments = parser.parse_args()
    try:
        asyncio.run(main_async(arguments))
    except KeyboardInterrupt:
        print("[RELAY] stopped", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
