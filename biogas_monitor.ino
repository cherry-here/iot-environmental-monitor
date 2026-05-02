/*
 * Smart Biogas Monitor -- IoT Dashboard
 * Author: Mothi Charan Naik Desavath
 * Hardware: Arduino Pro Mini + ESP8266 WiFi + MQ-4 + MQ-135 + MQ-136 + DS18B20
 * Pipeline: MQTT -> Node-RED -> InfluxDB -> Grafana
 * Features: 3 gas sensors, temperature compensation, 5 configurable alert thresholds
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- WiFi Credentials ------------------------------------
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- MQTT Broker -----------------------------------------
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "biogas/monitor";

// --- Sensor Pins -----------------------------------------
#define MQ4_PIN      A0    // CH4 Methane (200-10000 ppm)
#define MQ135_PIN    A1    // CO2 Air Quality
#define MQ136_PIN    A2    // H2S Toxic Gas (1-200 ppm)
#define DS18B20_PIN  D4    // Temperature Probe (1-Wire)

// --- Alert Thresholds (ppm) -----------------------------
#define CH4_WARN_THRESHOLD    1000
#define CH4_CRIT_THRESHOLD    5000
#define CO2_WARN_THRESHOLD    1000
#define CO2_CRIT_THRESHOLD    3000
#define H2S_WARN_THRESHOLD    10
#define H2S_CRIT_THRESHOLD    50

WiFiClient espClient;
PubSubClient mqttClient(espClient);
OneWire oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);

// --- Gas Sensor Calibration Data -------------------------
// Rs/R0 ratios at clean air for each sensor
const float MQ4_R0_CLEAN_AIR  = 4.4;    // Methane
const float MQ135_R0_CLEAN_AIR = 3.6;   // CO2
const float MQ136_R0_CLEAN_AIR = 3.0;   // H2S

float ch4_ppm = 0, co2_ppm = 0, h2s_ppm = 0, temperature = 0;

void setup() {
    Serial.begin(115200);
    tempSensor.begin();

    // Allow sensors to warm up
    Serial.println("Warming up sensors (60s)...");
    delay(60000);

    // Calibrate R0 at clean air
    float mq4_r0  = calibrateR0(MQ4_PIN, MQ4_R0_CLEAN_AIR);
    float mq135_r0 = calibrateR0(MQ135_PIN, MQ135_R0_CLEAN_AIR);
    float mq136_r0 = calibrateR0(MQ136_PIN, MQ136_R0_CLEAN_AIR);

    Serial.printf("R0 values -- MQ4: %.2f, MQ135: %.2f, MQ136: %.2f\n",
                  mq4_r0, mq135_r0, mq136_r0);

    connectWiFi();
    mqttClient.setServer(mqtt_server, mqtt_port);
    Serial.println("Smart Biogas Monitor Ready");
}

void loop() {
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();

    static unsigned long lastRead = 0;
    if (millis() - lastRead > 5000) {
        readSensors();
        checkAlerts();
        publishData();
        lastRead = millis();

        Serial.printf("CH4: %.0f ppm | CO2: %.0f ppm | H2S: %.1f ppm | Temp: %.1f C\n",
                      ch4_ppm, co2_ppm, h2s_ppm, temperature);
    }
}

void readSensors() {
    // Temperature (for gas sensor compensation)
    tempSensor.requestTemperatures();
    temperature = tempSensor.getTempCByIndex(0);

    // Gas sensor readings with temperature compensation
    float tempFactor = 1.0 + ((temperature - 25.0) * 0.005);  // ~0.5% per degree C

    ch4_ppm  = readGasPPM(MQ4_PIN,  MQ4_R0_CLEAN_AIR,  tempFactor);
    co2_ppm  = readGasPPM(MQ135_PIN, MQ135_R0_CLEAN_AIR, tempFactor);
    h2s_ppm  = readGasPPM(MQ136_PIN, MQ136_R0_CLEAN_AIR, tempFactor);
}

float readGasPPM(int pin, float r0CleanAir, float tempComp) {
    float rs = analogRead(pin) * (5.0 / 1023.0);  // Convert to voltage
    float ratio = rs / r0CleanAir;
    ratio *= tempComp;  // Temperature compensation
    // Simplified ppm estimation (calibration curve)
    return max(0.0, 100.0 * pow(ratio, -1.5));
}

float calibrateR0(int pin, float cleanAirRatio) {
    float rs = analogRead(pin) * (5.0 / 1023.0);
    return rs / cleanAirRatio;
}

void checkAlerts() {
    // CH4 alerts
    if (ch4_ppm >= CH4_CRIT_THRESHOLD) {
        mqttClient.publish("biogas/alert", "CRITICAL: CH4 level exceeds 5000 ppm!");
    } else if (ch4_ppm >= CH4_WARN_THRESHOLD) {
        mqttClient.publish("biogas/alert", "WARNING: CH4 level exceeds 1000 ppm");
    }
    // CO2 alerts
    if (co2_ppm >= CO2_CRIT_THRESHOLD) {
        mqttClient.publish("biogas/alert", "CRITICAL: CO2 level exceeds 3000 ppm!");
    } else if (co2_ppm >= CO2_WARN_THRESHOLD) {
        mqttClient.publish("biogas/alert", "WARNING: CO2 level exceeds 1000 ppm");
    }
    // H2S alerts (highest priority)
    if (h2s_ppm >= H2S_CRIT_THRESHOLD) {
        mqttClient.publish("biogas/alert", "CRITICAL: H2S toxic level exceeds 50 ppm!");
    } else if (h2s_ppm >= H2S_WARN_THRESHOLD) {
        mqttClient.publish("biogas/alert", "WARNING: H2S detected above 10 ppm");
    }
}

void publishData() {
    String payload = "{";
    payload += "\"ch4\":" + String(ch4_ppm, 1) + ",";
    payload += "\"co2\":" + String(co2_ppm, 1) + ",";
    payload += "\"h2s\":" + String(h2s_ppm, 2) + ",";
    payload += "\"temperature\":" + String(temperature, 1);
    payload += "}";
    mqttClient.publish(mqtt_topic, payload.c_str());
}

void connectWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

void reconnectMQTT() {
    while (!mqttClient.connected()) {
        if (mqttClient.connect("BiogasMonitor")) Serial.println("MQTT connected");
        else delay(5000);
    }
}