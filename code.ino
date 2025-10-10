#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
//#include "driver/ledc.h"  // ❌ Desactivamos PWM por hardware temporalmente

// ========= CONFIG WI-FI =========
const char* WIFI_SSID = "ASULMOVIS";
const char* WIFI_PASS = "Asul23alma";

// ========= CONFIG MQTT ==========
const char* MQTT_BROKER = "192.168.1.101";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_CLIENT_ID = "esp32_car";
const char* MQTT_TOPIC = "esp32car/http";

// ========= PINES MOTORES =========
const int ENA = 25;
const int IN1 = 26;
const int IN2 = 27;
const int ENB = 33;
const int IN3 = 32;
const int IN4 = 14;

// ========= PINES LEDS ===========
const int LED_IZQ  = 18;
const int LED_DER  = 23;
const int LED_STOP = 19;
const int LED_REV  = 22;
const int LED_AZULES = 21;

// ========= LED ACTIVO ===========
const bool LED_ACTIVE_HIGH = true;

// ========= PWM =========
const int PWM_FREQ_HZ  = 20000;
const int PWM_RES_BITS = 10;
const int PWM_MAX      = (1 << PWM_RES_BITS) - 1;
// const int PWM_CH_A     = 0;
// const int PWM_CH_B     = 1;

// ========= CONTROL =========
int velDefault = 800;
const int STEP = 50;
char lastMotion = 'S';

// ========= BLINK REVERSA =========
const unsigned long REV_BLINK_MS = 250;
unsigned long revLastToggle = 0;
bool revBlinkState = false;

// ========= AUTOSTOP =========
unsigned long activeMotionUntil = 0;

// ========= SERVIDORES =========
WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ========= UTILS =========
int clampDuty(int x){ return x<0?0:(x>PWM_MAX?PWM_MAX:x); }

inline void ledWrite(int pin, bool on){
  digitalWrite(pin, (LED_ACTIVE_HIGH ? (on?HIGH:LOW) : (on?LOW:HIGH)));
}

void setLeds(bool izq, bool der, bool stop){
  ledWrite(LED_IZQ,  izq);
  ledWrite(LED_DER,  der);
  ledWrite(LED_STOP, stop);
}

inline void setReverseLed(bool on){ ledWrite(LED_REV, on); }
inline void setBlue(bool on){ ledWrite(LED_AZULES, on); }

inline void figureStart(){ setBlue(true); }
inline void figureEnd(){ setBlue(false); }

void setDireccion(bool adelanteA, bool adelanteB){
  digitalWrite(IN1, adelanteA ? HIGH : LOW);
  digitalWrite(IN2, adelanteA ? LOW  : HIGH);
  digitalWrite(IN3, adelanteB ? HIGH : LOW);
  digitalWrite(IN4, adelanteB ? LOW  : HIGH);
}

// 🧩 Reemplazo temporal de ledcWrite con analogWrite
void setVelocidades(int dutyA, int dutyB){
  dutyA = clampDuty(dutyA);
  dutyB = clampDuty(dutyB);
  analogWrite(ENA, dutyA);
  analogWrite(ENB, dutyB);
}

void parar(){
  setVelocidades(0,0);
  setLeds(false, false, true);
  setReverseLed(false);
  setBlue(false);
  lastMotion = 'S';
  activeMotionUntil = 0;
}

void adelante(int vel){
  setDireccion(true,true);
  setVelocidades(vel, vel);
  setLeds(false, false, false);
  if(lastMotion!='B') setReverseLed(false);
  lastMotion = 'A';
}

void atras(int vel){
  setDireccion(false,false);
  setVelocidades(vel, vel);
  setLeds(true, true, false);
  revBlinkState = true;
  setReverseLed(true);
  revLastToggle = millis();
  lastMotion = 'B';
}

void izquierda(int vel){
  setDireccion(true,true);
  setVelocidades(vel/2, vel);
  setLeds(true, false, false);
  if(lastMotion!='B') setReverseLed(false);
  lastMotion = 'I';
}

void derecha(int vel){
  setDireccion(true,true);
  setVelocidades(vel, vel/2);
  setLeds(false, true, false);
  if(lastMotion!='B') setReverseLed(false);
  lastMotion = 'D';
}

void spinRight(int vel){
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  setVelocidades(vel, vel);
  setLeds(false, true, false);
  delay(600);
  parar();
}

void zigzag(int vel, int seg_ms, int rep){
  figureStart();
  for(int k=0;k<rep;k++){
    izquierda(vel); delay(seg_ms);
    derecha(vel);   delay(seg_ms);
  }
  figureEnd();
  parar();
}

void squarePath(int vel, int lado_ms){
  figureStart();
  for(int i=0;i<4;i++){
    adelante(vel); delay(lado_ms);
    spinRight(PWM_MAX);
  }
  figureEnd();
  parar();
}

void switchDirectionR() {
  figureStart();
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  setVelocidades(PWM_MAX, PWM_MAX);
  setLeds(false, true, false);
  delay(900);
  figureEnd();
  parar();
}

