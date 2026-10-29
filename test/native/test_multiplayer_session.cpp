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

  assert(app.requestActivity(Multiplayer::Activity::Battle));
  pump(app, device);
  assert(device.state() == Multiplayer::State::ConfirmIncomingActivity);
  device.answerActivity(true);
  pump(device, app);
  assert(app.activity() == Multiplayer::Activity::Battle);
  assert(device.activity() == Multiplayer::Activity::Battle);
  assert(app.state() == Multiplayer::State::BattlePreparing);
  assert(device.state() == Multiplayer::State::BattlePreparing);

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
  return 0;
}
