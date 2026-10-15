#include <cassert>
#include <cstdint>

#include "services/Multiplayer.h"

void pump(Multiplayer& from, Multiplayer& to) {
  uint8_t bytes[174]{};
  uint16_t length = 0;
  while (from.takeOutgoingPacket(bytes, length)) to.transportPacket(bytes, length);
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
  return 0;
}
