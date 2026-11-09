"""Regression contract for the link-battle SYNC -> ready prompt hand-off."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


def function_body(signature: str, next_signature: str) -> str:
    start = MAIN.index(signature)
    return MAIN[start : MAIN.index(next_signature, start)]


def main() -> None:
    transition = function_body(
        "bool clearPvpTransientBattlePrompt(",
        "bool beginScreenPresentation(",
    )
    for transient in ("WAITING", "LOCKED", "SYNCING"):
        assert f'std::strstr(message, "{transient}")' in transition
    assert "message[0] = 0;" in transition
    assert "battlePromptScroll = 0;" in transition
    assert "invalidatePixels(0, 160, 320, 80);" in transition
    assert "requestRetainedFrame();" in transition

    authoritative = function_body(
        "bool applyPvpAuthoritativeState(",
        "bool useRareCandyOutsideBattle(",
    )
    assert "if (!pvpAwaitingAuthoritativeState)" in authoritative
    assert 'clearPvpTransientBattlePrompt("authoritative-applied")' in authoritative

    service = function_body(
        "__attribute__((noinline)) void servicePvpSynchronization(",
        "__attribute__((noinline)) void serviceMultiplayerTradeCommit(",
    )
    assert 'clearPvpTransientBattlePrompt("state-acknowledged")' in service
    assert 'clearPvpTransientBattlePrompt("state-ack-retry")' in service
    assert 'std::strstr(message, "SYNCING")' not in service

    # The small timer damage remains valid only because every transition out
    # of a transport prompt first dirties the complete lower battle surface.
    clock = function_body(
        "void servicePvpDecisionClock(",
        "void finishPendingBattleFlow(",
    )
    assert "invalidatePixels(274, 160, 44, 16);" in clock

    print("PvP prompt transition contract passed.")


if __name__ == "__main__":
    main()
