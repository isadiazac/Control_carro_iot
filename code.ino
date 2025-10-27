// ====== esp32_car.ino ======
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include "config.h"

// ========= GLOBALS ==========
WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// Control
int velDefault = 800;
const int STEP = 50;
char lastMotion = 'S';

// Blink
unsigned long revLastToggle = 0;
bool revBlinkState = false;

// Autostop
unsigned long activeMotionUntil = 0;

// Sensor publish timer
unsigned long lastSensorPublish = 0;

// Utils
int clampDuty(int x){ return x<0?0:(x>PWM_MAX?PWM_MAX:x); }

inline void ledWrite(int pin, bool on){ digitalWrite(pin, (on?HIGH:LOW)); }

void setLeds(bool izq, bool der, bool stop){ ledWrite(LED_IZQ, izq); ledWrite(LED_DER, der); ledWrite(LED_STOP, stop); }
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

void adelante(int vel){ setDireccion(true,true); setVelocidades(vel, vel); setLeds(false,false,false); if(lastMotion!='B') setReverseLed(false); lastMotion='A'; }
void atras(int vel){ setDireccion(false,false); setVelocidades(vel, vel); setLeds(true,true,false); revBlinkState=true; setReverseLed(true); revLastToggle=millis(); lastMotion='B'; }
void izquierda(int vel){ setDireccion(true,true); setVelocidades(vel/2, vel); setLeds(true,false,false); if(lastMotion!='B') setReverseLed(false); lastMotion='I'; }
void derecha(int vel){ setDireccion(true,true); setVelocidades(vel, vel/2); setLeds(false,true,false); if(lastMotion!='B') setReverseLed(false); lastMotion='D'; }

void spinRight(int vel){
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);
  setVelocidades(vel, vel);
  setLeds(false, true, false);
  delay(600);
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
  } else {
    if(revBlinkState){ revBlinkState = false; }
    setReverseLed(false);
  }
}

// ===== MQTT helpers =====
void ensureMqtt() {
  if (mqtt.connected()) return;
  for (int i=0;i<3 && !mqtt.connected(); ++i){
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
  mqtt.publish(MQTT_TOPIC_MOVE, payload.c_str());
}

// ===== HC-SR04 reading (simulator OR physical) =====
// Function signature: no arguments, returns float (distance in cm). If sensor
// not available returns -1.
float readUltrasonic(){
#if USE_HCSR04 == 0
  // Simulator: produce a plausible distance between 5.0 cm and 300.0 cm
  long r = random(500, 30001); // 500 -> 30000 (0.5 m .. 300.00 m hundredths)
  float d = r / 100.0;
  return d;
#else
  // Physical sensor implementation. Ensure ECHO is protected with a
  // voltage divider (5V -> 3.3V) before connecting to ESP32.
  digitalWrite(HCSR04_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HCSR04_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HCSR04_TRIG_PIN, LOW);
  unsigned long duration = pulseIn(HCSR04_ECHO_PIN, HIGH, 30000UL); // timeout 30ms
  if(duration == 0) return -1.0; // no echo / out of range
  float distance_cm = (duration * 0.0343) / 2.0; // speed of sound
  if(distance_cm > HCSR04_MAX_CM) return -1.0;
  return distance_cm;
#endif
}

void publishDistanceToMqtt(float d){
  ensureMqtt();
  if(!mqtt.connected()) return;
  String payload = "{";
  if(d < 0){
    payload += "\"distance_cm\":null,";
  } else {
    payload += "\"distance_cm\":" + String(d, 2) + ",";
  }
  payload += "\"esp32_ip\":\"" + WiFi.localIP().toString() + "\"";
  payload += "}";
  mqtt.publish(MQTT_TOPIC_SENSOR, payload.c_str());
}

// ===== HTTP handlers (same as before) =====
void handleStatus() {
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
  if (!server.hasArg("dir")) { server.send(400, "application/json", "{\"error\":\"Parametro 'dir' requerido (A|B|I|D|S)\"}"); return; }
  String dir = server.arg("dir"); dir.toUpperCase();
  int speed = velDefault;
  if (server.hasArg("speed")) speed = clampDuty(server.arg("speed").toInt());
  uint32_t durationMs = 1000;
  if (server.hasArg("duration_ms")) durationMs = (uint32_t) server.arg("duration_ms").toInt();
  if (durationMs == 0) durationMs = 1;
  if (durationMs > 5000) durationMs = 5000;
  if      (dir == "A") adelante(speed);
  else if (dir == "B") atras(speed);
  else if (dir == "I") izquierda(speed);
  else if (dir == "D") derecha(speed);
  else if (dir == "S") parar();
  else { server.send(400, "application/json", "{\"error\":\"dir invalido. Use A|B|I|D|S\"}"); return; }
  if (dir != "S") activeMotionUntil = millis() + durationMs; else activeMotionUntil = 0;
  String clientIp = server.client().remoteIP().toString();
  publishMoveToMqtt(dir, speed, durationMs, clientIp);
  String json = "{";
  json += "\"ok\":true,";
  json += "\"dir\":\"" + dir + "\",";
  json += "\"speed\":" + String(speed) + ",";
  json += "\"duration_ms\":" + String(durationMs) + ",";
  json += "\"client_ip\":\"" + clientIp + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

// ===== WIFI =====
void connectWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-start < 20000) { delay(250); }
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

#if USE_HCSR04 == 1
  pinMode(HCSR04_TRIG_PIN, OUTPUT);
  pinMode(HCSR04_ECHO_PIN, INPUT);
#endif

  setLeds(false,false,false);
  setReverseLed(false);
  setBlue(false);
  setVelocidades(0,0);

  connectWiFi();
  Serial.println(WiFi.localIP());

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/move",   HTTP_POST, handleMove);
  server.on("/health", HTTP_GET, [](){ server.send(200, "application/json", "{\"status\":\"ok\"}"); });
  server.onNotFound([](){ server.send(404, "application/json", "{\"error\":\"Not found\"}"); });
  server.begin();

  parar();
  randomSeed(micros());
}

void loop(){
  server.handleClient();
  mqtt.loop();

  if (activeMotionUntil > 0 && millis() >= activeMotionUntil) { parar(); }
  updateReverseBlink();

  unsigned long now = millis();
  if (now - lastSensorPublish >= SENSOR_PUBLISH_INTERVAL_MS) {
    lastSensorPublish = now;
    float d = readUltrasonic();
    publishDistanceToMqtt(d);
  }
}
