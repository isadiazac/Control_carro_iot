import paho.mqtt.client as mqtt
import json

BROKER = "10.238.12.138"  # IP del broker MQTT
PORT = 1883
TOPIC = "esp32car/distance"

def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())
        distance = data.get("distance_cm")
        ip = data.get("esp32_ip")
        print(f"ESP32 {ip} -> Distancia: {distance:.2f} cm")
    except Exception as e:
        print(f"Error al procesar mensaje: {e}")

client = mqtt.Client()
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.subscribe(TOPIC)

print(f"Suscrito al tópico {TOPIC}. Esperando datos...\\n")

try:
    client.loop_forever()
except KeyboardInterrupt:
    print("\\nDesconectando...")
    client.disconnect()