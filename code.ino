// ====== esp32_car.ino ======
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>   // <--- TLS
#include <WebServer.h>
#include <PubSubClient.h>
#include "config.h"

// ========= TLS: Pega aquí tu CA =========
const char ca_cert[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFJzCCAw+gAwIBAgIIIsy1L3JkGfcwDQYJKoZIhvcNAQELBQAwRDELMAkGA1UE
BhMCQ08xFDASBgNVBAoTC0NpYmVyLU5leHVzMR8wHQYDVQQDExZDQSBJbnRlcm5h
IENpYmVyLU5leHVzMB4XDTI1MDkxOTEwMDgwMFoXDTI2MDkxOTEwMDgwMFowRDEL
MAkGA1UEBhMCQ08xFDASBgNVBAoTC0NpYmVyLU5leHVzMR8wHQYDVQQDExZDQSBJ
bnRlcm5hIENpYmVyLU5leHVzMIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKC
AgEAnoI8rmIyic4XSpGosrQ3RDjI6RL8XE+hNz6wVe/XpWi3XxanFSp02C8tiU/z
3seM59VkYIAAuSsrhKVOTzo+87fr9oXiS+6WK2SLjTGb01DTJpWSRgV2IZhnlTGW
JdAwoBWOfnpmQqfS9NQyFmLMTRa9qIeojmmFLUQFn9688bX7yK2XZDzWddc059a6
afRK8V1p5UUajqvhEheoBMeqBNVJ2V54M4nkOOjab2KFaIhM7KCwsKAx1/MSNv4k
G58OlluB2ob0RX3U2uEMr5b/84HAtsAtJB/24Bzp6MskIwetfqQYjuJhDsG02Fcd
XOYTJnGvIUeyazk9q39J2dqQp/++JOxW3Pq+HwY7Y4HqBgH3GX7lO1AQaKwQksXW
j6S9G4IiU1KBRuURbQ952dMX8BXWGayNHMrfxZQ1razU87QQLqdkSN92WMyVbwYV
G4IRg4g5UHn2ewoesCPX0JKzkPLDNg/ORIo5XNrTTNUCF9KI2UH/L9qVq5eTai9T
nFZrk8SNvEor6c9ImSW6SUBKvPlhssF4jjyhKLKNxj9qLX90ERYWRxjinTPp5kDj
14cGTJW/fJbCYUIgtNQvB7CAZH646Xt2T7K9Fjbbm1o2mAdiK5V47i/LZ7ckzHy2
gvC6wOvHxqfPdIvFWvN+6YNOqsRn1OVKwlrQqqE8ROas410CAwEAAaMdMBswDAYD
VR0TBAUwAwEB/zALBgNVHQ8EBAMCAQYwDQYJKoZIhvcNAQELBQADggIBAEQkLnTf
6OxCm9F6Ge1uU8IxgNGPdYDitIOh9HYlVGCN3ZbhnxODDOBj4BwJLJ24Xd6kpwtF
T14IQN44Y4be64VdiI/jb38+qrxltY4QcSRZ8RQ+0Ttlrm3g2aHtwqyH9QHwbFXq
epVkqupypToiJPwZwnr/EjyL4hLFdGL387OTg/p0kduzkbuGrfQnH00aFRQRppuy
sbxzGvZkMU3EBv9QCsDL4gKM0xB6lnDs9Uk0luwYtF/pn3/Q50wrjh7SDivwMFHu
pSJUUOLOaakhv6lRDXYqoo8QsCFirM/Wvx4nArdtt93Bvx5Rt2qGvB0DKGVk7LD5
TkQmEP3d//89klIie2ZlaBNgDBcH1HZyirSogQZ4MjMZe5udEuejC/ZhRQDfT/S0
wJBuCaFdSBu+gn6ojizHVQi4Q0LjIuwuNFKM6D/bVmVuP0GBueaFwMCT++u/fKCS
reFr+zwEoQHqHBq3nzmJB7BSS4ctjispxdxPGQBxozzEPnRFfhmVupAmREpGWtmO
WQfO9ap0qfIA68rulRnaMjL05cVb8peB19svnRGrfAp27X0BPPdJ5qYyHG1F5JZz
YebidfgNYF64hqytA5hx1dWVOHempqXJSivJrn5O7pP5uqug3ds77DMK3b2S0k+X
RvtFkJaexOb0wTm3Tb9o11M8bHGsA7wpSDZG
-----END CERTIFICATE-----
)EOF";

// ========= GLOBALS ==========

// ⚠ Cambiado: WiFiClient → WiFiClientSecure
WiFiClientSecure secureClient;

// PubSubClient usando secureClient (TLS)
PubSubClient mqtt(secureClient);

WebServer server(80);

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
    mqtt.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD);  // <-- TLS + auth
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

// ===== HC-SR04 =====
float readUltrasonic(){
#if USE_HCSR04 == 0
  long r = random(500, 30001);
  float d = r / 100.0;
  return d;
#else
  digitalWrite(HCSR04_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HCSR04_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HCSR04_TRIG_PIN, LOW);
  unsigned long duration = pulseIn(HCSR04_ECHO_PIN, HIGH, 30000UL);
  if(duration == 0) return -1.0;
  float distance_cm = (duration * 0.0343) / 2.0;
  if(distance_cm > HCSR04_MAX_CM) return -1.0;
  return distance_cm;
#endif
}

void publishDistanceToMqtt(float d){
  ensureMqtt();
  if(!mqtt.connected()) return;
  String payload = "{";
  payload += (d < 0 ? "\"distance_cm\":null," : "\"distance_cm\":" + String(d,2) + ",");
  payload += "\"esp32_ip\":\"" + WiFi.localIP().toString() + "\"";
  payload += "}";
  mqtt.publish(MQTT_TOPIC_SENSOR, payload.c_str());
}

// ===== HTTP =====
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
  WiFi.begin(WiFi_SSID, WiFi_PASS);
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

  // ====== TLS: carga CA ======
  secureClient.setCACert(ca_cert);

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);

  server.on("/status", HTTP_GET, handleStatus);
  server.on("/move",   HTTP_POST, handleMove);
  server.on("/health", HTTP_GET, [](){ server.send(200, "application/json", "{\"status\":\"ok\"}"); });
// Alias del healthcheck
  server.on("/api/v1/healthcheck", HTTP_GET, [](){
  server.send(200, "application/json", "{\"status\":\"ok\",\"version\":\"v1\"}");
  });
  // GET /api/v1/move (solo para probar sin Postman o app)
  server.on("/api/v1/move", HTTP_GET, handleMove);
  // POST /api/v1/move (el oficial pedido por el profe)
  server.on("/api/v1/move", HTTP_POST, handleMove);
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
