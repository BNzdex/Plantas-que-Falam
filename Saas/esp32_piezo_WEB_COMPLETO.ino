#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "arduinoFFT.h"

// Configuração do Display OLED (LilyGO)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Configurações WiFi
const char* ssid = "iPhone"; 
const char* password = "123456789"; 

// Configurações FFT
#define SAMPLES 512              
#define SAMPLING_FREQUENCY 10000 
#define PIEZO_PIN 34            

// Variáveis FFT
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

// Servidor Web
WebServer server(80);

// Variáveis de controle
unsigned long lastDisplay = 0;
const unsigned long displayInterval = 200; 

// Variáveis globais para dados
double dominantFreq = 0;
double maxMagnitude = 0;
double dominantDb = -80;
int rawSensorValue = 0;
double sensorVoltage = 0;
double avgMagnitude = 0;

// Status de conexão
bool wifiConnected = false;
int webRequests = 0;

// Histórico
#define HISTORY_SIZE 20
double magnitudeHistory[HISTORY_SIZE];
unsigned long historyTimestamps[HISTORY_SIZE];
int historyIndex = 0;

// Bandas de frequência
struct FrequencyBand {
  String name;
  double minFreq;
  double maxFreq;
  double magnitude;
  double magnitudeDb;
  String color;
};

FrequencyBand bands[] = {
  {"Sub Bass", 20, 60, 0, -80, "#22c55e"},
  {"Bass", 60, 250, 0, -80, "#16a34a"},
  {"Low Mid", 250, 500, 0, -80, "#15803d"},
  {"Mid", 500, 2000, 0, -80, "#166534"},
  {"High Mid", 2000, 4000, 0, -80, "#14532d"},
  {"Presence", 4000, 6000, 0, -80, "#84cc16"},
  {"Brilliance", 6000, 20000, 0, -80, "#65a30d"}
};

const int numBands = sizeof(bands) / sizeof(bands[0]);

// ---------- DECLARAÇÕES ----------
void showStartupScreen();
void setupWiFi();
void setupWebServer();
void collectSamples();
void processFFT();
void analyzeData();
void analyzeBands();
void updateHistory();
void updateDisplay();
void handleAPIData();
void handleAPIPlants();
void handleAPIPlantDetails();
void handleAPIAnalyticsSummary();

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  pinMode(PIEZO_PIN, INPUT);

  // Inicializar histórico
  for (int i = 0; i < HISTORY_SIZE; i++) {
    magnitudeHistory[i] = 0;
    historyTimestamps[i] = 0;
  }

  showStartupScreen();
  setupWiFi();
  setupWebServer();

  Serial.println("=== ESP32 API SERVER INICIADO ===");
  Serial.printf("- Pino sensor: GPIO %d\n", PIEZO_PIN);
  Serial.printf("- Frequência amostragem: %d Hz\n", SAMPLING_FREQUENCY);
  if (wifiConnected) {
    Serial.printf("- API Base URL: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.printf("- Endpoints disponíveis:\n");
    Serial.printf("  * GET /api/sensor/data\n");
    Serial.printf("  * GET /api/sensor/plants\n");
    Serial.printf("  * GET /api/sensor/analytics/summary\n");
  }
  Serial.println("================================");
}

// ---------- LOOP ----------
void loop() {
  server.handleClient();

  // Leitura do sensor
  rawSensorValue = analogRead(PIEZO_PIN);
  sensorVoltage = (rawSensorValue * 3.3) / 4095.0;

  // Processamento de dados
  collectSamples();
  processFFT();
  analyzeData();
  analyzeBands();
  updateHistory();

  // Atualizar display
  if (millis() - lastDisplay > displayInterval) {
    updateDisplay();
    lastDisplay = millis();
  }

  delay(10);
}

// ---------- TELAS ----------
void showStartupScreen() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "ESP32 API Server");
  u8g2.drawStr(0, 30, "Plantas Falantes");
  u8g2.drawStr(0, 45, "Iniciando...");
  u8g2.sendBuffer();
  delay(2000);
}

// ---------- WIFI ----------
void setupWiFi() {
  Serial.printf("Conectando ao WiFi: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "Conectando WiFi...");
  u8g2.sendBuffer();
  
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi conectado!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    
    u8g2.clearBuffer();
    u8g2.drawStr(0, 15, "WiFi Conectado!");
    u8g2.drawStr(0, 30, WiFi.localIP().toString().c_str());
    u8g2.sendBuffer();
    delay(2000);
  } else {
    wifiConnected = false;
    Serial.println("\nFalha na conexão WiFi!");
    
    u8g2.clearBuffer();
    u8g2.drawStr(0, 15, "Erro WiFi!");
    u8g2.drawStr(0, 30, "Modo offline");
    u8g2.sendBuffer();
    delay(2000);
  }
}

