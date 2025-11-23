import math
import json
import threading
import requests
import paho.mqtt.client as mqtt
import customtkinter as ctk
import tkinter as tk  # solo para el Canvas del radar

# ==========================================
# CONFIGURACIÓN (CAMBIA ESTO SI HACE FALTA)
# ==========================================
ESP32_BASE_URL = "http://192.168.1.123"   # IP del ESP32
API_MOVE = "/api/v1/move"
API_HEALTH = "/api/v1/healthcheck"

MQTT_BROKER = "ciber-nexus.loc"            # IP del broker MQTT
MQTT_PORT = 8883                          # Puerto seguro (TLS)
MQTT_TOPIC_DISTANCE = "esp32car/distance"

# Distancias
current_distance_text = "—"
current_distance_value = None     # en cm (float o None)

MAX_RADAR_DISTANCE_CM = 100.0     # rango máximo que quieres dibujar (ajustable)

# ==========================================
# FUNCIONES HTTP
# ==========================================
def move_car(direction: str):
    """Envía un comando de movimiento al ESP32 usando la API REST."""
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


def get_health() -> str:
    """Consulta el estado del ESP32 (healthcheck)."""
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
    """Callback para mensajes MQTT: actualiza la distancia global."""
    global current_distance_text, current_distance_value
    try:
        data = json.loads(msg.payload.decode())
        dist = data.get("distance_cm")
        if dist is None:
            current_distance_text = "—"
            current_distance_value = None
        else:
            current_distance_text = f"{float(dist):.1f} cm"
            current_distance_value = float(dist)
    except Exception as e:
        print("Error parseando MQTT:", e)
        current_distance_text = "error"
        current_distance_value = None


def mqtt_loop():
    """Hilo que mantiene la conexión MQTT y escucha el tópico de distancia."""
    client = mqtt.Client()
    client.on_message = on_message

    # TLS "sencillo" para entorno de laboratorio
    client.tls_set()               # usa certificados del sistema
    client.tls_insecure_set(True)  # no valida hostname (aceptable en laboratorio)

    print(f"Conectando a MQTT {MQTT_BROKER}:{MQTT_PORT} ...")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.subscribe(MQTT_TOPIC_DISTANCE)
    client.loop_forever()


