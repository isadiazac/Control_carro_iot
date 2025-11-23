import customtkinter as ctk
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

MQTT_BROKER = "192.168.84.252"  
MQTT_PORT = 8883              
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
    except Exception:
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

    # Si tu broker usa TLS, activa esta línea:
    client.tls_set()  
    client.tls_insecure_set(True)

    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(MQTT_TOPIC_DISTANCE)
    client.loop_forever()

# ==========================================
# GUI (CUSTOMTKINTER)
# ==========================================
def start_gui():
    ctk.set_appearance_mode("light")
    ctk.set_default_color_theme("blue")

    root = ctk.CTk()
    root.title("Control del Carro IoT")
    root.geometry("350x420")

    # Estado ESP32
    lbl_status = ctk.CTkLabel(root, text="Estado ESP32: —", font=("Segoe UI", 14))
    lbl_status.pack(pady=10)

    def update_status():
        status = get_health()
        lbl_status.configure(text=f"Estado ESP32: {status}")
        root.after(4000, update_status)

    # Distancia desde MQTT
    lbl_distance = ctk.CTkLabel(root, text="Distancia: —",
                                font=("Segoe UI", 28, "bold"))
    lbl_distance.pack(pady=20)

    def update_distance():
        lbl_distance.configure(text=f"Distancia: {current_distance}")
        root.after(300, update_distance)

    # Contenedor de botones
    frame = ctk.CTkFrame(root)
    frame.pack(pady=15)

    btn_up = ctk.CTkButton(frame, text="↑", width=60, height=45,
                           command=lambda: move_car("A"))
    btn_up.grid(row=0, column=1, pady=5)

    btn_left = ctk.CTkButton(frame, text="←", width=60, height=45,
                             command=lambda: move_car("I"))
    btn_left.grid(row=1, column=0, padx=5)

    btn_stop = ctk.CTkButton(frame, text="STOP", fg_color="purple",
                             hover_color="#800080",
                             width=60, height=45,
                             command=lambda: move_car("S"))
    btn_stop.grid(row=1, column=1, padx=5)

    btn_right = ctk.CTkButton(frame, text="→", width=60, height=45,
                              command=lambda: move_car("D"))
    btn_right.grid(row=1, column=2, padx=5)

    btn_down = ctk.CTkButton(frame, text="↓", width=60, height=45,
                             command=lambda: move_car("B"))
    btn_down.grid(row=2, column=1, pady=5)

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
