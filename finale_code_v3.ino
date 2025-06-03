#include <DHT.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "Adafruit_MAX1704X.h"
#include <Adafruit_LTR390.h>
// --- Sensor Pins ---
#define DHTPIN 18
#define DHTTYPE DHT22
#define SOUND_PIN 17
// --- WiFi Credentials ---
const char* ssid = "kokinetwork-2G";
const char* password = "0887588455";
// --- FIWARE Endpoint ---
const String endpoint = "http://165.232.124.98:7896/iot/json?k=987654321&i=esp32-monitoring-1";
// --- Sensor Objects ---
DHT dht(DHTPIN, DHTTYPE);
Adafruit_MAX17048 maxlipo;
Adafruit_LTR390 ltr390 = Adafruit_LTR390();
HTTPClient http;
// --- Calibration Factors ---
float ambientLuxScaleFactor = 0.601;
float uvScaleFactor = 0.1531;
// --- Structs ---
struct LightReadings {
  float ambientLux;
  uint32_t alsRaw;
  float uvLight;
  uint16_t uvRaw;
};
struct BatteryStatus {
  float level;
  float voltage;
};
struct ErrorEntry {
  String sensor;
  String severity;
  String message;
};
// --- WiFi Setup ---
void setup_wifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}
// --- Sensor Initialization ---
void setupSensors() {
  dht.begin();
  Wire.begin();
  if (!maxlipo.begin()) {
    Serial.println("MAX17048 not found!");
    while (1);
  }
  if (!ltr390.begin()) {
    Serial.println("LTR390 not found!");
    while (1);
  }
  ltr390.setGain(LTR390_GAIN_3);
  ltr390.setResolution(LTR390_RESOLUTION_16BIT);
  ltr390.setThresholds(100, 1000);
  ltr390.configInterrupt(true, LTR390_MODE_UVS);
}
// --- Sound Sensor ---
int getRawSoundLevel() {
  int samples = 50;
  int peak = 0;
  for (int i = 0; i < samples; i++) {
    int val = analogRead(SOUND_PIN);
    if (val > peak) peak = val;
    delay(1);
  }
  return peak;
}
// --- Light Sensor Readings ---
LightReadings readLightSensor() {
  LightReadings result;
  ltr390.setMode(LTR390_MODE_ALS);
  delay(100);
  result.alsRaw = ltr390.newDataAvailable() ? ltr390.readALS() : 0;
  result.ambientLux = result.alsRaw * ambientLuxScaleFactor;

  ltr390.setMode(LTR390_MODE_UVS);
  delay(100);
  result.uvRaw = ltr390.newDataAvailable() ? ltr390.readUVS() : 0;
  result.uvLight = result.uvRaw * uvScaleFactor;

  return result;
}
// --- Battery Info ---
BatteryStatus readBatteryStatus() {
  BatteryStatus status;
  status.voltage = maxlipo.cellVoltage();
  status.level = maxlipo.cellPercent();
  return status;
}
// --- Error Logging ---
void appendSensorError(JsonArray& array, const char* sensor, const char* severity, const char* message) {
  JsonObject error = array.createNestedObject();
  error["sensor"] = sensor;
  error["severity"] = severity;
  error["message"] = message;
}
// --- Data Upload ---
void sendDataToFiware(float temp, float humidity, int soundRaw,
                      LightReadings light, BatteryStatus battery, int wifis) {
  StaticJsonDocument<2048> doc;
  // Sensor values
  doc["temperature"] = temp;
  doc["humidity"] = humidity;
  doc["soundlevel"] = soundRaw;
  doc["uv_light"] = light.uvLight;
  doc["uv_light_raw"] = light.uvRaw;
  doc["ambient_light"] = light.ambientLux;
  doc["ambient_light_raw"] = light.alsRaw;
  doc["battery_level"] = battery.level;
  doc["battery_voltage"] = battery.voltage;
  doc["wifi_signal"]= wifis;
  // Metadata block with sensor names and raw values
  JsonObject metadata = doc.createNestedObject("sensor_metadata");
  // Temperature and Humidity sensor
  JsonObject tempMeta = metadata.createNestedObject("temperature");
  tempMeta["sensor_name"] = "DHT22";
  JsonObject humMeta = metadata.createNestedObject("humidity");
  humMeta["sensor_name"] = "DHT22";
  // Sound sensor
  JsonObject soundMeta = metadata.createNestedObject("soundlevel");
  soundMeta["sensor_name"] = "AnalogSoundSensor";
  // UV and Ambient Light
  JsonObject uvMeta = metadata.createNestedObject("uv_light");
  uvMeta["sensor_name"] = "LTR390";
  JsonObject luxMeta = metadata.createNestedObject("ambient_light");
  luxMeta["sensor_name"] = "LTR390";  
  // Battery readings
  JsonObject battLvlMeta = metadata.createNestedObject("battery_level");
  battLvlMeta["sensor_name"] = "MAX17048";
  JsonObject battVoltMeta = metadata.createNestedObject("battery_voltage");
  battVoltMeta["sensor_name"] = "MAX17048";
  // WiFi RSSI
  JsonObject wifiMeta = metadata.createNestedObject("wifi_signal");
  wifiMeta["sensor_name"] = "ESP32 WiFi Module";
  // Errors block
  JsonArray errors = doc.createNestedArray("sensor_errors");
  if (isnan(temp) || isnan(humidity)) {
    appendSensorError(errors, "DHT22", "critical", "Temperature or humidity read failed");
  }
  if (light.alsRaw == 0) {
    appendSensorError(errors, "LTR390-ALS", "low", "Ambient light not available");
  }
  if (light.uvRaw == 0) {
    appendSensorError(errors, "LTR390-UV", "low", "UV light not available");
  }
  // Send JSON
  String payload;
  serializeJson(doc, payload);
  http.begin(endpoint);
  http.addHeader("Content-Type", "application/json");
  int response = http.POST(payload);
  if (response == 200) {
    Serial.println("Data sent successfully.");
  } else {
    Serial.println("Send failed. HTTP code: " + String(response));
  }
  http.end();
}
// --- Setup ---
void setup() {
  Serial.begin(115200);
  setup_wifi();
  setupSensors();
}
// --- Loop ---
void loop() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  int soundRaw = getRawSoundLevel();
  int wifiSignal = WiFi.RSSI();
  LightReadings light = readLightSensor();
  BatteryStatus battery = readBatteryStatus();
  Serial.printf("Temp: %.2f°C, Humidity: %.2f%%, Sound: %d, UV: %.2f, Lux: %.2f, Battery: %.2f%%, Voltage: %.2fV, RSSI: %d dBm\n",
                temp, humidity, soundRaw, light.uvLight, light.ambientLux,
                battery.level, battery.voltage, wifiSignal);
  sendDataToFiware(temp, humidity, soundRaw, light, battery, wifiSignal);
  delay(60000); // 1-minute interval
}