# ==========================================
# GUI (CUSTOMTKINTER + RADAR 2D)
# ==========================================
class CarDashboard(ctk.CTk):
    def __init__(self):
        super().__init__()

        # Apariencia general
        ctk.set_appearance_mode("light")
        ctk.set_default_color_theme("blue")

        self.title("Control del Carro IoT")
        self.geometry("500x450")
        self.resizable(False, False)

        # --------- Layout principal ----------
        main_frame = ctk.CTkFrame(self)
        main_frame.pack(fill="both", expand=True, padx=10, pady=10)

        # Estado ESP32
        self.lbl_status = ctk.CTkLabel(
            main_frame,
            text="Estado ESP32: —",
            font=("Segoe UI", 14)
        )
        self.lbl_status.pack(pady=(5, 0))

        # Distancia textual
        self.lbl_distance = ctk.CTkLabel(
            main_frame,
            text="Distancia: —",
            font=("Segoe UI", 22, "bold")
        )
        self.lbl_distance.pack(pady=(10, 10))

        # Frame para radar + controles
        center_frame = ctk.CTkFrame(main_frame)
        center_frame.pack(fill="both", expand=True, pady=5)

        # --------- Radar 2D ----------
        radar_frame = ctk.CTkFrame(center_frame)
        radar_frame.grid(row=0, column=0, padx=(10, 10), pady=5, sticky="n")

        radar_label = ctk.CTkLabel(
            radar_frame,
            text="Radar ultrasónico",
            font=("Segoe UI", 13, "bold")
        )
        radar_label.pack(pady=(5, 0))

        self.radar_width = 480
        self.radar_height = 340

        # Usamos un Canvas de tkinter dentro del frame
        self.radar_canvas = tk.Canvas(
            radar_frame,
            width=self.radar_width,
            height=self.radar_height,
            bg="#111827",        # gris oscuro
            highlightthickness=0
        )
        self.radar_canvas.pack(pady=5)

        self._draw_radar_static()

        # --------- Controles de movimiento ----------
        controls_frame = ctk.CTkFrame(center_frame)
        controls_frame.grid(row=0, column=1, pady=5, sticky="n")

        controls_label = ctk.CTkLabel(
            controls_frame,
            text="Controles",
            font=("Segoe UI", 13, "bold")
        )
        controls_label.pack(pady=(5, 10))

        grid = ctk.CTkFrame(controls_frame, fg_color="transparent")
        grid.pack()

        btn_up = ctk.CTkButton(
            grid, text="↑", width=60, height=45,
            command=lambda: move_car("A")
        )
        btn_up.grid(row=0, column=1, pady=5)

        btn_left = ctk.CTkButton(
            grid, text="←", width=60, height=45,
            command=lambda: move_car("I")
        )
        btn_left.grid(row=1, column=0, padx=5)

        btn_stop = ctk.CTkButton(
            grid, text="STOP",
            fg_color="#8126DC", hover_color="#601CB9",
            width=60, height=45,
            command=lambda: move_car("S")
        )
        btn_stop.grid(row=1, column=1, padx=5, pady=5)

        btn_right = ctk.CTkButton(
            grid, text="→", width=60, height=45,
            command=lambda: move_car("D")
        )
        btn_right.grid(row=1, column=2, padx=5)

        btn_down = ctk.CTkButton(
            grid, text="↓", width=60, height=45,
            command=lambda: move_car("B")
        )
        btn_down.grid(row=2, column=1, pady=5)

        # Timers de actualización
        self.after(500, self._update_status_label)
        self.after(200, self._update_distance_and_radar)

    # ---------- Radar: dibujo estático ----------
    def _draw_radar_static(self):
        """Dibuja la base del radar (arcos y líneas) una sola vez."""
        cw = self.radar_width
        ch = self.radar_height

        # Fondo ya es oscuro, ahora dibujamos arcos verdes
        center_x = cw / 2
        center_y = ch   # origen en la parte de abajo

        # Líneas guía (triángulo de apertura)
        self.radar_canvas.create_line(
            center_x, center_y,
            center_x - 90, center_y - 150,
            fill="#374151"
        )
        self.radar_canvas.create_line(
            center_x, center_y,
            center_x + 90, center_y - 150,
            fill="#374151"
        )

        # Arcos de distancia (tres radios)
        for i in range(1, 4):
            r = (i / 3.0) * 140
            self.radar_canvas.create_arc(
                center_x - r, center_y - r,
                center_x + r, center_y + r,
                start=180-50, extent=100,
                style="arc", outline="#4B5563"
            )

        # Base (robot)
        self.radar_canvas.create_rectangle(
            center_x - 12, center_y - 8,
            center_x + 12, center_y,
            fill="#6B7280", outline=""
        )

    # ---------- Radar: dibujo dinámico ----------
    def _draw_radar_obstacle(self, distance_cm):
        """Dibuja el obstáculo según la distancia (en cm)."""
        cw = self.radar_width
        ch = self.radar_height
        center_x = cw / 2
        center_y = ch

        # Borramos cualquier obstáculo previo
        self.radar_canvas.delete("obstacle")

        if distance_cm is None or distance_cm <= 0:
            return

        # Limitar al máximo que queremos mostrar
        d = min(distance_cm, MAX_RADAR_DISTANCE_CM)

        # Mapear distancia a radio (pixeles)
        max_radius = 140.0
        radius = (d / MAX_RADAR_DISTANCE_CM) * max_radius

        # Punto en la línea central vertical (vector hacia arriba)
        angle_rad = math.radians(90)   # 90° desde el eje horizontal -> hacia arriba
        x = center_x + radius * math.cos(angle_rad)
        y = center_y - radius * math.sin(angle_rad)

        r_point = 5  # radio del puntico
        self.radar_canvas.create_oval(
            x - r_point, y - r_point,
            x + r_point, y + r_point,
            fill="#22C55E", outline="",
            tags="obstacle"
        )

    # ---------- Actualizaciones periódicas ----------
    def _update_status_label(self):
        status = get_health()
        self.lbl_status.configure(text=f"Estado ESP32: {status}")
        self.after(4000, self._update_status_label)

    def _update_distance_and_radar(self):
        self.lbl_distance.configure(text=f"Distancia: {current_distance_text}")
        self._draw_radar_obstacle(current_distance_value)
        self.after(200, self._update_distance_and_radar)


# ==========================================
# MAIN
# ==========================================
if __name__ == "__main__":
    # Hilo para MQTT (no bloquea la GUI)
    t = threading.Thread(target=mqtt_loop, daemon=True)
    t.start()

    app = CarDashboard()
    app.mainloop()
