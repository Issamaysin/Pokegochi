#!/usr/bin/env python3
"""Drive three complete Balanced battles against a physical server bot."""

from __future__ import annotations

import argparse
import struct

from test_online_server import (
    ACTIVITY_ACCEPT,
    ACTIVITY_REQUEST,
    BATTLE_COMMAND,
    BATTLE_LEAD,
    BATTLE_PARTY_MEMBER,
    BATTLE_PARTY_READY,
    BATTLE_START,
    BATTLE_SYNC_ACK,
    BATTLE_SYNC_CORE,
    BATTLE_SYNC_POKEMON,
    BATTLE_SYNC_REPLACEMENT,
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


COMMAND_MOVE = 1
COMMAND_SWITCH = 2
COMMAND_FORFEIT = 3
COMMAND_WAIT = 4
COMMAND_ITEM = 5
OUTCOME_ONGOING = 1
OUTCOME_VICTORY = 2
OUTCOME_DEFEAT = 3
POKEMON_SIZE = 64
UID_OFFSET = 0
SPECIES_OFFSET = 8
HP_OFFSET = 16
MAX_HP_OFFSET = 18
MOVE_PP_OFFSET = 44
MOVES_OFFSET = 36
HELD_ITEM_OFFSET = 56
U_TURN = 369
Z_CRYSTAL_FIRST = 109
Z_CRYSTAL_LAST = 126


def read_u16(record: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", record, offset)[0]


def write_u16(record: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", record, offset, max(0, min(0xFFFF, value)))


def hp(record: bytes | bytearray) -> int:
    return read_u16(record, HP_OFFSET)


def maximum_hp(record: bytes | bytearray) -> int:
    return read_u16(record, MAX_HP_OFFSET)


def set_hp(record: bytearray, value: int) -> None:
    write_u16(record, HP_OFFSET, min(maximum_hp(record), max(0, value)))


def damage(record: bytearray, percent: int) -> None:
    amount = max(1, maximum_hp(record) * percent // 100)
    set_hp(record, hp(record) - amount)


def make_host_team(bot_team: list[bytearray], battle_number: int) -> list[bytearray]:
    team: list[bytearray] = []
    for slot, source in enumerate(bot_team):
        pokemon = bytearray(source)
        struct.pack_into(
            "<I", pokemon, UID_OFFSET,
            0x71000000 | (battle_number << 8) | (slot + 1),
        )
        set_hp(pokemon, maximum_hp(pokemon))
        team.append(pokemon)
    return team


def first_healthy(team: list[bytearray], excluded: int) -> int | None:
    for slot, pokemon in enumerate(team):
        if slot != excluded and hp(pokemon):
            return slot
    return None


def decrement_move_pp(pokemon: bytearray, command_value: int) -> None:
    if command_value & 0x80:
        slot = command_value & 0x07
    else:
        slot = command_value
    if slot >= 4:
        return
    offset = MOVE_PP_OFFSET + slot
    if pokemon[offset]:
        pokemon[offset] -= 1


def set_all_moves(pokemon: bytearray, move: int, pp: int) -> None:
    for slot in range(4):
        struct.pack_into("<H", pokemon, MOVES_OFFSET + slot * 2, move)
        pokemon[MOVE_PP_OFFSET + slot] = pp


def apply_bot_item(team: list[bytearray], command_value: int) -> None:
    item = command_value & 0x1F
    target = (command_value >> 5) & 0x03
    if target >= len(team):
        raise AssertionError("bot selected an invalid Bag target")
    pokemon = team[target]
    if item == 2:  # Hyper Potion
        set_hp(pokemon, hp(pokemon) + 200)
    elif item == 15:  # Max Revive
        set_hp(pokemon, maximum_hp(pokemon))
    elif item == 19:  # Max Elixir
        # Exact maxima are not needed by this protocol peer. Restoring a
        # non-zero PP value is enough to make the next decision observable.
        for slot in range(4):
            if pokemon[MOVE_PP_OFFSET + slot] == 0:
                pokemon[MOVE_PP_OFFSET + slot] = 5


def send_snapshot(
    client: RelayClient,
    session: int,
    state_turn: int,
    host_team: list[bytearray],
    bot_team: list[bytearray],
    host_active: int,
    bot_active: int,
    active: bool,
    outcome: int,
    checksum_serial: int,
    host_replacement_reason: int = 0,
    guest_replacement_reason: int = 0,
) -> None:
    for side, team in ((0, host_team), (1, bot_team)):
        for slot, pokemon in enumerate(team):
            send_game(
                client,
                BOT_PLAYER_ID,
                session,
                BATTLE_SYNC_POKEMON,
                struct.pack("<HBB", state_turn, side, slot) + bytes(pokemon),
                transaction=state_turn,
            )
    checksum = 0xC3910000 | (checksum_serial & 0xFFFF)
    core = bytearray(32)
    struct.pack_into("<HI", core, 0, state_turn, checksum)
    core[10] = 1 if active else 0
    core[11] = outcome
    core[16:20] = bytes((len(host_team), len(bot_team), host_active, bot_active))
    send_game(
        client,
        BOT_PLAYER_ID,
        session,
        BATTLE_SYNC_REPLACEMENT,
        struct.pack(
            "<HBB", state_turn, host_replacement_reason,
            guest_replacement_reason,
        ),
        transaction=state_turn,
    )
    send_game(
        client,
        BOT_PLAYER_ID,
        session,
        BATTLE_SYNC_CORE,
        bytes(core),
        transaction=state_turn,
    )
    _, _, acknowledgement = expect_game(client, BATTLE_SYNC_ACK)
    if acknowledgement != struct.pack("<HI", state_turn, checksum):
        raise AssertionError("bot acknowledged a different battle snapshot")


def play_chained_pivot_regression(
    host: str, port: int, access_code: int
) -> None:
    """Bot pivots first; queued host U-turn then requires a second barrier."""
    client, session, host_team, bot_team = open_battle(host, port, access_code, 90)
    host_active = 0
    # Keep the first bot action out of its single Z slot, because a damaging
    # Z-U-turn intentionally does not pivot. The held Z-Crystal identifies that
    # slot without relying on the server's private Team layout.
    bot_active = next(
        slot for slot, pokemon in enumerate(bot_team)
        if not Z_CRYSTAL_FIRST <= pokemon[HELD_ITEM_OFFSET] <= Z_CRYSTAL_LAST
    )
    set_all_moves(host_team[host_active], U_TURN, 20)
    set_all_moves(bot_team[bot_active], U_TURN, 20)
    checksum_serial = 90000
    try:
        # Publish the controlled teams and active slots before the command.
        send_snapshot(
            client, session, 0, host_team, bot_team, host_active, bot_active,
            True, OUTCOME_ONGOING, checksum_serial,
        )

        send_game(
            client, BOT_PLAYER_ID, session, BATTLE_COMMAND,
            struct.pack("<HBB", 0, COMMAND_MOVE, 0), transaction=0,
        )
        _, _, first = expect_game(client, BATTLE_COMMAND)
        first_turn, first_kind, first_value = struct.unpack("<HBB", first)
        if first_turn != 0 or first_kind != COMMAND_MOVE or first_value & 0x80:
            raise AssertionError("bot did not choose the controlled plain U-turn")

        # The faster bot U-turn executes first. The engine has incremented the
        # battle turn from command turn 0 to replacement turn 1.
        decrement_move_pp(bot_team[bot_active], first_value)
        checksum_serial += 1
        send_snapshot(
            client, session, 1, host_team, bot_team, host_active, bot_active,
            True, OUTCOME_ONGOING, checksum_serial,
            guest_replacement_reason=2,
        )
        send_game(
            client, BOT_PLAYER_ID, session, BATTLE_COMMAND,
            struct.pack("<HBB", 1, COMMAND_WAIT, 0), transaction=1,
        )
        _, _, bot_switch = expect_game(client, BATTLE_COMMAND)
        switch_turn, switch_kind, switch_value = struct.unpack("<HBB", bot_switch)
        if (switch_turn != 1 or switch_kind != COMMAND_SWITCH or
                switch_value == bot_active or not hp(bot_team[switch_value])):
            raise AssertionError("bot failed its U-turn replacement barrier")
        bot_active = switch_value

        # The host's already-queued U-turn now lands on that replacement. The
        # server must remember the host action across the bot's switch and WAIT
        # for the host recipient instead of selecting a second attack.
        decrement_move_pp(host_team[host_active], 0)
        checksum_serial += 1
        send_snapshot(
            client, session, 1, host_team, bot_team, host_active, bot_active,
            True, OUTCOME_ONGOING, checksum_serial,
            host_replacement_reason=2,
        )
        host_replacement = 1
        send_game(
            client, BOT_PLAYER_ID, session, BATTLE_COMMAND,
            struct.pack("<HBB", 1, COMMAND_SWITCH, host_replacement),
            transaction=1,
        )
        _, _, wait = expect_game(client, BATTLE_COMMAND)
        wait_turn, wait_kind, _ = struct.unpack("<HBB", wait)
        if wait_turn != 1 or wait_kind != COMMAND_WAIT:
            raise AssertionError("server treated the U-turn recipient as a new action")

        host_active = host_replacement
        checksum_serial += 1
        send_snapshot(
            client, session, 1, host_team, bot_team, host_active, bot_active,
            True, OUTCOME_ONGOING, checksum_serial,
        )

        # A genuinely new decision follows the completed barriers and is also
        # numbered 1. It must now receive an ordinary bot action.
        send_game(
            client, BOT_PLAYER_ID, session, BATTLE_COMMAND,
            struct.pack("<HBB", 1, COMMAND_MOVE, 0), transaction=1,
        )
        _, _, resumed = expect_game(client, BATTLE_COMMAND)
        resumed_turn, resumed_kind, _ = struct.unpack("<HBB", resumed)
        if resumed_turn != 1 or resumed_kind not in (COMMAND_MOVE, COMMAND_ITEM):
            raise AssertionError("normal play did not resume after chained pivots")

        client.send(9, target=BOT_PLAYER_ID, session=session)
        source, _, closed_session, _ = expect(client, 10)
        if source != BOT_PLAYER_ID or closed_session != session:
            raise AssertionError("bot room did not close after pivot regression")
    finally:
        client.close()

    print("PHYSICAL PIVOT: chained bot/host U-turn barriers passed")


def open_battle(
    host: str, port: int, access_code: int, battle_number: int
) -> tuple[RelayClient, int, list[bytearray], list[bytearray]]:
    client = RelayClient(host, port, 73000 + battle_number)
    client.send(1, payload=HELLO.pack(GAME_VERSION, access_code, battle_number))
    expect(client, 2)
    client.send(6, target=BOT_PLAYER_ID)
    source, _, session, outbound = expect(client, 7)
    if source != BOT_PLAYER_ID or outbound != b"\x01":
        raise AssertionError("bot did not open the requested room")
    send_game(client, BOT_PLAYER_ID, session, INVITE,
              transaction=0xC3910000 | battle_number)
    source, transaction, _ = expect_game(client, INVITE_ACCEPT)
    if source != BOT_PLAYER_ID or transaction != 0xC3910000 | battle_number:
        raise AssertionError("bot did not accept the battle invitation")
    send_game(client, BOT_PLAYER_ID, session, ACTIVITY_REQUEST, b"\x01\x02")
    _, _, acceptance = expect_game(client, ACTIVITY_ACCEPT)
    if acceptance != b"\x01\x02":
        raise AssertionError("bot did not accept Balanced rules")

    bot_team: list[bytearray] = []
    for expected_slot in range(3):
        _, _, member = expect_game(client, BATTLE_PARTY_MEMBER)
        if len(member) != POKEMON_SIZE + 1 or member[0] != expected_slot:
            raise AssertionError("bot published a malformed party")
        bot_team.append(bytearray(member[1:]))
    _, _, ready = expect_game(client, BATTLE_PARTY_READY)
    _, _, lead = expect_game(client, BATTLE_LEAD)
    if ready != b"\x03" or lead != b"\x00":
        raise AssertionError("bot party preparation did not finish")

    host_team = make_host_team(bot_team, battle_number)
    for slot, pokemon in enumerate(host_team):
        send_game(
            client,
            BOT_PLAYER_ID,
            session,
            BATTLE_PARTY_MEMBER,
            bytes((slot,)) + bytes(pokemon),
        )
    send_game(client, BOT_PLAYER_ID, session, BATTLE_PARTY_READY, b"\x03")
    send_game(client, BOT_PLAYER_ID, session, BATTLE_LEAD, b"\x00")
    send_game(
        client,
        BOT_PLAYER_ID,
        session,
        BATTLE_START,
        struct.pack("<IBB", 0x39100000 | battle_number, 0, 0),
    )
    return client, session, host_team, bot_team


def play_battle(
    host: str,
    port: int,
    access_code: int,
    battle_number: int,
    host_should_win: bool,
) -> tuple[int, int, int]:
    client, session, host_team, bot_team = open_battle(
        host, port, access_code, battle_number
    )
    state_turn = 0
    host_active = 0
    bot_active = 0
    checksum_serial = battle_number * 1000
    normal_turns = 0
    forced_barriers = 0
    item_turns = 0
    try:
        while normal_turns < 120:
            send_game(
                client,
                BOT_PLAYER_ID,
                session,
                BATTLE_COMMAND,
                struct.pack("<HBB", state_turn, COMMAND_MOVE, 0),
                transaction=state_turn,
            )
            _, _, command = expect_game(client, BATTLE_COMMAND)
            bot_turn, kind, value = struct.unpack("<HBB", command)
            if bot_turn != state_turn:
                raise AssertionError("bot answered a different battle turn")
            if kind not in (COMMAND_MOVE, COMMAND_ITEM):
                raise AssertionError(
                    f"unexpected ordinary bot command kind={kind}"
                )

            decrement_move_pp(host_team[host_active], 0)
            if kind == COMMAND_MOVE:
                decrement_move_pp(bot_team[bot_active], value)
                damage(host_team[host_active], 15 if host_should_win else 65)
            else:
                apply_bot_item(bot_team, value)
                item_turns += 1
            damage(bot_team[bot_active], 65 if host_should_win else 15)
            normal_turns += 1
            state_turn += 1

            host_replacement = (
                first_healthy(host_team, host_active)
                if hp(host_team[host_active]) == 0 else None
            )
            bot_replacement = (
                first_healthy(bot_team, bot_active)
                if hp(bot_team[bot_active]) == 0 else None
            )
            host_lost = hp(host_team[host_active]) == 0 and host_replacement is None
            bot_lost = hp(bot_team[bot_active]) == 0 and bot_replacement is None
            if host_lost or bot_lost:
                outcome = OUTCOME_DEFEAT if host_lost else OUTCOME_VICTORY
                checksum_serial += 1
                send_snapshot(
                    client, session, state_turn, host_team, bot_team,
                    host_active, bot_active, False, outcome, checksum_serial,
                )
                expected_host_win = outcome == OUTCOME_VICTORY
                if expected_host_win != host_should_win:
                    raise AssertionError("battle ended for the unintended side")
                break

            checksum_serial += 1
            send_snapshot(
                client, session, state_turn, host_team, bot_team,
                host_active, bot_active, True, OUTCOME_ONGOING,
                checksum_serial,
            )

            if hp(host_team[host_active]) == 0 or hp(bot_team[bot_active]) == 0:
                _, _, forced = expect_game(client, BATTLE_COMMAND)
                forced_turn, forced_kind, forced_value = struct.unpack(
                    "<HBB", forced
                )
                if forced_turn != state_turn:
                    raise AssertionError("forced replacement used a stale turn")
                if hp(bot_team[bot_active]) == 0:
                    if forced_kind != COMMAND_SWITCH or bot_replacement is None:
                        raise AssertionError("bot failed to replace its fainted Pokemon")
                    if forced_value != bot_replacement:
                        if forced_value >= len(bot_team) or not hp(bot_team[forced_value]):
                            raise AssertionError("bot selected an invalid replacement")
                        bot_replacement = forced_value
                    bot_active = bot_replacement
                else:
                    if host_replacement is None:
                        raise AssertionError("host replacement disappeared")
                    host_active = host_replacement
                    if forced_kind == COMMAND_SWITCH:
                        if forced_value >= len(bot_team) or not hp(bot_team[forced_value]):
                            raise AssertionError("bot pivoted to an invalid replacement")
                        bot_active = forced_value
                    elif forced_kind != COMMAND_WAIT:
                        raise AssertionError("bot did not wait for the host replacement")
                forced_barriers += 1
                checksum_serial += 1
                send_snapshot(
                    client, session, state_turn, host_team, bot_team,
                    host_active, bot_active, True, OUTCOME_ONGOING,
                    checksum_serial,
                )
        else:
            raise AssertionError("complete battle exceeded 120 turns")

        client.send(9, target=BOT_PLAYER_ID, session=session)
        source, _, closed_session, _ = expect(client, 10)
        if source != BOT_PLAYER_ID or closed_session != session:
            raise AssertionError("bot room did not close after the final snapshot")
        return normal_turns, forced_barriers, item_turns
    finally:
        client.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="pokegochi-server.local")
    parser.add_argument("--port", type=int, default=32145)
    parser.add_argument("--access-code", type=int, required=True)
    arguments = parser.parse_args()

    monitor = RelayClient(arguments.host, arguments.port, 73999)
    monitor.send(1, payload=HELLO.pack(GAME_VERSION, arguments.access_code, 0))
    expect(monitor, 2)
    try:
        initial_wins = peer_list(monitor)[BOT_PLAYER_ID]
        configurations = (True, False, True)
        for number, host_should_win in enumerate(configurations, 1):
            turns, barriers, items = play_battle(
                arguments.host,
                arguments.port,
                arguments.access_code,
                number,
                host_should_win,
            )
            available = peer_list(monitor)
            if BOT_PLAYER_ID not in available:
                raise AssertionError("bot did not return after a complete battle")
            print(
                f"PHYSICAL BATTLE {number}: "
                f"{'host' if host_should_win else 'bot'} victory, "
                f"turns={turns}, replacements={barriers}, items={items}"
            )
        final_wins = peer_list(monitor)[BOT_PLAYER_ID]
        if final_wins != initial_wins + 1:
            raise AssertionError(
                f"bot Balanced wins changed by {final_wins - initial_wins}, expected 1"
            )
        play_chained_pivot_regression(
            arguments.host, arguments.port, arguments.access_code
        )
    finally:
        monitor.close()

    print("PASS: three complete physical battles against server bot")


if __name__ == "__main__":
    main()
