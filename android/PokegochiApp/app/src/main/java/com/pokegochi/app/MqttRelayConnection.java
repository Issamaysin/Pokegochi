package com.pokegochi.app;

import java.io.ByteArrayOutputStream;
import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.ArrayDeque;

/** Minimal MQTT 3.1.1/QoS-0 tunnel for the PGON frames. */
final class MqttRelayConnection implements AutoCloseable {
    static final String BROKER_HOST = "broker.emqx.io";
    static final int BROKER_PORT = 1883;
    static final String TOPIC_ROOT =
            "pokegochi/online/v1/8f4d1bd86a58468fbbcf120a6f63bd79";

    private final ArrayDeque<byte[]> incoming = new ArrayDeque<>();
    private Socket socket;
    private InputStream input;
    private OutputStream output;
    private String publishTopic;
    private int control = -1;
    private int remainingLength;
    private int remainingMultiplier = 1;
    private byte[] packet;
    private int packetRead;

    boolean connect(int playerId) throws IOException {
        close();
        int nonce = new SecureRandom().nextInt();
        if (nonce == 0) nonce = 1;
        String route = String.format("%05d/%08x", playerId, nonce);
        publishTopic = TOPIC_ROOT + "/server/" + route;
        String subscribeTopic = TOPIC_ROOT + "/client/" + route;
        String clientId = "pga-" + route.replace('/', '-');

        Socket opened = new Socket();
        opened.setTcpNoDelay(true);
        opened.connect(new InetSocketAddress(BROKER_HOST, BROKER_PORT), 5000);
        opened.setSoTimeout(5000);
        socket = opened;
        input = opened.getInputStream();
        output = opened.getOutputStream();

        ByteArrayOutputStream variable = new ByteArrayOutputStream();
        writeUtf8(variable, "MQTT");
        variable.write(4);             // MQTT 3.1.1
        variable.write(2);             // clean session
        variable.write(0);
        variable.write(20);            // keepalive seconds
        writeUtf8(variable, clientId);
        writePacket(0x10, variable.toByteArray());
        byte[] connack = readBlockingPacket(0x20);
        if (connack.length != 2 || connack[1] != 0)
            throw new IOException("MQTT CONNACK rejected");

        ByteArrayOutputStream subscribe = new ByteArrayOutputStream();
        subscribe.write(0);
        subscribe.write(1);             // packet id
        writeUtf8(subscribe, subscribeTopic);
        subscribe.write(0);             // QoS 0
        writePacket(0x82, subscribe.toByteArray());
        byte[] suback = readBlockingPacket(0x90);
        if (suback.length < 3 || suback[2] == (byte) 0x80)
            throw new IOException("MQTT SUBACK rejected");
        opened.setSoTimeout(0);
        return true;
    }

    boolean isConnected() {
        return socket != null && socket.isConnected() && !socket.isClosed();
    }

    void publish(byte[] payload) throws IOException {
        if (!isConnected() || publishTopic == null) throw new IOException("MQTT offline");
        ByteArrayOutputStream body = new ByteArrayOutputStream();
        writeUtf8(body, publishTopic);
        body.write(payload, 0, payload.length);
        writePacket(0x30, body.toByteArray());
    }

    void ping() throws IOException {
        if (!isConnected()) throw new IOException("MQTT offline");
        output.write(new byte[]{(byte) 0xC0, 0});
        output.flush();
    }

    void poll() throws IOException {
        while (isConnected() && input.available() > 0) {
            if (control < 0) {
                control = input.read();
                if (control < 0) throw new EOFException("MQTT closed");
                remainingLength = 0;
                remainingMultiplier = 1;
            }
            if (packet == null) {
                while (input.available() > 0) {
                    int encoded = input.read();
                    if (encoded < 0) throw new EOFException("MQTT closed");
                    remainingLength += (encoded & 127) * remainingMultiplier;
                    if ((encoded & 128) == 0) {
                        if (remainingLength > 1024) throw new IOException("MQTT packet too large");
                        packet = new byte[remainingLength];
                        packetRead = 0;
                        break;
                    }
                    remainingMultiplier *= 128;
                    if (remainingMultiplier > 128 * 128 * 128)
                        throw new IOException("invalid MQTT length");
                }
                if (packet == null) return;
            }
            while (packetRead < packet.length && input.available() > 0) {
                int count = input.read(packet, packetRead, packet.length - packetRead);
                if (count < 0) throw new EOFException("MQTT closed");
                packetRead += count;
            }
            if (packetRead < packet.length) return;
            handlePacket(control, packet);
            control = -1;
            packet = null;
            packetRead = 0;
        }
    }

    byte[] takeIncoming() {
        return incoming.poll();
    }

    private void handlePacket(int header, byte[] body) throws IOException {
        if ((header & 0xF0) != 0x30) return;
        if (body.length < 2) throw new IOException("short MQTT publish");
        int topicLength = ((body[0] & 0xFF) << 8) | (body[1] & 0xFF);
        int payloadOffset = 2 + topicLength;
        if (payloadOffset > body.length) throw new IOException("bad MQTT topic");
        byte[] payload = new byte[body.length - payloadOffset];
        System.arraycopy(body, payloadOffset, payload, 0, payload.length);
        incoming.add(payload);
    }

    private byte[] readBlockingPacket(int expectedType) throws IOException {
        int header = input.read();
        if (header < 0 || (header & 0xF0) != expectedType)
            throw new IOException("unexpected MQTT control packet");
        int multiplier = 1;
        int length = 0;
        int encoded;
        do {
            encoded = input.read();
            if (encoded < 0) throw new EOFException("MQTT closed");
            length += (encoded & 127) * multiplier;
            multiplier *= 128;
        } while ((encoded & 128) != 0 && multiplier <= 128 * 128 * 128);
        byte[] body = new byte[length];
        int offset = 0;
        while (offset < length) {
            int count = input.read(body, offset, length - offset);
            if (count < 0) throw new EOFException("MQTT closed");
            offset += count;
        }
        return body;
    }

    private void writePacket(int header, byte[] body) throws IOException {
        output.write(header);
        int length = body.length;
        do {
            int encoded = length % 128;
            length /= 128;
            if (length > 0) encoded |= 128;
            output.write(encoded);
        } while (length > 0);
        output.write(body);
        output.flush();
    }

    private static void writeUtf8(ByteArrayOutputStream output, String value) {
        byte[] bytes = value.getBytes(StandardCharsets.UTF_8);
        output.write((bytes.length >>> 8) & 0xFF);
        output.write(bytes.length & 0xFF);
        output.write(bytes, 0, bytes.length);
    }

    @Override public void close() {
        Socket closing = socket;
        socket = null;
        input = null;
        output = null;
        publishTopic = null;
        incoming.clear();
        control = -1;
        packet = null;
        packetRead = 0;
        if (closing != null) try { closing.close(); } catch (IOException ignored) {}
    }
}
