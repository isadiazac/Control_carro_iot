import time
import json
import paho.mqtt.client as mqtt

BROKER = "ciber-nexus.loc"   # cambia si tu broker tiene otra IP
PORT = 8883                 # igual que en tu GUI
TOPIC = "esp32car/distance"

client = mqtt.Client()

# TLS sencillo (igual que en tu GUI)
client.tls_set()
client.tls_insecure_set(True)

print("Conectando al broker MQTT...")
client.connect(BROKER, PORT, 60)
print("Conectado. Enviando distancias falsas...")

# Animación: distancia sube de 10cm a 120cm y baja
while True:
    # Subiendo de 10 a 120 cm
    for d in range(10, 120, 5):
        payload = {"distance_cm": d}
        client.publish(TOPIC, json.dumps(payload))
        time.sleep(0.15)

    # Bajando de 120 a 10 cm
    for d in range(120, 10, -5):
        payload = {"distance_cm": d}
        client.publish(TOPIC, json.dumps(payload))
        time.sleep(0.15)