// ---------- WEB SERVER ----------
void setupWebServer() {
  // CORS para todos os endpoints
  server.enableCORS(true);
  
  // API Endpoints
  server.on("/api/sensor/data", HTTP_GET, handleAPIData);
  server.on("/api/sensor/plants", HTTP_GET, handleAPIPlants);
  server.on("/api/sensor/analytics/summary", HTTP_GET, handleAPIAnalyticsSummary);

  // Endpoint para detalhes específicos de plantas
  server.onNotFound([]() {
    String path = server.uri();
    if (path.startsWith("/api/sensor/plants/")) {
      handleAPIPlantDetails();
    } else {
      server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
    }
  });

  server.begin();
  Serial.println("API Server iniciado na porta 80");
}

// ---------- API HANDLERS ----------
void handleAPIData() {
  DynamicJsonDocument doc(3072);
  
  // Timestamp atual
  doc["timestamp"] = millis();
  doc["uptime"] = millis() / 1000;
  
  // Dados básicos do sensor
  doc["raw_value"] = rawSensorValue;
  doc["voltage"] = round(sensorVoltage * 1000.0) / 1000.0;
  doc["dominant_frequency"] = round(dominantFreq * 10.0) / 10.0;
  doc["dominant_magnitude"] = round(maxMagnitude * 1000.0) / 1000.0;
  doc["dominant_magnitude_db"] = round(dominantDb * 10.0) / 10.0;
  doc["average_magnitude"] = round(avgMagnitude * 1000.0) / 1000.0;
  
  // Status da planta
  doc["status"] = (maxMagnitude > 0.05) ? "online" : "offline";
  doc["plant_name"] = "Sensor Piezoelétrico";
  doc["device_id"] = WiFi.macAddress();
  
  // Histórico de magnitude
  JsonArray history = doc.createNestedArray("history");
  for(int i = 0; i < HISTORY_SIZE; i++) {
    JsonObject point = history.createNestedObject();
    int idx = (historyIndex + i) % HISTORY_SIZE;
    point["time"] = String(historyTimestamps[idx]);
    point["magnitude"] = round(magnitudeHistory[idx] * 1000.0) / 1000.0;
  }

  // Dados das bandas de frequência
  JsonArray bandsArray = doc.createNestedArray("bands");
  for(int i = 0; i < numBands; i++) {
    JsonObject bandObj = bandsArray.createNestedObject();
    bandObj["name"] = bands[i].name;
    bandObj["range"] = String(bands[i].minFreq) + "-" + String(bands[i].maxFreq) + " Hz";
    bandObj["magnitude"] = round(bands[i].magnitude * 1000.0) / 1000.0;
    bandObj["magnitude_db"] = round(bands[i].magnitudeDb * 10.0) / 10.0;
    bandObj["color"] = bands[i].color;
  }

  // Metadados do sistema
  JsonObject system = doc.createNestedObject("system");
  system["free_heap"] = ESP.getFreeHeap();
  system["wifi_rssi"] = WiFi.RSSI();
  system["requests_count"] = webRequests;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", jsonString);
  
  webRequests++;
}

