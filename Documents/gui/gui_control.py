import tkinter as tk
from tkinter import ttk
import threading
import requests
import json
import paho.mqtt.client as mqtt

# ==========================================
# CONFIGURACIÓN (CAMBIAR ESTO)
# ==========================================
ESP32_BASE_URL = "http://192.168.1.123"  # Cambiar por IP real del ESP32
API_MOVE = "/api/v1/move"
API_HEALTH = "/api/v1/healthcheck"

MQTT_BROKER = "10.238.12.138"  # Cambiar por IP del broker
MQTT_PORT = 1883               # 8883 si usas TLS
MQTT_TOPIC_DISTANCE = "esp32car/distance"

current_distance = "—"


# ==========================================
# FUNCIONES HTTP
# ==========================================
def move_car(direction):
    params = {
        "dir": direction,
        "speed": 800,
        "duration_ms": 1000
    }
    try:
        url = ESP32_BASE_URL + API_MOVE
        r = requests.post(url, params=params, timeout=2)
        print("RESPUESTA MOVE:", r.status_code, r.text)
    except Exception as e:
        print("Error al mover:", e)


def get_health():
    try:
        url = ESP32_BASE_URL + API_HEALTH
        r = requests.get(url, timeout=2)
        return r.json().get("status", "desconocido")
    except Exception as e:
        return "error"


# ==========================================
# MQTT CALLBACKS
# ==========================================
def on_message(client, userdata, msg):
    global current_distance
    try:
        data = json.loads(msg.payload.decode())
        dist = data.get("distance_cm")
        if dist is None:
            current_distance = "—"
        else:
            current_distance = f"{dist:.1f} cm"
    except:
        current_distance = "error"


def mqtt_loop():
    client = mqtt.Client()
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(MQTT_TOPIC_DISTANCE)
    client.loop_forever()


# ==========================================
# GUI
# ==========================================
def start_gui():
    root = tk.Tk()
    root.title("Control del Carro IoT")
    root.geometry("300x350")

    # Estado del ESP32
    lbl_status = ttk.Label(root, text="Estado ESP32: —")
    lbl_status.pack(pady=5)

    def update_status():
        status = get_health()
        lbl_status.config(text=f"Estado ESP32: {status}")
        root.after(4000, update_status)

    # Distancia desde MQTT
    lbl_distance = ttk.Label(root, text="Distancia: —", font=("Arial", 16))
    lbl_distance.pack(pady=15)

    def update_distance():
        lbl_distance.config(text=f"Distancia: {current_distance}")
        root.after(300, update_distance)

    # Botonera de control
    frame = tk.Frame(root)
    frame.pack()

    ttk.Button(frame, text="↑", width=6,
               command=lambda: move_car("A")).grid(row=0, column=1, pady=5)

    ttk.Button(frame, text="←", width=6,
               command=lambda: move_car("I")).grid(row=1, column=0, padx=5)

    ttk.Button(frame, text="→", width=6,
               command=lambda: move_car("D")).grid(row=1, column=2, padx=5)

    ttk.Button(frame, text="↓", width=6,
               command=lambda: move_car("B")).grid(row=2, column=1, pady=5)

    ttk.Button(frame, text="STOP", width=6,
               command=lambda: move_car("S")).grid(row=1, column=1, pady=10)

    update_status()
    update_distance()

    root.mainloop()


# ==========================================
# MAIN
# ==========================================
if __name__ == "__main__":
    t = threading.Thread(target=mqtt_loop, daemon=True)
    t.start()
    start_gui()