void updateReverseBlink(){
  if(lastMotion=='B'){
    unsigned long now = millis();
    if(now - revLastToggle >= REV_BLINK_MS){
      revLastToggle = now;
      revBlinkState = !revBlinkState;
      setReverseLed(revBlinkState);
    }
  }else{
    if(revBlinkState){ revBlinkState = false; }
    setReverseLed(false);
  }
}

// ========= MQTT =========
void ensureMqtt() {
  if (mqtt.connected()) return;
  for (int i=0; i<3 && !mqtt.connected(); ++i) {
    mqtt.connect(MQTT_CLIENT_ID);
    delay(250);
  }
}

void publishMoveToMqtt(const String& dir, int speed, uint32_t durationMs, const String& clientIp) {
  ensureMqtt();
  if (!mqtt.connected()) return;
  String payload = "{";
  payload += "\"dir\":\"" + dir + "\",";
  payload += "\"speed\":" + String(speed) + ",";
  payload += "\"duration_ms\":" + String(durationMs) + ",";
  payload += "\"client_ip\":\"" + clientIp + "\",";
  payload += "\"esp32_ip\":\"" + WiFi.localIP().toString() + "\"";
  payload += "}";
  mqtt.publish(MQTT_TOPIC, payload.c_str());
}

// ========= HTTP HANDLERS =========
void handleStatus() {
  // remaining ms
  long remaining = 0;
  if (activeMotionUntil > 0) {
    long now = (long)millis();
    remaining = (long)activeMotionUntil - now;
    if (remaining < 0) remaining = 0;
  }
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"lastMotion\":\"" + String(lastMotion) + "\",";
  json += "\"velDefault\":" + String(velDefault) + ",";
  json += "\"moving\":" + String(lastMotion!='S' ? "true":"false") + ",";
  json += "\"remaining_ms\":" + String(remaining) + ",";
  json += "\"esp32_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleMove() {
  // Parámetros: dir = A|B|I|D|S, speed (0..1023), duration_ms (1..5000)
  if (!server.hasArg("dir")) {
    server.send(400, "application/json", "{\"error\":\"Parametro 'dir' requerido (A|B|I|D|S)\"}");
    return;
  }

  String dir = server.arg("dir");
  dir.toUpperCase();

  int speed = velDefault;
  if (server.hasArg("speed")) {
    speed = clampDuty(server.arg("speed").toInt());
  }

  uint32_t durationMs = 1000; // por defecto 1s
  if (server.hasArg("duration_ms")) {
    durationMs = (uint32_t) server.arg("duration_ms").toInt();
  }
  if (durationMs == 0) durationMs = 1;
  if (durationMs > 5000) durationMs = 5000; // límite de seguridad

  // Ejecutar movimiento
  if      (dir == "A") adelante(speed);
  else if (dir == "B") atras(speed);
  else if (dir == "I") izquierda(speed);
  else if (dir == "D") derecha(speed);
  else if (dir == "S") parar();
  else {
    server.send(400, "application/json", "{\"error\":\"dir invalido. Use A|B|I|D|S\"}");
    return;
  }
  // Programar autostop (si no es S)
  if (dir != "S") {
    activeMotionUntil = millis() + durationMs;
  } else {
    activeMotionUntil = 0;
  }

  // IP del cliente
  String clientIp = server.client().remoteIP().toString();

  // Publicar en MQTT
  publishMoveToMqtt(dir, speed, durationMs, clientIp);

  // Respuesta
  String json = "{";
  json += "\"ok\":true,";
  json += "\"dir\":\"" + dir + "\",";
  json += "\"speed\":" + String(speed) + ",";
  json += "\"duration_ms\":" + String(durationMs) + ",";
  json += "\"client_ip\":\"" + clientIp + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ========= WIFI =========
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-start < 20000) {
    delay(250);
  }
}

void setup(){
  Serial.begin(115200);

  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT);
  pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  pinMode(LED_IZQ, OUTPUT);
  pinMode(LED_DER, OUTPUT);
  pinMode(LED_STOP, OUTPUT);
  pinMode(LED_REV, OUTPUT);
  pinMode(LED_AZULES, OUTPUT);

  setLeds(false,false,false);
  setReverseLed(false);
  setBlue(false);

  // ❌ PWM desactivado temporalmente
  // ledcSetup(PWM_CH_A, PWM_FREQ_HZ, PWM_RES_BITS);
  // ledcSetup(PWM_CH_B, PWM_FREQ_HZ, PWM_RES_BITS);
  // ledcAttachPin(ENA, PWM_CH_A);
  // ledcAttachPin(ENB, PWM_CH_B);
  setVelocidades(0,0);

  connectWiFi();
  Serial.println(WiFi.localIP());

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);

  // MQTT
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);

  // HTTP server
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/move",   HTTP_POST, handleMove);
  server.on("/health", HTTP_GET, [](){ server.send(200, "application/json", "{\"status\":\"ok\"}"); });
  server.onNotFound([](){ server.send(404, "application/json", "{\"error\":\"Not found\"}"); });
  server.begin();

  parar();

}

void loop(){
  server.handleClient();
  mqtt.loop();

  if (activeMotionUntil > 0 && millis() >= activeMotionUntil) {
    parar();
  }
  updateReverseBlink();
}