void handleAPIPlants() {
  DynamicJsonDocument doc(1024);
  
  JsonArray plantsArray = doc.to<JsonArray>();
  
  // Planta 1 - Sensor atual
  JsonObject plant1 = plantsArray.createNestedObject();
  plant1["id"] = 1;
  plant1["name"] = "Sensor Principal";
  plant1["type"] = "Piezoelétrico";
  plant1["location"] = "ESP32 GPIO34";
  plant1["status"] = (maxMagnitude > 0.05) ? "online" : "offline";
  plant1["communication_frequency"] = round(dominantFreq * 10.0) / 10.0;
  plant1["health_score"] = (maxMagnitude > 0.1) ? 95 : 60;
  plant1["last_communication"] = "2025-09-05T" + String(millis()/1000) + "Z";

  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIPlantDetails() {
  String path = server.uri();
  int lastSlash = path.lastIndexOf('/');
  int plantId = path.substring(lastSlash + 1).toInt();

  DynamicJsonDocument doc(512);
  
  if (plantId == 1) {
    doc["plant_id"] = plantId;
    doc["name"] = "Sensor Principal";
    doc["raw_value"] = rawSensorValue;
    doc["voltage"] = round(sensorVoltage * 1000.0) / 1000.0;
    doc["dominant_frequency"] = round(dominantFreq * 10.0) / 10.0;
    doc["dominant_magnitude"] = round(maxMagnitude * 1000.0) / 1000.0;
    doc["status"] = (maxMagnitude > 0.05) ? "online" : "offline";
    doc["last_update"] = millis();
  } else {
    doc["error"] = "Plant not found";
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(404, "application/json", "{\"error\":\"Plant not found\"}");
    return;
  }

  String jsonString;
  serializeJson(doc, jsonString);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsSummary() {
  DynamicJsonDocument doc(2048);
  
  // Estatísticas básicas
  doc["total_plants"] = 1;
  doc["active_plants"] = (maxMagnitude > 0.05) ? 1 : 0;
  doc["total_communications_today"] = webRequests;
  doc["average_frequency"] = round(dominantFreq * 10.0) / 10.0;
  doc["max_magnitude_today"] = round(maxMagnitude * 1000.0) / 1000.0;
  doc["system_uptime"] = millis() / 1000;

  // Tendências de comunicação (últimas 24 "horas" = últimos 24 pontos)
  JsonArray trends = doc.createNestedArray("communication_trends");
  for(int i = 0; i < min(24, HISTORY_SIZE); i++) {
    JsonObject trend_point = trends.createNestedObject();
    trend_point["period"] = i;
    int idx = (historyIndex + i) % HISTORY_SIZE;
    trend_point["magnitude"] = round(magnitudeHistory[idx] * 1000.0) / 1000.0;
    trend_point["timestamp"] = historyTimestamps[idx];
  }

  // Distribuição de frequências por bandas
  JsonArray distribution = doc.createNestedArray("frequency_distribution");
  double totalMagnitude = 0;
  for(int i = 0; i < numBands; i++) {
    totalMagnitude += bands[i].magnitude;
  }
  
  for(int i = 0; i < numBands; i++) {
    JsonObject dist = distribution.createNestedObject();
    dist["range"] = bands[i].name;
    dist["percentage"] = totalMagnitude > 0 ? 
      round((bands[i].magnitude / totalMagnitude) * 100.0 * 10.0) / 10.0 : 0;
    dist["magnitude"] = round(bands[i].magnitude * 1000.0) / 1000.0;
  }

  String jsonString;
  serializeJson(doc, jsonString);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

// ---------- PROCESSAMENTO ----------
void collectSamples() {
  unsigned long samplingPeriod = 1000000L / SAMPLING_FREQUENCY;
  for (int i = 0; i < SAMPLES; i++) {
    unsigned long startTime = micros();
    int sensorValue = analogRead(PIEZO_PIN);
    double voltage = (sensorValue * 3.3) / 4095.0;
    vReal[i] = voltage - 1.65; // Remove DC offset
    vImag[i] = 0.0;
    while (micros() - startTime < samplingPeriod);
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
  double totalMagnitude = 0;
  int validBins = 0;

  // Encontrar frequência dominante e calcular média
  for (int i = 1; i < (SAMPLES / 2); i++) {
    double frequency = i * frequencyResolution;
    double magnitude = vReal[i];
    
    if (magnitude > maxMagnitude) {
      maxMagnitude = magnitude;
      dominantFreq = frequency;
    }
    
    totalMagnitude += magnitude;
    validBins++;
  }
  
  avgMagnitude = validBins > 0 ? totalMagnitude / validBins : 0;
  dominantDb = 20 * log10(maxMagnitude + 0.001);
}

void analyzeBands() {
  double frequencyResolution = (double)SAMPLING_FREQUENCY / SAMPLES;
  
  // Resetar bandas
  for (int b = 0; b < numBands; b++) {
    bands[b].magnitude = 0;
    bands[b].magnitudeDb = -80;
  }
  
  // Analisar cada bin de frequência
  for (int i = 1; i < (SAMPLES / 2); i++) {
    double frequency = i * frequencyResolution;
    double magnitude = vReal[i];
    
    // Verificar em qual banda essa frequência se encaixa
    for (int b = 0; b < numBands; b++) {
      if (frequency >= bands[b].minFreq && frequency <= bands[b].maxFreq) {
        if (magnitude > bands[b].magnitude) {
          bands[b].magnitude = magnitude;
          bands[b].magnitudeDb = 20 * log10(magnitude + 0.001);
        }
      }
    }
  }
}

void updateHistory() {
  magnitudeHistory[historyIndex] = maxMagnitude;
  historyTimestamps[historyIndex] = millis();
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

void updateDisplay() {
  u8g2.clearBuffer();
  
  // Linha 1: Status WiFi e IP
  if (wifiConnected) {
    u8g2.drawStr(0, 10, "API: Online");
    u8g2.drawStr(80, 10, String(webRequests).c_str());
  } else {
    u8g2.drawStr(0, 10, "API: Offline");
  }
  
  // Linha 2: Valor raw e voltagem
  char line[32];
  sprintf(line, "RAW:%d V:%.2f", rawSensorValue, sensorVoltage);
  u8g2.drawStr(0, 25, line);
  
  // Linha 3: Frequência dominante
  sprintf(line, "FREQ:%.1fHz", dominantFreq);
  u8g2.drawStr(0, 40, line);
  
  // Linha 4: Magnitude e dB
  sprintf(line, "MAG:%.3f", maxMagnitude);
  u8g2.drawStr(0, 55, line);
  sprintf(line, "%.1fdB", dominantDb);
  u8g2.drawStr(80, 55, line);
  
  u8g2.sendBuffer();
}
