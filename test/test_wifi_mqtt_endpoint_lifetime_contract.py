from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MULTIPLAYER = ROOT / "src" / "services" / "Multiplayer.cpp"


def test_cloud_mqtt_keeps_a_durable_endpoint_for_reconnects():
    source = MULTIPLAYER.read_text(encoding="utf-8")
    assert "gOnlineMqtt.setServer(onlineHost_, onlinePort_);" in source
    assert "gOnlineMqtt.setServer(relayHost, relayPort);" not in source
