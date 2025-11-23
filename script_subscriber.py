import paho.mqtt.client as mqtt
import ssl
import json

BROKER = "10.238.12.138"
PORT = 8883
TOPIC = "esp32car/distance"
CA_PATH = "C:/Users/isadi/OneDrive/Documentos/2025-2/Infra/certificado_dominio_local.crt" 

def on_connect(client, userdata, flags, rc):
    print("Connected rc=", rc)
    client.subscribe(TOPIC)

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        d = data.get("distance_cm")
        ip = data.get("esp32_ip")
        print(f"ESP32 {ip} -> Distancia: {d}")
    except Exception as e:
        print("Error:", e)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

# Forzar TLS con el CA local
client.tls_set(ca_certs=CA_PATH, tls_version=ssl.PROTOCOL_TLSv1_2)
client.tls_insecure_set(False)  # True = no validar CN; False = validar

client.connect(BROKER, PORT, 60)
client.loop_forever()