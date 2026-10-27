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
