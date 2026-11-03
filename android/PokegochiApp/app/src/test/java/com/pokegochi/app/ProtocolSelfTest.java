package com.pokegochi.app;

import java.util.Arrays;

/** No-dependency golden-vector test for the v7 packet ABI. */
public final class ProtocolSelfTest {
    public static void main(String[] args) {
        byte[] advertisement = MultiplayerProtocol.advertisement(0x01020304);
        assertHex(advertisement, "504743480704030201");
        MultiplayerProtocol.Advertisement parsedAdvertisement = MultiplayerProtocol.parseAdvertisement(advertisement);
        check(parsedAdvertisement != null && parsedAdvertisement.playerId == 0x01020304, "advertisement round trip");
        byte[] androidPayload = MultiplayerProtocol.androidManufacturerPayload(0x01020304);
        assertHex(androidPayload, "43480704030201");
        parsedAdvertisement = MultiplayerProtocol.parseManufacturerAdvertisement(
                MultiplayerProtocol.MANUFACTURER_ID, androidPayload);
        check(parsedAdvertisement != null && parsedAdvertisement.playerId == 0x01020304,
                "ESP32 advertisement through Android ScanRecord");
        parsedAdvertisement = MultiplayerProtocol.parseManufacturerAdvertisement(
                MultiplayerProtocol.LEGACY_ANDROID_MANUFACTURER_ID, advertisement);
        check(parsedAdvertisement != null && parsedAdvertisement.playerId == 0x01020304,
                "legacy Android advertisement compatibility");
        check(MultiplayerProtocol.parseManufacturerAdvertisement(0x1234, androidPayload) == null,
                "unrelated manufacturer rejection");

        byte[] packet = MultiplayerProtocol.packet(5, 0x01020304, 0xAABBCCDD, new byte[] {1, 2, 3});
        assertHex(packet, "5047070504030201ddccbbaa0300010203");
        MultiplayerProtocol.Packet parsedPacket = MultiplayerProtocol.parsePacket(packet, 99);
        check(parsedPacket != null && parsedPacket.type == 5 && parsedPacket.sourceId == 0x01020304,
                "packet header round trip");
        check(parsedPacket.transaction == 0xAABBCCDD && Arrays.equals(parsedPacket.payload, new byte[] {1, 2, 3}),
                "packet payload round trip");

        check(MultiplayerProtocol.parsePacket(packet, 0x01020304) == null, "self packet rejection");
        byte[] wrongVersion = packet.clone(); wrongVersion[2] = 5;
        check(MultiplayerProtocol.parsePacket(wrongVersion, 99) == null, "version rejection");
        check(BleTransportPolicy.isIdleProtocolState(BleTransportPolicy.STATE_ADVERTISING),
                "advertising is idle");
        check(BleTransportPolicy.isIdleProtocolState(BleTransportPolicy.STATE_SCANNING),
                "scanning is idle");
        check(!BleTransportPolicy.isIdleProtocolState(BleTransportPolicy.STATE_CONNECTING),
                "connecting is not stale-idle");
        check(BleTransportPolicy.staleLinkGraceMs(false) >= 10_000L,
                "new inbound GATT link gets full handshake window");
        check(BleTransportPolicy.staleLinkGraceMs(true) < 1_000L,
                "closed established session is released promptly");
        check(BleTransportPolicy.canRetryCentral(BleTransportPolicy.STATE_CONNECTING, 1),
                "central connection retries while protocol is connecting");
        check(!BleTransportPolicy.canRetryCentral(BleTransportPolicy.STATE_ADVERTISING, 1),
                "central retry stops after protocol reset");
        check(!BleTransportPolicy.canRetryCentral(BleTransportPolicy.STATE_CONNECTING,
                BleTransportPolicy.MAX_CENTRAL_ATTEMPTS), "central retry is bounded");
        check(BleTransportPolicy.MAX_CENTRAL_ATTEMPTS >= 5,
                "central path survives transient Android GATT failures");
        check(BleTransportPolicy.CENTRAL_ROLE_SETTLE_MS >= 350L &&
                        BleTransportPolicy.CENTRAL_ROLE_SETTLE_MS < 1_000L,
                "central role transition is settled without stalling the handshake");
        check(BleTransportPolicy.GATT_SERVER_TO_CENTRAL_SETTLE_MS >= 200L,
                "peripheral GATT registration is released before connectGatt");
        check(BleTransportPolicy.shouldAbortUnreadyCentral(
                        BleTransportPolicy.STATE_ADVERTISING, false),
                "cancelled unready central link is released immediately");
        check(!BleTransportPolicy.shouldAbortUnreadyCentral(
                        BleTransportPolicy.STATE_CONNECTING, false),
                "active central handshake remains alive");
        check(BleTransportPolicy.shouldReplayHandshake(MultiplayerProtocol.INVITE,
                        BleTransportPolicy.STATE_CONNECTING, 0),
                "outgoing Invite is replayed while waiting for acceptance");
        check(!BleTransportPolicy.shouldReplayHandshake(MultiplayerProtocol.INVITE,
                        BleTransportPolicy.STATE_CONNECTED, 0),
                "Invite replay stops after connection");
        check(BleTransportPolicy.shouldReplayHandshake(MultiplayerProtocol.INVITE_ACCEPT,
                        BleTransportPolicy.STATE_CONNECTED, 0),
                "InviteAccept receives bounded loss recovery");
        check(!BleTransportPolicy.shouldReplayHandshake(MultiplayerProtocol.INVITE_ACCEPT,
                        BleTransportPolicy.STATE_CONNECTED,
                        BleTransportPolicy.MAX_HANDSHAKE_REPLAYS),
                "handshake replay is bounded");
        System.out.println("MultiplayerProtocol v7 discovery and packet vectors: OK");
    }

    private static void assertHex(byte[] actual, String expected) {
        StringBuilder value = new StringBuilder();
        for (byte element : actual) value.append(String.format("%02x", element & 0xff));
        check(value.toString().equals(expected), "expected " + expected + " got " + value);
    }

    private static void check(boolean condition, String label) {
        if (!condition) throw new AssertionError(label);
    }
}
