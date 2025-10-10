#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include "arduinoFFT.h"

// ==================== PINOS LILYGO ESP32 ====================
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 23
#define LORA_DIO0 26
#define LORA_BAND 915E6

// Sensores
#define PIEZO_PIN 34
#define BATTERY_PIN 35
#define LED_PIN 2

// Configurações FFT
#define SAMPLES 512
#define SAMPLING_FREQUENCY 10000

// Variáveis FFT
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

// Variáveis globais
double dominantFreq = 0;
double maxMagnitude = 0;
double dominantDb = -80;
int rawSensorValue = 0;
double sensorVoltage = 0;
double avgMagnitude = 0;
double batteryVoltage = 0;
double batteryPercentage = 0;

// Controle de transmissão
unsigned long lastTransmission = 0;
const unsigned long transmissionInterval = 1000;
unsigned int packetCounter = 0;

// Configurações
String plantName = "Cafesal 01";
float sensitivityThreshold = 0.001;

void setup() {
  Serial.begin(115200);
  delay(10000);
  
  Serial.println("\n========================================");
  Serial.println("TRANSMISSOR LORA - VERSAO OTIMIZADA");
  Serial.println("========================================\n");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(PIEZO_PIN, INPUT);
  pinMode(BATTERY_PIN, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  Serial.println("Pinos analogicos configurados");

  Serial.print("\nInicializando LoRa em ");
  Serial.print(LORA_BAND / 1E6);
  Serial.print(" MHz... ");
  
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("FALHOU!");
    while(1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  
  Serial.println("OK!");
  
  // Configurar LoRa
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.setTxPower(20);
  LoRa.enableCrc();

  Serial.println("\n========================================");
  Serial.println("SISTEMA PRONTO!");
  Serial.println("========================================");
  Serial.printf("Planta: %s\n", plantName.c_str());
  Serial.printf("Freq Amostragem: %d Hz\n", SAMPLING_FREQUENCY);
  Serial.printf("Amostras FFT: %d\n", SAMPLES);
  Serial.println("========================================\n");
  
  digitalWrite(LED_PIN, HIGH);
  delay(2000);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  rawSensorValue = analogRead(PIEZO_PIN);
  sensorVoltage = (rawSensorValue * 3.3) / 4095.0;
  readBatteryLevel();
  
  collectSamples();
  processFFT();
  analyzeData();
  
  if (millis() - lastTransmission >= transmissionInterval) {
    lastTransmission = millis();
    transmitData();
  }
  
  delay(10);
}

void readBatteryLevel() {
  int batteryRawValue = analogRead(BATTERY_PIN);
  batteryVoltage = (batteryRawValue * 3.3 * 2) / 4095.0;
  
  if (batteryVoltage >= 4.2) {
    batteryPercentage = 100.0;
  } else if (batteryVoltage <= 3.0) {
    batteryPercentage = 0.0;
  } else {
    batteryPercentage = ((batteryVoltage - 3.0) / 1.2) * 100.0;
  }
}

void collectSamples() {
  unsigned long samplingPeriod = 1000000L / SAMPLING_FREQUENCY;
  
  for (int i = 0; i < SAMPLES; i++) {
    unsigned long startTime = micros();
    
    int sensorValue = analogRead(PIEZO_PIN);
    double voltage = (sensorValue * 3.3) / 4095.0;
    vReal[i] = voltage - 1.65;
    vImag[i] = 0.0;
    
    while (micros() - startTime < samplingPeriod) {}
  }
}

void processFFT() {
  FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
  FFT.compute(FFTDirection::Forward);
  FFT.complexToMagnitude();
}

void analyzeData() {
  double frequencyResolution = (double)SAMPLING_FREQUENCY / SAMPLES;
  
  maxMagnitude = 0;
  dominantFreq = 0;
  avgMagnitude = 0;
  
  double totalMagnitude = 0;
  int validSamples = 0;
  
  for (int i = 1; i < (SAMPLES / 2); i++) {
    double frequency = i * frequencyResolution;
    double magnitude = vReal[i];
    
    totalMagnitude += magnitude;
    validSamples++;
    
    if (magnitude > maxMagnitude) {
      maxMagnitude = magnitude;
      dominantFreq = frequency;
    }
  }
  
  avgMagnitude = totalMagnitude / validSamples;
  dominantDb = 20 * log10(maxMagnitude + 0.001);
}

void transmitData() {
  packetCounter++;
  digitalWrite(LED_PIN, HIGH);
  
  // JSON ULTRA-COMPACTO (apenas dados essenciais)
  // Formato: pk,f,m,db,bv,bp,st
  StaticJsonDocument<128> doc;  // Buffer MUITO menor
  
  doc["pk"] = packetCounter;
  doc["f"] = (int)dominantFreq;  // Frequência como inteiro
  doc["m"] = (int)(maxMagnitude * 1000);  // Magnitude x1000 como inteiro
  doc["db"] = (int)dominantDb;
  doc["bv"] = (int)(batteryVoltage * 100);  // Tensão x100 como inteiro
  doc["bp"] = (int)batteryPercentage;
  doc["st"] = (maxMagnitude > sensitivityThreshold) ? 1 : 0;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Verificar tamanho ANTES de transmitir
  int jsonSize = jsonString.length();
  
  Serial.println("\n========================================");
  Serial.println("TX PACOTE #" + String(packetCounter));
  Serial.println("========================================");
  Serial.println("Tamanho: " + String(jsonSize) + " bytes");
  
  if (jsonSize > 200) {
    Serial.println("AVISO: Pacote muito grande!");
  }
  
  Serial.println("JSON: " + jsonString);
  
  // Transmitir
  LoRa.beginPacket();
  LoRa.print(jsonString);
  LoRa.endPacket();
  
  Serial.println("Freq: " + String(dominantFreq, 2) + " Hz");
  Serial.println("Mag: " + String(maxMagnitude, 4));
  Serial.println("dB: " + String(dominantDb, 2));
  Serial.println("Bat: " + String(batteryPercentage, 1) + "% (" + String(batteryVoltage, 2) + "V)");
  Serial.println("Status: " + String((maxMagnitude > sensitivityThreshold) ? "ON" : "OFF"));
  Serial.println("========================================\n");
  
  digitalWrite(LED_PIN, LOW);
}
