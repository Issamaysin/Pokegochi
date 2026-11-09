#include <cassert>
#include <cstdint>

#include "services/Multiplayer.h"

void pump(Multiplayer& from, Multiplayer& to) {
  uint8_t bytes[174]{};
  uint16_t length = 0;
  while (from.takeOutgoingPacket(bytes, length)) to.transportPacket(bytes, length);
}

OwnedPokemon pokemon(uint32_t uid, uint16_t species, uint32_t personality) {
  OwnedPokemon value{};
  value.uid = uid;
  value.speciesId = species;
  value.personality = personality;
  value.level = 50;
  value.maximumHp = 100;
  value.currentHp = 100;
  return value;
}

void tradeOnce(Multiplayer& coordinator, Multiplayer& peer,
               const OwnedPokemon& coordinatorOffer,
               const OwnedPokemon& peerOffer) {
  assert(coordinator.requestActivity(Multiplayer::Activity::Trade));
  pump(coordinator, peer);
  assert(peer.state() == Multiplayer::State::ConfirmIncomingActivity);
  peer.answerActivity(true);
  pump(peer, coordinator);
  assert(coordinator.state() == Multiplayer::State::TradeSelecting);
  assert(peer.state() == Multiplayer::State::TradeSelecting);

  assert(coordinator.submitTradeOffer(coordinatorOffer));
  pump(coordinator, peer);
  assert(peer.submitTradeOffer(peerOffer));
  pump(peer, coordinator);
  assert(coordinator.state() == Multiplayer::State::TradeConfirming);
  assert(peer.state() == Multiplayer::State::TradeConfirming);

  assert(peer.setTradeConfirmation(true));
  pump(peer, coordinator);
  assert(coordinator.setTradeConfirmation(true));
  pump(coordinator, peer);

  Multiplayer::TradeCommit peerCommit{};
  assert(peer.takeTradeCommit(peerCommit));
  assert(peerCommit.outgoingUid == peerOffer.uid);
  assert(peerCommit.incoming.speciesId == coordinatorOffer.speciesId);
  peer.completeTradeCommit(true);
  pump(peer, coordinator);

  Multiplayer::TradeCommit coordinatorCommit{};
  assert(coordinator.takeTradeCommit(coordinatorCommit));
  assert(coordinatorCommit.outgoingUid == coordinatorOffer.uid);
  assert(coordinatorCommit.incoming.speciesId == peerOffer.speciesId);
  coordinator.completeTradeCommit(true);
  pump(coordinator, peer);

  assert(coordinator.state() == Multiplayer::State::Connected);
  assert(peer.state() == Multiplayer::State::Connected);
  assert(coordinator.activity() == Multiplayer::Activity::None);
  assert(peer.activity() == Multiplayer::Activity::None);
}

