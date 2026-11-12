#!/usr/bin/env python3
"""Stress the bot's first-room handshake and abandoned-room recovery."""

from __future__ import annotations

import argparse
import time

from test_online_server import (
    BOT_PLAYER_ID,
    GAME_VERSION,
    HELLO,
    INVITE,
    INVITE_ACCEPT,
    RelayClient,
    expect,
    expect_game,
    peer_list,
    send_game,
)


def authenticate(host: str, port: int, access_code: int,
                 player_id: int) -> RelayClient:
    client = RelayClient(host, port, player_id)
    client.send(1, payload=HELLO.pack(GAME_VERSION, access_code, 0))
    expect(client, 2)
    return client


def open_bot_room(client: RelayClient) -> int:
    if BOT_PLAYER_ID not in peer_list(client):
        raise AssertionError("bot is absent before the first room request")
    client.send(6, target=BOT_PLAYER_ID)
    source, _, session, outbound = expect(client, 7)
    if source != BOT_PLAYER_ID or not session or outbound != b"\x01":
        raise AssertionError("bot returned an invalid first SessionReady")
    return session


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="pokegochi-server.local")
    parser.add_argument("--port", type=int, default=32145)
    parser.add_argument("--access-code", type=int, required=True)
    parser.add_argument("--cycles", type=int, default=20)
    arguments = parser.parse_args()

    slowest_ms = 0.0
    for cycle in range(arguments.cycles):
        client = authenticate(arguments.host, arguments.port,
                              arguments.access_code, 74000 + cycle)
        try:
            started = time.monotonic()
            session = open_bot_room(client)
            if cycle % 2 == 0:
                # This is the exact failed-app trace: the room exists, but the
                # protocol returns to discovery before Invite is completed.
                # The same RefreshPeers response must already contain the bot.
                if BOT_PLAYER_ID not in peer_list(client):
                    raise AssertionError(
                        "abandoned first room hid the bot until reconnect"
                    )
            else:
                transaction = 0x42300000 | cycle
                send_game(client, BOT_PLAYER_ID, session, INVITE,
                          transaction=transaction)
                source, accepted_transaction, _ = expect_game(
                    client, INVITE_ACCEPT
                )
                if source != BOT_PLAYER_ID or accepted_transaction != transaction:
                    raise AssertionError("bot rejected the first invitation")
                client.send(9, target=BOT_PLAYER_ID, session=session)
                source, _, closed_session, _ = expect(client, 10)
                if source != BOT_PLAYER_ID or closed_session != session:
                    raise AssertionError("first bot room did not close cleanly")
                if BOT_PLAYER_ID not in peer_list(client):
                    raise AssertionError("bot did not return after first-room close")
            elapsed_ms = (time.monotonic() - started) * 1000.0
            slowest_ms = max(slowest_ms, elapsed_ms)
            if elapsed_ms >= 5000.0:
                raise AssertionError(
                    f"cycle {cycle + 1} entered the old 5 s recovery path"
                )
        finally:
            client.close()

    print(
        f"PASS: {arguments.cycles} first bot rooms accepted/released without "
        f"a reconnect delay; slowest={slowest_ms:.0f} ms"
    )


if __name__ == "__main__":
    main()
