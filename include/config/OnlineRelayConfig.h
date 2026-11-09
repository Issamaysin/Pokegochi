#pragma once

#include <cstdint>

// Internet transport for the home ESP32 relay.  The application protocol and
// room authority still run on that ESP32; the public MQTT service is only an
// outbound message path that crosses carrier NAT/CGNAT.  A long deployment
// namespace prevents unrelated public-broker users from sharing our topics.
namespace PokegochiOnlineRelayConfig {
inline constexpr char kBrokerHost[] = "broker.emqx.io";
inline constexpr uint16_t kBrokerPort = 1883;
inline constexpr char kTopicRoot[] =
    "pokegochi/online/v1/8f4d1bd86a58468fbbcf120a6f63bd79";
inline constexpr uint32_t kAccessCode = 579856U;
inline constexpr uint16_t kMqttBufferBytes = 512U;
inline constexpr uint16_t kMqttKeepAliveSeconds = 20U;
}  // namespace PokegochiOnlineRelayConfig
