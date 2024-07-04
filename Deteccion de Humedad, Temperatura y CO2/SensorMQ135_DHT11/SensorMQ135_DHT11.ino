#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <MQUnifiedsensor.h>
#include <DHT.h>

const char* ssid = "***"; // Nombre de la red Wifi
const char* password = "***"; // Contraseña de la red Wifi
const char* serverName = "***"; // URL del servidor

unsigned long lastTime = 0;
unsigned long timerDelay = 1000;

#define PLACA "NodeMCU Esp8266"
#define ADC_BIT_RESOLUTION 10
#define VOLTAGE_RESOLUTION 5
#define PIN A0
#define TYPE "MQ-135"
#define RATIO_MQ135_CLEAN_AIR 3.6

#define DHT_PIN 2
#define DHT_TYPE DHT11

DHT dht(DHT_PIN, DHT_TYPE);
MQUnifiedsensor MQ135(PLACA, VOLTAGE_RESOLUTION, ADC_BIT_RESOLUTION, PIN, TYPE);

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  MQ135.setRegressionMethod(1);
  MQ135.init();

  Serial.print("Calibrando por favor espere.");
  float calcR0 = 0;
  for (int i = 1; i <= 10; i++) {
    MQ135.update();
    calcR0 += MQ135.calibrate(RATIO_MQ135_CLEAN_AIR);
    Serial.print(".");
  }
  MQ135.setR0(calcR0 / 10);
  Serial.println("  Hecho!");

  if (isinf(calcR0) || calcR0 == 0) {
    Serial.println("Error en la calibración, verifique el cableado y el suministro");
    while (true);
  }

  Serial.println("* Lecturas desde MQ-135 ***");
  Serial.println("|    CO   |   CO2  |");
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float CO2 = 0;

  if (!isnan(h) && !isnan(t)) {
    MQ135.update();
    MQ135.setA(110.47);
    MQ135.setB(-2.862);
    CO2 = MQ135.readSensor() + 400;
    if (isnan(CO2)) {
      Serial.println("Error de lectura desde el sensor MQ-135!");
      return;
    }
  } else {
    Serial.println("Error de lectura desde el sensor DHT!");
    return;
  }

  Serial.print(", CO2: ");
  Serial.println(CO2);
  Serial.print(", Temperature: ");
  Serial.print(t);
  Serial.print(" degrees Celcius, Humidity: ");
  Serial.print(h);
  Serial.println("%. Send to server.");

  if ((millis() - lastTime) > timerDelay) {
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      HTTPClient http;

      http.begin(client, serverName);
      http.addHeader("Content-Type", "application/json");

      String payload = "{\"temp\":" + String(t) + ",\"hum\":" + String(h) + ",\"co2\":" + String(CO2) + "}";
      int httpResponseCode = http.POST(payload);

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      http.end();
    } else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
  delay(timerDelay);
}