int main() {
  Multiplayer app;
  Multiplayer device;
  assert(app.beginWithPlayerId(1001));
  assert(device.beginWithPlayerId(2002));
  assert(app.setEnabled(true));
  assert(device.setEnabled(true));

  device.transportConnected(false);
  assert(app.requestPeer(2002));
  pump(app, device);
  assert(device.state() == Multiplayer::State::ConfirmIncomingPeer);
  device.answerPeerInvitation(true);
  pump(device, app);
  assert(app.state() == Multiplayer::State::Connected);
  assert(device.state() == Multiplayer::State::Connected);
  assert(app.initiatedConnection());
  assert(!device.initiatedConnection());
  assert(!device.requestActivity(Multiplayer::Activity::Battle));
  assert(!device.requestActivity(Multiplayer::Activity::Trade));

  assert(app.requestActivity(Multiplayer::Activity::Battle));
  assert(app.state() == Multiplayer::State::BattlePreparing);
  pump(app, device);
  assert(device.state() == Multiplayer::State::Connected);
  assert(app.chooseBattleRules(Multiplayer::BattleRules::Balanced));
  assert(app.state() == Multiplayer::State::WaitingActivityReply);
  pump(app, device);
  assert(device.state() == Multiplayer::State::ConfirmIncomingActivity);
  assert(device.battleRules() == Multiplayer::BattleRules::Balanced);
  assert(!device.chooseBattleRules(Multiplayer::BattleRules::Normal));
  device.answerActivity(true);
  pump(device, app);
  assert(app.activity() == Multiplayer::Activity::Battle);
  assert(device.activity() == Multiplayer::Activity::Battle);
  assert(app.state() == Multiplayer::State::BattlePreparing);
  assert(device.state() == Multiplayer::State::BattlePreparing);

  // The connection initiator's complete proposal is authoritative. The peer
  // only accepts it, so Party snapshots can never disagree on the rules.
  assert(app.battleRulesReady());
  assert(device.battleRulesReady());
  assert(app.battleRules() == Multiplayer::BattleRules::Balanced);
  assert(device.battleRules() == Multiplayer::BattleRules::Balanced);

  PokemonCollection appParty{};
  appParty.box[0] = CollectionLogic::createPokemon(101, 25, 42, false, 1234, 7);
  appParty.box[0].currentHp = 0;
  appParty.box[0].status = StatusCondition::Burn;
  appParty.box[0].movePp[0] = 0;
  appParty.party[0] = appParty.box[0].uid;
  appParty.box[1] = CollectionLogic::createPokemon(102, 6, 43, false, 1235, 7);
  appParty.box[2] = CollectionLogic::createPokemon(103, 9, 44, false, 1236, 7);
  appParty.party[1] = appParty.box[1].uid;
  appParty.party[2] = appParty.box[2].uid;
  PokemonCollection deviceParty{};
  deviceParty.box[0] = CollectionLogic::createPokemon(201, 133, 88, false, 5678, 7);
  deviceParty.box[0].currentHp = 1;
  deviceParty.box[0].status = StatusCondition::Paralysis;
  deviceParty.box[0].movePp[0] = 0;
  deviceParty.party[0] = deviceParty.box[0].uid;
  deviceParty.box[1] = CollectionLogic::createPokemon(202, 134, 89, false, 5679, 7);
  deviceParty.box[2] = CollectionLogic::createPokemon(203, 135, 90, false, 5680, 7);
  deviceParty.party[1] = deviceParty.box[1].uid;
  deviceParty.party[2] = deviceParty.box[2].uid;

  assert(app.setBattleReady(appParty));
  pump(app, device);
  // Receiving READY first must not lock this player's Box/Party preparation.
  // Only the player who pressed READY enters the locked waiting state.
  assert(app.state() == Multiplayer::State::BattleWaitingReady);
  assert(app.localBattleReady() && !app.remoteBattleReady());
  assert(device.state() == Multiplayer::State::BattlePreparing);
  assert(!device.localBattleReady() && device.remoteBattleReady());
  assert(device.setBattleReady(deviceParty));
  pump(device, app);
  assert(app.state() == Multiplayer::State::BattleChoosingLead);
  assert(device.state() == Multiplayer::State::BattleChoosingLead);
  const OwnedPokemon& appPrepared = app.localBattleRoster().members[0];
  const OwnedPokemon& devicePrepared = device.localBattleRoster().members[0];
  assert(appPrepared.level == 70 && devicePrepared.level == 70);
  assert(appPrepared.currentHp == appPrepared.maximumHp);
  assert(devicePrepared.currentHp == devicePrepared.maximumHp);
  assert(appPrepared.status == StatusCondition::None);
  assert(devicePrepared.status == StatusCondition::None);
  assert(appPrepared.movePp[0] == CollectionLogic::maximumMovePp(appPrepared, 0));
  assert(devicePrepared.movePp[0] == CollectionLogic::maximumMovePp(devicePrepared, 0));

  assert(app.submitBattleLead(0));
  pump(app, device);
  assert(device.submitBattleLead(0));
  pump(device, app);
  Multiplayer::BattleLaunch appLaunch{};
  assert(app.takeBattleLaunch(appLaunch));
  pump(app, device);
  Multiplayer::BattleLaunch deviceLaunch{};
  assert(device.takeBattleLaunch(deviceLaunch));
  assert(appLaunch.rules == Multiplayer::BattleRules::Balanced);
  assert(deviceLaunch.rules == Multiplayer::BattleRules::Balanced);
  assert(app.submitBattleCommand(Multiplayer::BattleCommandKind::Item, 0x42U, 0));
  assert(device.submitBattleCommand(Multiplayer::BattleCommandKind::Move, 1U, 0));
  pump(app, device);
  pump(device, app);
  Multiplayer::BattleCommand appLocal{}, appRemote{};
  Multiplayer::BattleCommand deviceLocal{}, deviceRemote{};
  assert(app.takeBattleCommands(appLocal, appRemote));
  assert(device.takeBattleCommands(deviceLocal, deviceRemote));
  assert(appLocal.kind == Multiplayer::BattleCommandKind::Item);
  assert(appRemote.kind == Multiplayer::BattleCommandKind::Move);

  // Reproduce the complete faint barrier, not only the engine's switch
  // helper. The host publishes all six roster snapshots, the defeated guest
  // acknowledges them, chooses a replacement, and both peers continue into
  // another command turn without closing the session.
  BattleState faintedState{};
  faintedState.active = true;
  faintedState.kind = BattleKind::Pvp;
  faintedState.outcome = BattleOutcome::Ongoing;
  faintedState.turn = 1;
  faintedState.rngState = 0x12345678U;
  faintedState.playerUid = app.localBattleRoster().members[0].uid;
  appParty.box[0].currentHp = appParty.box[0].maximumHp;
  faintedState.opponentCount = device.remoteBattleRoster().count;
  faintedState.opponentIndex = 0;
  for (uint8_t slot = 0; slot < faintedState.opponentCount; ++slot)
    faintedState.opponents[slot] = device.localBattleRoster().members[slot];
  // From the host's perspective the guest lead fainted and roster slot 1 is
  // the first valid replacement.
  faintedState.opponents[0].currentHp = 0;
  faintedState.opponents[1].currentHp = faintedState.opponents[1].maximumHp;
  const uint8_t remoteRosterSlots[kPartyCapacity]{0, 1, 2};
  assert(app.isBattleHost());
  assert(app.state() == Multiplayer::State::BattleActive);
  assert(app.localBattleRoster().count == kPartyCapacity);
  assert(app.remoteBattleRoster().count == kPartyCapacity);
  for (uint8_t slot = 0; slot < kPartyCapacity; ++slot)
    assert(CollectionLogic::find(appParty, app.localBattleRoster().members[slot].uid));
  assert(app.publishBattleState(faintedState, appParty, remoteRosterSlots));
  pump(app, device);

  Multiplayer::BattleStateSync syncedFaint{};
  assert(device.takeBattleState(syncedFaint));
  assert(syncedFaint.ready);
  assert(syncedFaint.hostCount == kPartyCapacity);
  assert(syncedFaint.guestCount == kPartyCapacity);
  assert(syncedFaint.guestParty[0].currentHp == 0);
  assert(syncedFaint.guestParty[1].currentHp > 0);
  assert(syncedFaint.snapshotChecksum != 0U);
  assert(device.acknowledgeBattleState(
      syncedFaint.turn, syncedFaint.snapshotChecksum));
  pump(device, app);
  uint16_t acknowledgedTurn = 0;
  uint32_t acknowledgedChecksum = 0;
  assert(app.takeBattleStateAcknowledgement(
      acknowledgedTurn, acknowledgedChecksum));
  assert(acknowledgedTurn == faintedState.turn);
  assert(acknowledgedChecksum == syncedFaint.snapshotChecksum);
  assert(acknowledgedChecksum == app.publishedBattleStateChecksum());

  assert(device.submitBattleCommand(Multiplayer::BattleCommandKind::Switch, 1,
                                    faintedState.turn));
  assert(app.submitBattleCommand(Multiplayer::BattleCommandKind::Wait, 0,
                                 faintedState.turn));
  pump(device, app);
  pump(app, device);
  Multiplayer::BattleCommand appForcedLocal{}, appForcedRemote{};
  Multiplayer::BattleCommand deviceForcedLocal{}, deviceForcedRemote{};
  assert(app.takeBattleCommands(appForcedLocal, appForcedRemote));
  assert(device.takeBattleCommands(deviceForcedLocal, deviceForcedRemote));
  assert(appForcedLocal.kind == Multiplayer::BattleCommandKind::Wait);
  assert(appForcedRemote.kind == Multiplayer::BattleCommandKind::Switch);
  assert(appForcedRemote.value == 1);
  assert(deviceForcedLocal.kind == Multiplayer::BattleCommandKind::Switch);
  assert(deviceForcedRemote.kind == Multiplayer::BattleCommandKind::Wait);

  // The forced replacement is a second authoritative snapshot with the same
  // battle turn. Its digest must differ, and an old ACK must remain visibly
  // stale instead of clearing the new publication barrier.
  BattleState switchedState = faintedState;
  switchedState.opponentIndex = 1;
  assert(app.publishBattleState(switchedState, appParty, remoteRosterSlots));
  const uint32_t switchedChecksum = app.publishedBattleStateChecksum();
  assert(switchedChecksum != 0U);
  assert(switchedChecksum != syncedFaint.snapshotChecksum);
  pump(app, device);
  Multiplayer::BattleStateSync syncedSwitch{};
  assert(device.takeBattleState(syncedSwitch));
  assert(syncedSwitch.ready);
  assert(syncedSwitch.turn == syncedFaint.turn);
  assert(syncedSwitch.guestActiveRosterSlot == 1U);
  assert(syncedSwitch.snapshotChecksum == switchedChecksum);
  assert(device.acknowledgeBattleState(
      syncedFaint.turn, syncedFaint.snapshotChecksum));
  pump(device, app);
  assert(app.takeBattleStateAcknowledgement(
      acknowledgedTurn, acknowledgedChecksum));
  assert(acknowledgedChecksum != app.publishedBattleStateChecksum());
  assert(device.acknowledgeBattleState(
      syncedSwitch.turn, syncedSwitch.snapshotChecksum));
  pump(device, app);
  assert(app.takeBattleStateAcknowledgement(
      acknowledgedTurn, acknowledgedChecksum));
  assert(acknowledgedChecksum == app.publishedBattleStateChecksum());

  assert(app.submitBattleCommand(Multiplayer::BattleCommandKind::Move, 0, 2));
  assert(device.submitBattleCommand(Multiplayer::BattleCommandKind::Move, 0, 2));
  pump(app, device);
  pump(device, app);
  assert(app.takeBattleCommands(appLocal, appRemote));
  assert(device.takeBattleCommands(deviceForcedLocal, deviceForcedRemote));
  assert(app.connected() && device.connected());

  // RUN is a unilateral concession. The sender resolves FORFEIT against a
  // synthetic WAIT immediately, before the packet is pumped to the peer. It
  // also supersedes a move already locked while waiting on a stalled peer.
  assert(app.submitBattleCommand(
      Multiplayer::BattleCommandKind::Move, 2U, 3));
  assert(app.submitBattleCommand(
      Multiplayer::BattleCommandKind::Forfeit, 0, 3));
  assert(app.localBattleCommandPending());
  assert(app.takeBattleCommands(appLocal, appRemote));
  assert(appLocal.kind == Multiplayer::BattleCommandKind::Forfeit);
  assert(appRemote.kind == Multiplayer::BattleCommandKind::Wait);
  assert(!app.localBattleCommandPending());
  pump(app, device);
  assert(device.takeBattleCommands(deviceForcedLocal, deviceForcedRemote));
  assert(deviceForcedLocal.kind == Multiplayer::BattleCommandKind::Wait);
  assert(deviceForcedRemote.kind == Multiplayer::BattleCommandKind::Forfeit);
  pump(device, app);

  app.clearActivity();
  pump(app, device);
  assert(app.state() == Multiplayer::State::Connected);
  assert(device.state() == Multiplayer::State::Connected);

  // NORMAL preserves each original level/stat build while still starting from
  // disposable full HP/status/PP snapshots.
  assert(app.requestActivity(Multiplayer::Activity::Battle));
  assert(app.chooseBattleRules(Multiplayer::BattleRules::Normal));
  pump(app, device);
  assert(device.state() == Multiplayer::State::ConfirmIncomingActivity);
  assert(device.battleRules() == Multiplayer::BattleRules::Normal);
  device.answerActivity(true);
  pump(device, app);
  assert(app.setBattleReady(appParty));
  pump(app, device);
  assert(device.setBattleReady(deviceParty));
  pump(device, app);
  const OwnedPokemon& normalPrepared = app.localBattleRoster().members[0];
  assert(normalPrepared.level == appParty.box[0].level);
  assert(normalPrepared.maximumHp == appParty.box[0].maximumHp);
  assert(normalPrepared.currentHp == normalPrepared.maximumHp);
  assert(normalPrepared.status == StatusCondition::None);
  assert(normalPrepared.movePp[0] == CollectionLogic::maximumMovePp(normalPrepared, 0));
  app.clearActivity();
  pump(app, device);
  assert(app.state() == Multiplayer::State::Connected);
  assert(device.state() == Multiplayer::State::Connected);

  // The lower player id coordinates the two-phase commit.  Two consecutive
  // trades on the same BLE connection cover the stale commit-latch regression
  // that used to leave the second trade waiting forever after confirmation.
  tradeOnce(app, device, pokemon(11, 25, 0x11111111U),
            pokemon(21, 133, 0x22222222U));
  tradeOnce(app, device, pokemon(12, 643, 0x33333333U),
            pokemon(22, 644, 0x44444444U));

  // Online discovery is a bounded query. Results received before COMPLETE
  // remain selectable, while a delayed PeerEntry must not silently mutate a
  // completed list. An empty result returns to the explicit FIND PLAYER state.
  Multiplayer wifiSearch;
  assert(wifiSearch.beginWithPlayerId(9008));
  assert(wifiSearch.selectTransport(Multiplayer::TransportKind::Wifi));
  assert(wifiSearch.setEnabled(true));
  wifiSearch.startScan();
  assert(wifiSearch.state() == Multiplayer::State::Scanning);
  assert(wifiSearch.peerSearchInProgress());
  wifiSearch.transportDiscovered(9101, 0, "ONLINE-9101", 0, 7);
  assert(wifiSearch.peerCount() == 1);
  wifiSearch.completeOnlinePeerScan();
  assert(!wifiSearch.peerSearchInProgress());
  assert(wifiSearch.state() == Multiplayer::State::Scanning);
  assert(wifiSearch.peerCount() == 1);
  wifiSearch.transportDiscovered(9102, 0, "ONLINE-9102", 0, 9);
  assert(wifiSearch.peerCount() == 1);
  wifiSearch.startScan();
  assert(wifiSearch.peerSearchInProgress());
  assert(wifiSearch.peerCount() == 0);
  wifiSearch.completeOnlinePeerScan();
  assert(!wifiSearch.peerSearchInProgress());
  assert(wifiSearch.state() == Multiplayer::State::Advertising);

  // The same activity state machine is transport-independent. Wi-Fi exposes
  // the exact two-phase trade that BLE uses instead of hiding the option.
  Multiplayer wifiHost;
  Multiplayer wifiPeer;
  assert(wifiHost.beginWithPlayerId(9009));
  assert(wifiPeer.beginWithPlayerId(9010));
  assert(wifiHost.selectTransport(Multiplayer::TransportKind::Wifi));
  assert(wifiPeer.selectTransport(Multiplayer::TransportKind::Wifi));
  assert(wifiHost.setEnabled(true));
  assert(wifiPeer.setEnabled(true));
  wifiPeer.transportConnected(false);
  assert(wifiHost.requestPeer(9010));
  pump(wifiHost, wifiPeer);
  wifiPeer.answerPeerInvitation(true);
  pump(wifiPeer, wifiHost);
  assert(wifiHost.transportKind() == Multiplayer::TransportKind::Wifi);
  assert(wifiPeer.transportKind() == Multiplayer::TransportKind::Wifi);
  tradeOnce(wifiHost, wifiPeer, pokemon(41, 778, 0x77777777U),
            pokemon(42, 807, 0x88888888U));

  // Closing or declining a logical session must release the physical link so
  // the very next invitation can establish a fresh connection.
  app.disconnect();
  pump(app, device);
  assert(app.state() == Multiplayer::State::Advertising);
  assert(device.state() == Multiplayer::State::Advertising);
  assert(app.requestPeer(2002));
  pump(app, device);
  assert(device.state() == Multiplayer::State::ConfirmIncomingPeer);
  device.answerPeerInvitation(false);
  pump(device, app);
  assert(app.state() == Multiplayer::State::Advertising);
  assert(device.state() == Multiplayer::State::Advertising);
  assert(app.requestPeer(2002));
  pump(app, device);
  device.answerPeerInvitation(true);
  pump(device, app);
  assert(app.state() == Multiplayer::State::Connected);
  assert(device.state() == Multiplayer::State::Connected);

  // A lost InviteAccept is recovered when the initiator repeats its Invite.
  Multiplayer retryInitiator;
  Multiplayer retryReceiver;
  assert(retryInitiator.beginWithPlayerId(3003));
  assert(retryReceiver.beginWithPlayerId(4004));
  assert(retryInitiator.setEnabled(true));
  assert(retryReceiver.setEnabled(true));
  retryReceiver.transportConnected(false);
  assert(retryInitiator.requestPeer(4004));
  uint8_t repeatedInvite[174]{};
  uint16_t repeatedInviteLength = 0;
  assert(retryInitiator.takeOutgoingPacket(repeatedInvite, repeatedInviteLength));
  retryReceiver.transportPacket(repeatedInvite, repeatedInviteLength);
  retryReceiver.answerPeerInvitation(true);
  uint8_t lostAccept[174]{};
  uint16_t lostAcceptLength = 0;
  assert(retryReceiver.takeOutgoingPacket(lostAccept, lostAcceptLength));
  assert(retryInitiator.state() == Multiplayer::State::Connecting);
  retryReceiver.transportPacket(repeatedInvite, repeatedInviteLength);
  pump(retryReceiver, retryInitiator);
  assert(retryInitiator.state() == Multiplayer::State::Connected);
  assert(retryReceiver.state() == Multiplayer::State::Connected);

  // If both players initiate together, player id deterministically resolves
  // the crossed invitations instead of leaving both at an incoming prompt.
  Multiplayer lowerId;
  Multiplayer higherId;
  assert(lowerId.beginWithPlayerId(5005));
  assert(higherId.beginWithPlayerId(6006));
  assert(lowerId.setEnabled(true));
  assert(higherId.setEnabled(true));
  assert(lowerId.requestPeer(6006));
  assert(higherId.requestPeer(5005));
  pump(lowerId, higherId);
  pump(higherId, lowerId);
  pump(lowerId, higherId);
  pump(higherId, lowerId);
  assert(lowerId.state() == Multiplayer::State::Connected);
  assert(higherId.state() == Multiplayer::State::Connected);

  // Turning Bluetooth off on the peer is an unplanned physical loss. Even in
  // the middle of a trade it must expose one recovery edge and discard only
  // volatile offers/commit latches; the main UI uses the edge to rebuild Home.
  Multiplayer lostLinkHost;
  Multiplayer lostLinkPeer;
  assert(lostLinkHost.beginWithPlayerId(7007));
  assert(lostLinkPeer.beginWithPlayerId(8008));
  assert(lostLinkHost.setEnabled(true));
  assert(lostLinkPeer.setEnabled(true));
  lostLinkPeer.transportConnected(false);
  assert(lostLinkHost.requestPeer(8008));
  pump(lostLinkHost, lostLinkPeer);
  lostLinkPeer.answerPeerInvitation(true);
  pump(lostLinkPeer, lostLinkHost);
  assert(lostLinkHost.requestActivity(Multiplayer::Activity::Trade));
  pump(lostLinkHost, lostLinkPeer);
  lostLinkPeer.answerActivity(true);
  pump(lostLinkPeer, lostLinkHost);
  assert(lostLinkHost.submitTradeOffer(pokemon(31, 25, 0x55555555U)));
  pump(lostLinkHost, lostLinkPeer);
  assert(lostLinkPeer.submitTradeOffer(pokemon(32, 133, 0x66666666U)));
  pump(lostLinkPeer, lostLinkHost);
  assert(lostLinkPeer.state() == Multiplayer::State::TradeConfirming);
  lostLinkPeer.transportDisconnected();
  assert(lostLinkPeer.takeUnexpectedDisconnect());
  assert(!lostLinkPeer.takeUnexpectedDisconnect());
  assert(lostLinkPeer.state() == Multiplayer::State::Advertising);
  assert(lostLinkPeer.activity() == Multiplayer::Activity::None);
  assert(!lostLinkPeer.localTradeOffer().valid);
  assert(!lostLinkPeer.remoteTradeOffer().valid);

  // A locally requested close also causes a later platform disconnect
  // callback, but that callback is expected and must not masquerade as a lost
  // peer after the UI has already handled the button/toggle action.
  lowerId.disconnect();
  assert(!lowerId.takeUnexpectedDisconnect());
  lowerId.transportDisconnected();
  assert(!lowerId.takeUnexpectedDisconnect());
  return 0;
}
