#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <TinyGPS++.h>
#include "html.h"

const char *AP_SSID = "WxRadar";
const char *AP_PASSWORD = "wx1234567";

#define LED_PIN 2
#define SERVO_PIN 13
#define RF_RSSI_PIN 34
#define IMU_SDA 21
#define IMU_SCL 22
#define GPS_RX 16
#define GPS_TX 17

#define SWEEP_MIN_DEG 0
#define SWEEP_MAX_DEG 180
#define SWEEP_STEP 2
#define DWELL_MS 40

#define ADC_MIN 200
#define ADC_MAX 3800
#define DBZ_MIN 0.0f
#define DBZ_MAX 75.0f

#define SAMPLES_PER_STEP 8

#define ADXL345_ADDR 0x53
#define POWER_CTL 0x2D
#define DATA_FORMAT 0x31
#define DATAX0 0x32

WebServer server(80);
Servo antennaServo;

int sweepAngle = SWEEP_MIN_DEG;
bool sweepForward = true;
float reflectivity[181];

TinyGPSPlus gps;
float pitchDeg = 0.0f;
float rollDeg = 0.0f;
float gpsLat = 0.0f;
float gpsLon = 0.0f;
float gpsAltFt = 0.0f;
float gpsSpeedKt = 0.0f;
float gpsCourse = 0.0f;

unsigned long lastSweepMs = 0;

void imuInit()
{
    Wire.begin(IMU_SDA, IMU_SCL);
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(POWER_CTL);
    Wire.write(0x08);
    Wire.endTransmission();
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(DATA_FORMAT);
    Wire.write(0x01);
    Wire.endTransmission();
}

void imuRead()
{
    Wire.beginTransmission(ADXL345_ADDR);
    Wire.write(DATAX0);
    Wire.endTransmission(false);
    Wire.requestFrom(ADXL345_ADDR, 6, true);
    if (Wire.available() < 6)
        return;
    int16_t ax = (Wire.read() | (Wire.read() << 8));
    int16_t ay = (Wire.read() | (Wire.read() << 8));
    int16_t az = (Wire.read() | (Wire.read() << 8));

    float gx = ax * 0.0087f;
    float gy = ay * 0.0087f;
    float gz = az * 0.0087f;

    pitchDeg = atan2f(-gx, sqrtf(gy * gy + gz * gz)) * 57.2958f;
    rollDeg = atan2f(gy, gz) * 57.2958f;
}

float measureReflectivity()
{
    long sum = 0;
    for (int i = 0; i < SAMPLES_PER_STEP; i++)
    {
        sum += analogRead(RF_RSSI_PIN);
        delayMicroseconds(200);
    }
    float avg = (float)sum / SAMPLES_PER_STEP;
    avg = constrain(avg, ADC_MIN, ADC_MAX);

    float dbz = DBZ_MIN + (avg - ADC_MIN) / (float)(ADC_MAX - ADC_MIN) * (DBZ_MAX - DBZ_MIN);

    float correctionFactor = cosf(pitchDeg * 0.01745f) * cosf(rollDeg * 0.01745f);
    correctionFactor = max(correctionFactor, 0.1f);
    return dbz * correctionFactor;
}

void sweepStep()
{
    unsigned long now = millis();
    if (now - lastSweepMs < DWELL_MS)
        return;
    lastSweepMs = now;

    antennaServo.write(sweepAngle);
    delayMicroseconds(500);

    imuRead();
    float dbz = measureReflectivity();

    if (dbz < 0.0f)
        dbz = 0.0f;
    reflectivity[sweepAngle] = dbz;

    if (sweepForward)
    {
        sweepAngle += SWEEP_STEP;
        if (sweepAngle > SWEEP_MAX_DEG)
        {
            sweepAngle = SWEEP_MAX_DEG;
            sweepForward = false;
        }
    }
    else
    {
        sweepAngle -= SWEEP_STEP;
        if (sweepAngle < SWEEP_MIN_DEG)
        {
            sweepAngle = SWEEP_MIN_DEG;
            sweepForward = true;
        }
    }

    while (Serial2.available())
        gps.encode(Serial2.read());

    if (gps.location.isValid())
    {
        gpsLat = gps.location.lat();
        gpsLon = gps.location.lng();
        gpsAltFt = gps.altitude.feet();
        gpsSpeedKt = gps.speed.knots();
        gpsCourse = gps.course.deg();
    }
}

void handleRoot()
{
    server.send(200, "text/html", html_page);
}

void handleRadarData()
{
    String json = "{";
    json += "\"angle\":" + String(sweepAngle) + ",";
    json += "\"pitch\":" + String(pitchDeg, 1) + ",";
    json += "\"roll\":" + String(rollDeg, 1) + ",";
    json += "\"lat\":" + String(gpsLat, 5) + ",";
    json += "\"lon\":" + String(gpsLon, 5) + ",";
    json += "\"alt\":" + String(gpsAltFt, 0) + ",";
    json += "\"spd\":" + String(gpsSpeedKt, 1) + ",";
    json += "\"hdg\":" + String(gpsCourse, 1) + ",";
    json += "\"sweep\":[";
    for (int i = 0; i <= 180; i++)
    {
        json += String(reflectivity[i], 1);
        if (i < 180)
            json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
}

void setup()
{
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    antennaServo.attach(SERVO_PIN);
    antennaServo.write(90);
    delay(500);

    imuInit();

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    for (int i = 0; i <= 180; i++)
        reflectivity[i] = 0.0f;

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/radardata", handleRadarData);
    server.begin();

    digitalWrite(LED_PIN, HIGH);
    Serial.println("WxRadar ready.");
}

void loop()
{
    server.handleClient();
    sweepStep();
}