import paho.mqtt.client as mqtt
import ssl
import json

BROKER = "ciber-nexus.loc"
PORT = 8883
TOPIC = "esp32car/#"

def on_connect(client, userdata, flags, rc):
    print("Conectado rc =", rc)
    client.subscribe(TOPIC)
    print("Suscrito a:", TOPIC)

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        d = data.get("distance_cm")
        ip = data.get("esp32_ip")
        print(f"➡ ESP32 {ip} → Distancia: {d} cm")
    except Exception as e:
        print("Error:", e)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

# SELF-SIGNED, así que NO usamos CA_CERTS 
client.tls_set(
    ca_certs=None,
    certfile=None,
    keyfile=None,
    tls_version=ssl.PROTOCOL_TLSv1_2
)

# Permitir CN self-signed 
client.tls_insecure_set(True)

print("TLS configurado (modo permisivo para self-signed)")

client.connect(BROKER, PORT, 60)

print("Esperando mensajes...")
client.loop_forever()