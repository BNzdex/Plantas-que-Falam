#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "arduinoFFT.h"

// Configuração do Display OLED (LilyGO)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Configurações WiFi
const char* ssid = "Redmi Note 13";
const char* password = "12345678";

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

// Histórico para gráfico
#define HISTORY_SIZE 20
double magnitudeHistory[HISTORY_SIZE];
int historyIndex = 0;

// Configurações do sistema
struct SystemConfig {
  int samplingFreq = 10000;
  int samples = 512;
  int refreshRate = 1000;
  bool autoDetection = true;
  float sensitivityThreshold = 0.001;
  bool alertsEnabled = true;
  String plantName = "Samambaia Principal";
  String plantType = "Nephrolepis exaltata";
} config;

// Dados estatísticos
struct Statistics {
  unsigned long totalCommunications = 0;
  unsigned long sessionStart = 0;
  double maxFreqRecorded = 0;
  double avgSessionFreq = 0;
  int plantsActive = 0;
  double peakMagnitude = 0;
} stats;

// Histórico de análises (últimas 24 horas simuladas)
#define ANALYSIS_HISTORY_SIZE 24
struct AnalysisPoint {
  int hour;
  int communications;
  double avgFreq;
  double avgMagnitude;
} analysisHistory[ANALYSIS_HISTORY_SIZE];

struct FrequencyBand {
  String name;
  double minFreq;
  double maxFreq;
  double magnitude;
  double magnitudeDb;
  String color;
};

FrequencyBand bands[] = {
  {"Sub Bass", 20, 60, 0, -80, "#ff6b6b"},
  {"Bass", 60, 250, 0, -80, "#4ecdc4"},
  {"Low Mid", 250, 500, 0, -80, "#45b7d1"},
  {"Mid", 500, 2000, 0, -80, "#96ceb4"},
  {"High Mid", 2000, 4000, 0, -80, "#feca57"},
  {"Presence", 4000, 6000, 0, -80, "#ff9ff3"},
  {"Brilliance", 6000, 20000, 0, -80, "#54a0ff"}
};

const int numBands = sizeof(bands) / sizeof(bands[0]);

void setup() {
  Serial.begin(115200);
  
  // Inicializar display
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Configurar pino do sensor
  pinMode(PIEZO_PIN, INPUT);
  
  // Inicializar histórico
  for (int i = 0; i < HISTORY_SIZE; i++) {
    magnitudeHistory[i] = 0;
  }
  
  // Inicializar estatísticas
  stats.sessionStart = millis();
  
  // Inicializar dados de análise
  initializeAnalysisData();
  
  // Tela de inicialização
  showStartupScreen();
  
  // Conectar WiFi
  setupWiFi();
  
  // Configurar servidor web
  setupWebServer();
  
  Serial.println("=== SISTEMA WEB INICIADO ===");
  Serial.printf("- Pino sensor: GPIO %d\n", PIEZO_PIN);
  Serial.printf("- Frequência amostragem: %d Hz\n", SAMPLING_FREQUENCY);
  if (wifiConnected) {
    Serial.printf("- Dashboard: http://%s\n", WiFi.localIP().toString().c_str());
  }
  Serial.println("============================");
}

void loop() {
  // Manter servidor web
  server.handleClient();
  
  // Ler valor bruto do sensor
  rawSensorValue = analogRead(PIEZO_PIN);
  sensorVoltage = (rawSensorValue * 3.3) / 4095.0;
  
  // Coletar amostras para FFT
  collectSamples();
  
  // Processar FFT
  processFFT();
  
  // Analisar dados
  analyzeData();
  
  // Atualizar estatísticas
  updateStatistics();
  
  // Atualizar display
  if (millis() - lastDisplay > displayInterval) {
    updateDisplay();
    lastDisplay = millis();
  }
  
  delay(10);
}

void initializeAnalysisData() {
  for (int i = 0; i < ANALYSIS_HISTORY_SIZE; i++) {
    analysisHistory[i].hour = i;
    analysisHistory[i].communications = random(5, 25);
    analysisHistory[i].avgFreq = random(100, 2000);
    analysisHistory[i].avgMagnitude = random(1, 100) / 1000.0;
  }
}

void updateStatistics() {
  if (maxMagnitude > config.sensitivityThreshold) {
    stats.totalCommunications++;
    stats.plantsActive = 1;
  } else {
    stats.plantsActive = 0;
  }
  
  if (dominantFreq > stats.maxFreqRecorded) {
    stats.maxFreqRecorded = dominantFreq;
  }
  
  if (maxMagnitude > stats.peakMagnitude) {
    stats.peakMagnitude = maxMagnitude;
  }
  
  // Atualizar média da sessão
  unsigned long sessionDuration = millis() - stats.sessionStart;
  if (sessionDuration > 0) {
    stats.avgSessionFreq = (stats.avgSessionFreq + dominantFreq) / 2.0;
  }
}

void showStartupScreen() {
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "Plantas que Falam");
  u8g2.drawStr(0, 30, "Iniciando...");
  u8g2.drawStr(0, 45, "GPIO 34 - 10kHz");
  u8g2.sendBuffer();
  delay(2000);
}

void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando ao WiFi: ");
  Serial.println(ssid);
  
  // Mostrar status no display
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "Conectando WiFi...");
  u8g2.drawStr(0, 30, ssid);
  u8g2.sendBuffer();
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("");
    Serial.println("WiFi conectado!");
    Serial.print("Dashboard: http://");
    Serial.println(WiFi.localIP());
    
    // Mostrar sucesso no display
    u8g2.clearBuffer();
    u8g2.drawStr(0, 15, "WiFi Conectado!");
    u8g2.drawStr(0, 30, WiFi.localIP().toString().c_str());
    u8g2.drawStr(0, 45, "Dashboard ativo!");
    u8g2.sendBuffer();
    delay(3000);
  } else {
    wifiConnected = false;
    Serial.println("");
    Serial.println("Falha na conexão WiFi!");
    
    // Modo AP como fallback
    WiFi.softAP("Plantas-Que-Falam", "12345678");
    Serial.printf("Modo AP ativado. IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    // Mostrar modo AP no display
    u8g2.clearBuffer();
    u8g2.drawStr(0, 15, "Modo AP Ativo");
    u8g2.drawStr(0, 30, WiFi.softAPIP().toString().c_str());
    u8g2.drawStr(0, 45, "SSID: Plantas-Que-Falam");
    u8g2.sendBuffer();
    delay(2000);
  }
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/api/data", handleAPIData);
  server.on("/api/plants", handleAPIPlants);
  server.on("/api/analytics/summary", handleAPIAnalyticsSummary);
  server.on("/api/analytics/history", handleAPIAnalyticsHistory);
  server.on("/api/analytics/bands", handleAPIAnalyticsBands);
  server.on("/api/config", HTTP_GET, handleAPIConfigGet);
  server.on("/api/config", HTTP_POST, handleAPIConfigPost);
  
  server.onNotFound([]() {
    String path = server.uri();
    if (path.startsWith("/api/plants/")) {
      handleAPIPlantDetails();
    } else {
      server.send(404, "text/plain", "Not Found");
    }
  });
  
  server.begin();
  Serial.println("Servidor web iniciado na porta 80");
}

void handleRoot() {
  webRequests++;
  String html = getHTMLPage();
  server.send(200, "text/html", html);
}

void handleAPIData() {
  Serial.println("Requisição recebida em /api/data");
  
  DynamicJsonDocument doc(2048);
  
  doc["timestamp"] = millis();
  doc["raw_value"] = rawSensorValue;
  doc["voltage"] = sensorVoltage;
  doc["dominant_frequency"] = dominantFreq;
  doc["dominant_magnitude"] = maxMagnitude;
  doc["dominant_magnitude_db"] = dominantDb;
  doc["average_magnitude"] = avgMagnitude;
  
  // Status baseado na magnitude
  if (maxMagnitude > config.sensitivityThreshold) {
    doc["status"] = "online";
  } else {
    doc["status"] = "offline";
  }
  
  doc["plant_name"] = config.plantName;
  
  // Histórico para gráfico
  JsonArray historyArray = doc.createNestedArray("history");
  for (int i = 0; i < HISTORY_SIZE; i++) {
    JsonObject point = historyArray.createNestedObject();
    point["time"] = String(i);
    point["magnitude"] = magnitudeHistory[(historyIndex + i) % HISTORY_SIZE];
  }
  
  // Bandas de frequência
  JsonArray bandsArray = doc.createNestedArray("bands");
  for (int i = 0; i < numBands; i++) {
    JsonObject band = bandsArray.createNestedObject();
    band["name"] = bands[i].name;
    band["range"] = String(bands[i].minFreq) + "-" + String(bands[i].maxFreq) + " Hz";
    band["magnitude"] = bands[i].magnitude;
    band["magnitude_db"] = bands[i].magnitudeDb;
    band["color"] = bands[i].color;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  Serial.println("JSON enviado:");
  Serial.println(jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIPlants() {
  DynamicJsonDocument doc(2048);
  
  JsonArray plantsArray = doc.to<JsonArray>();
  
  // Planta principal
  JsonObject plant1 = plantsArray.createNestedObject();
  plant1["id"] = 1;
  plant1["name"] = config.plantName;
  plant1["type"] = config.plantType;
  plant1["location"] = "Sensor GPIO 34";
  plant1["status"] = (maxMagnitude > config.sensitivityThreshold) ? "online" : "offline";
  plant1["communication_frequency"] = dominantFreq;
  plant1["health_score"] = (maxMagnitude > config.sensitivityThreshold) ? 95 : 60;
  plant1["last_communication"] = "Agora";
  plant1["signal_strength"] = (maxMagnitude > 0.01) ? "Forte" : (maxMagnitude > 0.001) ? "Médio" : "Fraco";
  plant1["daily_communications"] = stats.totalCommunications;
  plant1["peak_frequency"] = stats.maxFreqRecorded;
  plant1["sensor_voltage"] = sensorVoltage;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIPlantDetails() {
  String path = server.uri();
  int lastSlash = path.lastIndexOf('/');
  int plantId = path.substring(lastSlash + 1).toInt();

  DynamicJsonDocument doc(1024);
  doc["plant_id"] = plantId;
  
  if (plantId == 1) {
    doc["name"] = config.plantName;
    doc["type"] = config.plantType;
    doc["raw_value"] = rawSensorValue;
    doc["voltage"] = sensorVoltage;
    doc["dominant_frequency"] = dominantFreq;
    doc["magnitude"] = maxMagnitude;
    doc["status"] = (maxMagnitude > config.sensitivityThreshold) ? "online" : "offline";
    doc["health_score"] = (maxMagnitude > config.sensitivityThreshold) ? 95 : 60;
    doc["total_communications"] = stats.totalCommunications;
    doc["peak_magnitude"] = stats.peakMagnitude;
    doc["session_avg_freq"] = stats.avgSessionFreq;
  } else {
    doc["error"] = "Planta não encontrada ou sensor não conectado";
  }

  String jsonString;
  serializeJson(doc, jsonString);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsSummary() {
  DynamicJsonDocument doc(2048);
  
  doc["total_plants"] = 3;
  doc["active_plants"] = stats.plantsActive;
  doc["total_communications_today"] = stats.totalCommunications;
  doc["average_frequency"] = stats.avgSessionFreq;
  doc["peak_frequency"] = stats.maxFreqRecorded;
  doc["peak_magnitude"] = stats.peakMagnitude;
  doc["session_duration_minutes"] = (millis() - stats.sessionStart) / 60000;
  doc["communication_rate"] = stats.totalCommunications > 0 ? (stats.totalCommunications / ((millis() - stats.sessionStart) / 60000.0)) : 0;

  JsonArray trends = doc.createNestedArray("communication_trends");
  for(int i = 0; i < ANALYSIS_HISTORY_SIZE; i++) {
      JsonObject trend_point = trends.createNestedObject();
      trend_point["hour"] = analysisHistory[i].hour;
      trend_point["communications"] = analysisHistory[i].communications;
      trend_point["avg_frequency"] = analysisHistory[i].avgFreq;
      trend_point["avg_magnitude"] = analysisHistory[i].avgMagnitude;
  }

  JsonArray distribution = doc.createNestedArray("frequency_distribution");
  JsonObject dist1 = distribution.createNestedObject();
  dist1["range"] = "Baixa (0-500Hz)";
  dist1["percentage"] = 35;
  dist1["color"] = "#ff6b6b";
  JsonObject dist2 = distribution.createNestedObject();
  dist2["range"] = "Média (500-2000Hz)";
  dist2["percentage"] = 45;
  dist2["color"] = "#4ecdc4";
  JsonObject dist3 = distribution.createNestedObject();
  dist3["range"] = "Alta (2000Hz+)";
  dist3["percentage"] = 20;
  dist3["color"] = "#45b7d1";

  String jsonString;
  serializeJson(doc, jsonString);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsHistory() {
  DynamicJsonDocument doc(1024);
  
  JsonArray historyArray = doc.createNestedArray("history");
  for (int i = 0; i < ANALYSIS_HISTORY_SIZE; i++) {
    JsonObject point = historyArray.createNestedObject();
    point["hour"] = analysisHistory[i].hour;
    point["communications"] = analysisHistory[i].communications;
    point["avg_frequency"] = analysisHistory[i].avgFreq;
    point["avg_magnitude"] = analysisHistory[i].avgMagnitude;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsBands() {
  DynamicJsonDocument doc(1024);
  
  JsonArray bandsArray = doc.to<JsonArray>();
  for (int i = 0; i < numBands; i++) {
    JsonObject band = bandsArray.createNestedObject();
    band["name"] = bands[i].name;
    band["min_freq"] = bands[i].minFreq;
    band["max_freq"] = bands[i].maxFreq;
    band["current_magnitude"] = bands[i].magnitude;
    band["current_db"] = bands[i].magnitudeDb;
    band["color"] = bands[i].color;
    
    // Dados históricos simulados
    JsonArray history = band.createNestedArray("history");
    for (int j = 0; j < 10; j++) {
      history.add(random(0, 100) / 1000.0);
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIConfigGet() {
  DynamicJsonDocument doc(1024);
  
  doc["sampling_frequency"] = config.samplingFreq;
  doc["samples"] = config.samples;
  doc["refresh_rate"] = config.refreshRate;
  doc["auto_detection"] = config.autoDetection;
  doc["sensitivity_threshold"] = config.sensitivityThreshold;
  doc["alerts_enabled"] = config.alertsEnabled;
  doc["plant_name"] = config.plantName;
  doc["plant_type"] = config.plantType;
  doc["wifi_ssid"] = ssid;
  doc["sensor_pin"] = PIEZO_PIN;
  doc["system_uptime"] = millis();
  doc["memory_free"] = ESP.getFreeHeap();
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIConfigPost() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, server.arg("plain"));
    
    // Atualizar configurações
    if (doc.containsKey("plant_name")) {
      config.plantName = doc["plant_name"].as<String>();
    }
    if (doc.containsKey("plant_type")) {
      config.plantType = doc["plant_type"].as<String>();
    }
    if (doc.containsKey("sensitivity_threshold")) {
      config.sensitivityThreshold = doc["sensitivity_threshold"];
    }
    if (doc.containsKey("alerts_enabled")) {
      config.alertsEnabled = doc["alerts_enabled"];
    }
    if (doc.containsKey("auto_detection")) {
      config.autoDetection = doc["auto_detection"];
    }
    if (doc.containsKey("refresh_rate")) {
      config.refreshRate = doc["refresh_rate"];
    }
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Configurações atualizadas\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"Dados inválidos\"}");
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
    
    while (micros() - startTime < samplingPeriod) {
      // Aguardar
    }
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
  
  // Reset das bandas
  for (int b = 0; b < numBands; b++) {
    bands[b].magnitude = 0;
    bands[b].magnitudeDb = -80;
  }
  
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
    
    // Classificar nas bandas de frequência
    for (int b = 0; b < numBands; b++) {
      if (frequency >= bands[b].minFreq && frequency < bands[b].maxFreq) {
        if (magnitude > bands[b].magnitude) {
          bands[b].magnitude = magnitude;
          bands[b].magnitudeDb = 20 * log10(magnitude + 0.001);
        }
        break;
      }
    }
  }
  
  avgMagnitude = totalMagnitude / validSamples;
  dominantDb = 20 * log10(maxMagnitude + 0.001);
  
  // Atualizar histórico
  magnitudeHistory[historyIndex] = maxMagnitude;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

void updateDisplay() {
  u8g2.clearBuffer();
  
  // Status de conexão e requests
  u8g2.setFont(u8g2_font_5x7_tf);
  String statusStr = "";
  if (wifiConnected) {
    statusStr += "WiFi:OK ";
  } else {
    statusStr += "AP:ON ";
  }
  statusStr += "Req:" + String(webRequests);
  u8g2.drawStr(0, 8, statusStr.c_str());
  
  // Valor bruto do sensor
  u8g2.setFont(u8g2_font_6x10_tf);
  char rawStr[32];
  sprintf(rawStr, "RAW: %d (%.2fV)", rawSensorValue, sensorVoltage);
  u8g2.drawStr(0, 20, rawStr);
  
  // Frequência dominante
  char freqStr[32];
  if (dominantFreq < 1000) {
    sprintf(freqStr, "FREQ: %.1f Hz", dominantFreq);
  } else {
    sprintf(freqStr, "FREQ: %.2f kHz", dominantFreq / 1000.0);
  }
  u8g2.drawStr(0, 32, freqStr);
  
  // Magnitude
  char magStr[32];
  sprintf(magStr, "MAG: %.3f (%.1fdB)", maxMagnitude, dominantDb);
  u8g2.drawStr(0, 44, magStr);
  
  // Status de atividade
  char statusActivityStr[32];
  if (maxMagnitude > config.sensitivityThreshold) {
    sprintf(statusActivityStr, "STATUS: PLANTA ATIVA!");
  } else if (maxMagnitude > 0.001) {
    sprintf(statusActivityStr, "STATUS: sinal baixo");
  } else {
    sprintf(statusActivityStr, "STATUS: silencio");
  }
  u8g2.drawStr(0, 56, statusActivityStr);
  
  u8g2.sendBuffer();
}

String getHTMLPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Plantas que Falam - Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #0f4c3a 0%, #2d5a27 50%, #1a472a 100%);
            color: white;
            min-height: 100vh;
        }
        
        .sidebar {
            position: fixed;
            left: 0;
            top: 0;
            width: 280px;
            height: 100vh;
            background: rgba(0, 0, 0, 0.3);
            backdrop-filter: blur(10px);
            border-right: 1px solid rgba(255, 255, 255, 0.1);
            padding: 20px;
            z-index: 1000;
        }
        
        .logo {
            text-align: center;
            margin-bottom: 40px;
        }
        
        .logo h1 {
            font-size: 1.5rem;
            margin-bottom: 5px;
            color: #22c55e;
        }
        
        .logo p {
            font-size: 0.9rem;
            opacity: 0.7;
        }
        
        .nav-menu {
            list-style: none;
        }
        
        .nav-item {
            margin-bottom: 10px;
        }
        
        .nav-btn {
            display: flex;
            align-items: center;
            width: 100%;
            padding: 15px 20px;
            background: transparent;
            border: none;
            color: white;
            text-align: left;
            border-radius: 10px;
            cursor: pointer;
            transition: all 0.3s ease;
            font-size: 1rem;
        }
        
        .nav-btn:hover, .nav-btn.active {
            background: rgba(34, 197, 94, 0.2);
            color: #22c55e;
        }
        
        .nav-btn .icon {
            margin-right: 12px;
            font-size: 1.2rem;
        }
        
        .connection-status {
            position: absolute;
            bottom: 20px;
            left: 20px;
            right: 20px;
            padding: 15px;
            background: rgba(0, 0, 0, 0.2);
            border-radius: 10px;
            text-align: center;
        }
        
        .status-dot {
            display: inline-block;
            width: 8px;
            height: 8px;
            border-radius: 50%;
            margin-right: 8px;
        }
        
        .status-dot.online { background: #22c55e; }
        .status-dot.offline { background: #ef4444; }
        .status-dot.connecting { background: #f59e0b; }
        
        .main-content {
            margin-left: 280px;
            padding: 30px;
            min-height: 100vh;
        }
        
        .tab-content {
            display: none;
        }
        
        .tab-content.active {
            display: block;
        }
        
        .header {
            margin-bottom: 30px;
        }
        
        .header h2 {
            font-size: 2rem;
            margin-bottom: 10px;
            color: #22c55e;
        }
        
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .stat-card {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 15px;
            padding: 25px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .stat-card h3 {
            font-size: 0.9rem;
            opacity: 0.8;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .stat-value {
            font-size: 2rem;
            font-weight: bold;
            color: #22c55e;
            margin-bottom: 5px;
        }
        
        .stat-unit {
            font-size: 0.9rem;
            opacity: 0.7;
        }
        
        .chart-container {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 15px;
            padding: 25px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            margin-bottom: 30px;
        }
        
        .chart-container h3 {
            margin-bottom: 20px;
            color: #22c55e;
        }
        
        .chart-wrapper {
            position: relative;
            height: 300px;
        }
        
        .bands-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
        }
        
        .band-card {
            background: rgba(255, 255, 255, 0.05);
            border-radius: 10px;
            padding: 15px;
            text-align: center;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .band-name {
            font-size: 0.9rem;
            margin-bottom: 8px;
            color: #22c55e;
        }
        
        .band-value {
            font-size: 1.1rem;
            font-weight: bold;
            margin-bottom: 4px;
        }
        
        .band-db {
            font-size: 0.8rem;
            opacity: 0.7;
        }
        
        .loading {
            text-align: center;
            padding: 40px;
            opacity: 0.7;
        }
        
        /* Estilos para Plantas */
        .plants-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 20px;
        }
        
        .plant-card {
            background: rgba(255, 255, 255, 0.1);
            border-radius: 15px;
            padding: 25px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            transition: transform 0.3s ease, box-shadow 0.3s ease;
        }
        
        .plant-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.3);
        }
        
        .plant-header {
            display: flex;
            justify-content: between;
            align-items: center;
            margin-bottom: 15px;
        }
        
        .plant-name {
            font-size: 1.3rem;
            font-weight: bold;
            color: #22c55e;
            margin-bottom: 5px;
        }
        
        .plant-type {
            font-size: 0.9rem;
            opacity: 0.7;
            font-style: italic;
        }
        
        .plant-status {
            padding: 5px 12px;
            border-radius: 20px;
            font-size: 0.8rem;
            font-weight: bold;
            text-transform: uppercase;
        }
        
        .status-online {
            background: rgba(34, 197, 94, 0.2);
            color: #22c55e;
            border: 1px solid #22c55e;
        }
        
        .status-offline {
            background: rgba(239, 68, 68, 0.2);
            color: #ef4444;
            border: 1px solid #ef4444;
        }
        
        .status-maintenance {
            background: rgba(245, 158, 11, 0.2);
            color: #f59e0b;
            border: 1px solid #f59e0b;
        }
        
        .plant-details {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 15px;
            margin-top: 15px;
        }
        
        .detail-item {
            text-align: center;
        }
        
        .detail-label {
            font-size: 0.8rem;
            opacity: 0.7;
            margin-bottom: 5px;
            text-transform: uppercase;
        }
        
        .detail-value {
            font-size: 1.1rem;
            font-weight: bold;
            color: #22c55e;
        }
        
        /* Estilos para Análises */
        .analysis-section {
            margin-bottom: 40px;
        }
        
        .metrics-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-bottom: 30px;
        }
        
        .metric-card {
            background: rgba(255, 255, 255, 0.05);
            border-radius: 10px;
            padding: 20px;
            text-align: center;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .metric-icon {
            font-size: 2rem;
            margin-bottom: 10px;
            color: #22c55e;
        }
        
        .metric-value {
            font-size: 1.5rem;
            font-weight: bold;
            color: white;
            margin-bottom: 5px;
        }
        
        .metric-label {
            font-size: 0.8rem;
            opacity: 0.7;
            text-transform: uppercase;
        }
        
        .chart-grid {
            display: grid;
            grid-template-columns: 2fr 1fr;
            gap: 20px;
            margin-bottom: 30px;
        }
        
        /* Estilos para Configurações */
        .config-section {
            margin-bottom: 30px;
        }
        
        .config-section h3 {
            color: #22c55e;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .form-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
        }
        
        .form-group {
            margin-bottom: 20px;
        }
        
       .form-label {
            display: block;
            margin-bottom: 8px;
            color: #22c55e;
            font-weight: 500;
        }
        .form-select {
            width: 100%;              
            padding: 10px 12px;        
            font-size: 14px;          
            border: 1px solid #ccc;    
            border-radius: 8px;       
            background-color: #fff;    
            color: #333;              
            appearance: none;          
            outline: none;             
            transition: border-color 0.3s, box-shadow 0.3s; 
        }

        
        .form-input, .form-select {
            width: 100%;
            padding: 12px;
            background: rgba(255, 255, 255, 0.1);
            border: 1px solid rgba(255, 255, 255, 0.2);
            border-radius: 8px;
            color: white;
            font-size: 1rem;
        }
        
        .form-input:focus, .form-select:focus {
            outline: none;
            border-color: #22c55e;
            box-shadow: 0 0 0 2px rgba(34, 197, 94, 0.2);
        }
        
        .form-checkbox {
            display: flex;
            align-items: center;
            margin-bottom: 15px;
        }
        
        .form-checkbox input {
            margin-right: 10px;
            transform: scale(1.2);
        }
        
        .btn {
            padding: 12px 24px;
            background: #22c55e;
            color: white;
            border: none;
            border-radius: 8px;
            cursor: pointer;
            font-size: 1rem;
            font-weight: 500;
            transition: background 0.3s ease;
        }
        
        .btn:hover {
            background: #16a34a;
        }
        
        .btn-secondary {
            background: rgba(255, 255, 255, 0.1);
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        
        .btn-secondary:hover {
            background: rgba(255, 255, 255, 0.2);
        }
        
        .system-info {
            background: rgba(0, 0, 0, 0.2);
            border-radius: 10px;
            padding: 20px;
            margin-top: 20px;
        }
        
        .info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        
        .info-item {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .info-label {
            opacity: 0.7;
        }
        
        .info-value {
            color: #22c55e;
            font-weight: 500;
        }
        
        @media (max-width: 768px) {
            .sidebar {
                transform: translateX(-100%);
                transition: transform 0.3s ease;
            }
            
            .main-content {
                margin-left: 0;
                padding: 20px;
            }
            
            .stats-grid, .plants-grid, .form-grid, .chart-grid {
                grid-template-columns: 1fr;
            }
            
            .plant-details {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="sidebar">
        <div class="logo">
            <h1>Plantas que Falam</h1>
            <p>Comunicação Vegetal em Tempo Real</p>
        </div>
        
        <ul class="nav-menu">
            <li class="nav-item">
                <button class="nav-btn active" data-tab="dashboard">
                    Dashboard
                </button>
            </li>
            <li class="nav-item">
                <button class="nav-btn" data-tab="plants">
                    Plantas
                </button>
            </li>
            <li class="nav-item">
                <button class="nav-btn" data-tab="analytics">
                    Análises
                </button>
            </li>
            <li class="nav-item">
                <button class="nav-btn" data-tab="settings">
                    Configurações
                </button>
            </li>
        </ul>
        
        <div class="connection-status">
            <span class="status-dot" id="status-dot"></span>
            <span id="connection-status">Conectando...</span>
        </div>
    </div>
    
    <div class="main-content">
        <!-- Dashboard Tab -->
        <div id="dashboard" class="tab-content active">
            <div class="header">
                <h2>Dashboard Principal</h2>
                <p>Monitoramento em tempo real da comunicação das plantas</p>
            </div>
            
            <div class="stats-grid">
                <div class="stat-card">
                    <h3>Frequência Dominante</h3>
                    <div class="stat-value" id="dominant-freq">0</div>
                    <div class="stat-unit">Hz</div>
                </div>
                
                <div class="stat-card">
                    <h3>Magnitude do Sinal</h3>
                    <div class="stat-value" id="signal-magnitude">0.000</div>
                    <div class="stat-unit">Amplitude</div>
                </div>
                
                <div class="stat-card">
                    <h3>Valor Bruto</h3>
                    <div class="stat-value" id="raw-value">0</div>
                    <div class="stat-unit">ADC</div>
                </div>
                
                <div class="stat-card">
                    <h3>Voltagem</h3>
                    <div class="stat-value" id="voltage-value">0.00</div>
                    <div class="stat-unit">V</div>
                </div>
            </div>
            
            <div class="chart-container">
                <h3>Magnitude da Comunicação em Tempo Real</h3>
                <div class="chart-wrapper">
                    <canvas id="magnitudeChart"></canvas>
                </div>
            </div>
            
            <div class="chart-container">
                <h3>Bandas de Frequência</h3>
                <div class="bands-grid" id="bands-container">
                    <div class="loading">Carregando dados das bandas...</div>
                </div>
            </div>
        </div>
        
        <!-- Plants Tab -->
        <div id="plants" class="tab-content">
            <div class="header">
                <h2>Plantas Conectadas</h2>
                <p>Status e informações das plantas monitoradas</p>
            </div>
            <div class="plants-grid" id="plants-container">
                <div class="loading">Carregando plantas...</div>
            </div>
        </div>
        
        <!-- Analytics Tab -->
        <div id="analytics" class="tab-content">
            <div class="header">
                <h2>Análises Detalhadas</h2>
                <p>Análise avançada dos dados de comunicação das plantas</p>
            </div>
            
            <div class="analysis-section">
                <div class="metrics-grid" id="analytics-metrics">
                    <div class="loading">Carregando métricas...</div>
                </div>
            </div>
            
            <div class="chart-grid">
                <div class="chart-container">
                    <h3>Comunicações por Hora</h3>
                    <div class="chart-wrapper">
                        <canvas id="communicationsChart"></canvas>
                    </div>
                </div>
                
                <div class="chart-container">
                    <h3>Distribuição de Frequências</h3>
                    <div class="chart-wrapper">
                        <canvas id="frequencyDistChart"></canvas>
                    </div>
                </div>
            </div>
            
            <div class="chart-container">
                <h3>Análise de Bandas de Frequência</h3>
                <div class="chart-wrapper">
                    <canvas id="bandsAnalysisChart"></canvas>
                </div>
            </div>
        </div>
        
        <!-- Settings Tab -->
        <div id="settings" class="tab-content">
            <div class="header">
                <h2>Configurações do Sistema</h2>
                <p>Ajustes e configurações do sistema de monitoramento</p>
            </div>
            <div class="config-section">
                <h3>Configurações do Sistema</h3>
                <div class="form-grid">
                    <div>
                        <div class="form-group">
                            <label class="form-label">Taxa de Atualização (ms)</label>
                            <select id="refresh-rate" class="form-select">
                                <option value="500">500ms (Muito Rápido)</option>
                                <option value="1000">1000ms (Padrão)</option>
                                <option value="2000">2000ms (Lento)</option>
                                <option value="5000">5000ms (Muito Lento)</option>
                            </select>
                        </div>
                        <div class="form-checkbox">
                            <input type="checkbox" id="alerts-enabled">
                            <label>Alertas Habilitados</label>
                        </div>
                    </div>
                    <div>
                        <div class="form-group">
                            <label class="form-label">Modo de Operação</label>
                            <select id="operation-mode" class="form-select">
                                <option value="realtime">Tempo Real</option>
                                <option value="batch">Processamento em Lote</option>
                                <option value="eco">Modo Econômico</option>
                            </select>
                        </div>
                        <div class="form-checkbox">
                            <input type="checkbox" id="data-logging">
                            <label>Registro de Dados</label>
                        </div>
                    </div>
                </div>
                
                <div style="margin-top: 20px;">
                    <button class="btn" onclick="saveSettings()">Salvar Configurações</button>
                    <button class="btn btn-secondary" onclick="resetSettings()" style="margin-left: 10px;">Restaurar Padrões</button>
                </div>
            </div>
            
            <div class="config-section">
                <h3>Informações do Sistema</h3>
                <div class="system-info" id="system-info">
                    <div class="loading">Carregando informações do sistema...</div>
                </div>
            </div>
        </div>
    </div>

    <script>
        const CONFIG = {
            API_BASE_URL: '/api',
            REFRESH_INTERVAL: 1000,
            CHART_MAX_POINTS: 20
        };

        let appState = {
            currentTab: 'dashboard',
            refreshInterval: null,
            charts: {
                magnitude: null,
                communications: null,
                frequencyDist: null,
                bandsAnalysis: null
            },
            isConnected: false,
            settings: {}
        };

        document.addEventListener('DOMContentLoaded', function() {
            initializeApp();
        });

        function initializeApp() {
            setupNavigation();
            startDataRefresh();
            loadInitialData();
            loadSettings();
            console.log('Plantas que Falam - Aplicação iniciada');
        }

        function setupNavigation() {
            const navButtons = document.querySelectorAll('.nav-btn');
            
            navButtons.forEach(button => {
                button.addEventListener('click', function() {
                    const tabId = this.getAttribute('data-tab');
                    switchTab(tabId);
                });
            });
        }

        function switchTab(tabId) {
            // Remove active class from all tabs and buttons
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.classList.remove('active');
            });
            document.querySelectorAll('.nav-btn').forEach(btn => {
                btn.classList.remove('active');
            });
            
            // Add active class to selected tab and button
            document.getElementById(tabId).classList.add('active');
            document.querySelector(`[data-tab="${tabId}"]`).classList.add('active');
            
            appState.currentTab = tabId;
            
            // Load specific data for each tab
            if (tabId === 'plants') {
                loadPlantsData();
            } else if (tabId === 'analytics') {
                loadAnalyticsData();
            } else if (tabId === 'settings') {
                loadSettings();
            }
        }

        function startDataRefresh() {
            if (appState.refreshInterval) {
                clearInterval(appState.refreshInterval);
            }
            
            appState.refreshInterval = setInterval(fetchData, CONFIG.REFRESH_INTERVAL);
        }

        function loadInitialData() {
            fetchData();
        }

        function fetchData() {
            fetch(`${CONFIG.API_BASE_URL}/data`)
                .then(response => {
                    if (!response.ok) {
                        throw new Error(`HTTP error! status: ${response.status}`);
                    }
                    return response.json();
                })
                .then(data => {
                    console.log('Dados recebidos:', data);
                    updateDashboard(data);
                    updateConnectionStatus('online');
                })
                .catch(error => {
                    console.error('Erro ao buscar dados:', error);
                    updateConnectionStatus('offline');
                });
        }

        function updateDashboard(data) {
            // Atualizar estatísticas principais
            updateElement('dominant-freq', data.dominant_frequency ? data.dominant_frequency.toFixed(1) : '0');
            updateElement('signal-magnitude', data.dominant_magnitude ? data.dominant_magnitude.toFixed(6) : '0.000');
            updateElement('raw-value', data.raw_value || '0');
            updateElement('voltage-value', data.voltage ? data.voltage.toFixed(3) : '0.00');
            
            // Atualizar gráfico
            if (data.history) {
                updateMagnitudeChart(data.history);
            }
            
            // Atualizar bandas
            if (data.bands) {
                updateBands(data.bands);
            }
        }

        function updateElement(id, value) {
            const element = document.getElementById(id);
            if (element) {
                element.textContent = value;
            }
        }

        function updateConnectionStatus(status) {
            const statusDot = document.getElementById('status-dot');
            const statusText = document.getElementById('connection-status');
            
            if (statusDot && statusText) {
                statusDot.className = `status-dot ${status}`;
                
                switch (status) {
                    case 'online':
                        statusText.textContent = 'Online';
                        appState.isConnected = true;
                        break;
                    case 'offline':
                        statusText.textContent = 'Offline';
                        appState.isConnected = false;
                        break;
                    case 'connecting':
                        statusText.textContent = 'Conectando...';
                        break;
                }
            }
        }

        function updateMagnitudeChart(historyData) {
            const ctx = document.getElementById('magnitudeChart');
            if (!ctx) return;
            
            if (appState.charts.magnitude) {
                // Atualizar gráfico existente
                appState.charts.magnitude.data.labels = historyData.map((item, index) => index);
                appState.charts.magnitude.data.datasets[0].data = historyData.map(item => item.magnitude || item);
                appState.charts.magnitude.update('none');
            } else {
                // Criar novo gráfico
                appState.charts.magnitude = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: historyData.map((item, index) => index),
                        datasets: [{
                            label: 'Magnitude da Comunicação',
                            data: historyData.map(item => item.magnitude || item),
                            borderColor: '#22c55e',
                            backgroundColor: 'rgba(34, 197, 94, 0.1)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.4,
                            pointBackgroundColor: '#22c55e',
                            pointBorderColor: '#16a34a',
                            pointRadius: 2,
                            pointHoverRadius: 4
                        }]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        plugins: {
                            legend: {
                                display: true,
                                labels: {
                                    color: 'white',
                                    font: {
                                        size: 12
                                    }
                                }
                            }
                        },
                        scales: {
                            x: {
                                display: true,
                                grid: {
                                    color: 'rgba(255, 255, 255, 0.1)'
                                },
                                ticks: {
                                    color: 'rgba(255, 255, 255, 0.7)'
                                }
                            },
                            y: {
                                display: true,
                                beginAtZero: true,
                                grid: {
                                    color: 'rgba(255, 255, 255, 0.1)'
                                },
                                ticks: {
                                    color: 'rgba(255, 255, 255, 0.7)'
                                }
                            }
                        },
                        animation: {
                            duration: 0
                        }
                    }
                });
            }
        }

        function updateBands(bands) {
            const container = document.getElementById('bands-container');
            if (!container) return;
            
            container.innerHTML = '';
            
            bands.forEach(band => {
                const bandElement = document.createElement('div');
                bandElement.className = 'band-card';
                bandElement.style.borderLeft = `4px solid ${band.color || '#22c55e'}`;
                bandElement.innerHTML = `
                    <div class="band-name">${band.name}</div>
                    <div class="band-value">${(band.magnitude || 0).toFixed(3)}</div>
                    <div class="band-db">${(band.magnitude_db || -80).toFixed(1)} dB</div>
                    <div style="font-size: 0.7rem; opacity: 0.6; margin-top: 4px;">${band.range}</div>
                `;
                container.appendChild(bandElement);
            });
        }

        // Funções para Plantas
        function loadPlantsData() {
            fetch(`${CONFIG.API_BASE_URL}/plants`)
                .then(response => response.json())
                .then(plants => {
                    displayPlantsData(plants);
                })
                .catch(error => {
                    console.error('Erro ao carregar plantas:', error);
                    document.getElementById('plants-container').innerHTML = 
                        '<div class="loading">Erro ao carregar dados das plantas</div>';
                });
        }

        function displayPlantsData(plants) {
            const container = document.getElementById('plants-container');
            container.innerHTML = '';
            
            plants.forEach(plant => {
                const plantCard = document.createElement('div');
                plantCard.className = 'plant-card';
                
                const statusClass = plant.status === 'online' ? 'status-online' : 
                                  plant.status === 'maintenance' ? 'status-maintenance' : 'status-offline';
                
                plantCard.innerHTML = `
                    <div class="plant-header">
                        <div>
                            <div class="plant-name">${plant.name}</div>
                            <div class="plant-type">${plant.type}</div>
                        </div>
                        <div class="plant-status ${statusClass}">${plant.status}</div>
                    </div>
                    
                    <div class="plant-details">
                        <div class="detail-item">
                            <div class="detail-label">Localização</div>
                            <div class="detail-value">${plant.location}</div>
                        </div>
                        <div class="detail-item">
                            <div class="detail-label">Saúde</div>
                            <div class="detail-value">${plant.health_score}%</div>
                        </div>
                        <div class="detail-item">
                            <div class="detail-label">Freq. Comunicação</div>
                            <div class="detail-value">${plant.communication_frequency ? plant.communication_frequency.toFixed(1) + ' Hz' : 'N/A'}</div>
                        </div>
                        <div class="detail-item">
                            <div class="detail-label">Força do Sinal</div>
                            <div class="detail-value">${plant.signal_strength}</div>
                        </div>
                        <div class="detail-item">
                            <div class="detail-label">Última Comunicação</div>
                            <div class="detail-value">${plant.last_communication}</div>
                        </div>
                        <div class="detail-item">
                `;
                
                container.appendChild(plantCard);
            });
        }

        // Funções para Analytics
        function loadAnalyticsData() {
            Promise.all([
                fetch(`${CONFIG.API_BASE_URL}/analytics/summary`).then(r => r.json()),
                fetch(`${CONFIG.API_BASE_URL}/analytics/history`).then(r => r.json()),
                fetch(`${CONFIG.API_BASE_URL}/analytics/bands`).then(r => r.json())
            ])
            .then(([summary, history, bands]) => {
                displayAnalyticsSummary(summary);
                updateCommunicationsChart(history.history);
                updateFrequencyDistChart(summary.frequency_distribution);
                updateBandsAnalysisChart(bands);
            })
            .catch(error => {
                console.error('Erro ao carregar análises:', error);
            });
        }

        function displayAnalyticsSummary(summary) {
            const container = document.getElementById('analytics-metrics');
            container.innerHTML = `
                <div class="metric-card">
                    <div class="metric-value">${summary.total_plants}</div>
                    <div class="metric-label">Total de Plantas</div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">${summary.active_plants}</div>
                    <div class="metric-label">Plantas Ativas</div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">${summary.total_communications_today}</div>
                    <div class="metric-label">Comunicações Hoje</div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">${summary.average_frequency ? summary.average_frequency.toFixed(1) : '0'}</div>
                    <div class="metric-label">Freq. Média (Hz)</div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">${summary.peak_frequency ? summary.peak_frequency.toFixed(1) : '0'}</div>
                    <div class="metric-label">Pico de Freq. (Hz)</div>
                </div>
                <div class="metric-card">
                    <div class="metric-value">${summary.session_duration_minutes ? summary.session_duration_minutes.toFixed(0) : '0'}</div>
                    <div class="metric-label">Sessão (min)</div>
                </div>
            `;
        }

        function updateCommunicationsChart(historyData) {
            const ctx = document.getElementById('communicationsChart');
            if (!ctx) return;
            
            if (appState.charts.communications) {
                appState.charts.communications.destroy();
            }
            
            appState.charts.communications = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: historyData.map(item => `${item.hour}h`),
                    datasets: [{
                        label: 'Comunicações',
                        data: historyData.map(item => item.communications),
                        borderColor: '#22c55e',
                        backgroundColor: 'rgba(34, 197, 94, 0.1)',
                        borderWidth: 2,
                        fill: true,
                        tension: 0.4
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            labels: { color: 'white' }
                        }
                    },
                    scales: {
                        x: {
                            grid: { color: 'rgba(255, 255, 255, 0.1)' },
                            ticks: { color: 'rgba(255, 255, 255, 0.7)' }
                        }
                    }
                }
            });
        }

        function updateFrequencyDistChart(distributionData) {
            const ctx = document.getElementById('frequencyDistChart');
            if (!ctx) return;
            
            if (appState.charts.frequencyDist) {
                appState.charts.frequencyDist.destroy();
            }
            
            appState.charts.frequencyDist = new Chart(ctx, {
                type: 'doughnut',
                data: {
                    labels: distributionData.map(item => item.range),
                    datasets: [{
                        data: distributionData.map(item => item.percentage),
                        backgroundColor: distributionData.map(item => item.color),
                        borderWidth: 2,
                        borderColor: 'rgba(255, 255, 255, 0.1)'
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            position: 'bottom',
                            labels: { 
                                color: 'white',
                                padding: 15,
                                font: { size: 11 }
                            }
                        }
                    }
                }
            });
        }

        function updateBandsAnalysisChart(bandsData) {
            const ctx = document.getElementById('bandsAnalysisChart');
            if (!ctx) return;
            
            if (appState.charts.bandsAnalysis) {
                appState.charts.bandsAnalysis.destroy();
            }
            
            appState.charts.bandsAnalysis = new Chart(ctx, {
                type: 'bar',
                data: {
                    labels: bandsData.map(band => band.name),
                    datasets: [{
                        label: 'Magnitude Atual',
                        data: bandsData.map(band => band.current_magnitude),
                        backgroundColor: bandsData.map(band => band.color),
                        borderColor: bandsData.map(band => band.color),
                        borderWidth: 1
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    plugins: {
                        legend: {
                            labels: { color: 'white' }
                        }
                    },
                    scales: {
                        x: {
                            grid: { color: 'rgba(255, 255, 255, 0.1)' },
                            ticks: { 
                                color: 'rgba(255, 255, 255, 0.7)',
                                maxRotation: 45
                            }
                        },
                        y: {
                            grid: { color: 'rgba(255, 255, 255, 0.1)' },
                            ticks: { color: 'rgba(255, 255, 255, 0.7)' }
                        }
                    }
                }
            });
        }

        // Funções para Configurações
        function loadSettings() {
            fetch(`${CONFIG.API_BASE_URL}/config`)
                .then(response => response.json())
                .then(config => {
                    populateSettingsForm(config);
                    displaySystemInfo(config);
                })
                .catch(error => {
                    console.error('Erro ao carregar configurações:', error);
                });
        }

        function populateSettingsForm(config) {
            const elements = {
                'plant-name': config.plant_name,
                'plant-type': config.plant_type,
                'sensitivity-threshold': config.sensitivity_threshold,
                'refresh-rate': config.refresh_rate,
                'auto-detection': config.auto_detection,
                'alerts-enabled': config.alerts_enabled
            };
            
            Object.entries(elements).forEach(([id, value]) => {
                const element = document.getElementById(id);
                if (element) {
                    if (element.type === 'checkbox') {
                        element.checked = value;
                    } else {
                        element.value = value;
                    }
                }
            });
            
            appState.settings = config;
        }

        function displaySystemInfo(config) {
            const container = document.getElementById('system-info');
            container.innerHTML = `
                <div class="info-grid">
                    <div class="info-item">
                        <span class="info-label">WiFi SSID:</span>
                        <span class="info-value">${config.wifi_ssid}</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Pino do Sensor:</span>
                        <span class="info-value">GPIO ${config.sensor_pin}</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Freq. Amostragem:</span>
                        <span class="info-value">${config.sampling_frequency} Hz</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Amostras FFT:</span>
                        <span class="info-value">${config.samples}</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Tempo Ativo:</span>
                        <span class="info-value">${Math.floor(config.system_uptime / 60000)} min</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Memória Livre:</span>
                        <span class="info-value">${Math.floor(config.memory_free / 1024)} KB</span>
                    </div>
                </div>
            `;
        }

        function saveSettings() {
            const settings = {
                plant_name: document.getElementById('plant-name').value,
                plant_type: document.getElementById('plant-type').value,
                sensitivity_threshold: parseFloat(document.getElementById('sensitivity-threshold').value),
                refresh_rate: parseInt(document.getElementById('refresh-rate').value),
                auto_detection: document.getElementById('auto-detection').checked,
                alerts_enabled: document.getElementById('alerts-enabled').checked
            };
            
            fetch(`${CONFIG.API_BASE_URL}/config`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(settings)
            })
            .then(response => response.json())
            .then(result => {
                if (result.status === 'success') {
                    alert('Configurações salvas com sucesso!');
                    
                    // Atualizar intervalo de refresh se mudou
                    if (settings.refresh_rate !== CONFIG.REFRESH_INTERVAL) {
                        CONFIG.REFRESH_INTERVAL = settings.refresh_rate;
                        startDataRefresh();
                    }
                } else {
                    alert('Erro ao salvar configurações: ' + result.message);
                }
            })
            .catch(error => {
                console.error('Erro ao salvar configurações:', error);
                alert('Erro ao salvar configurações');
            });
        }

        function resetSettings() {
            if (confirm('Tem certeza que deseja restaurar as configurações padrão?')) {
                const defaultSettings = {
                    plant_name: 'Cafesal 1',
                    plant_type: 'Coffea arabica',
                    sensitivity_threshold: 0.001,
                    refresh_rate: 1000,
                    auto_detection: true,
                    alerts_enabled: true
                };
                
                populateSettingsForm(defaultSettings);
                saveSettings();
            }
        }

        // Função para exportar dados
        function exportData() {
            const data = {
                timestamp: new Date().toISOString(),
                settings: appState.settings,
                current_data: {
                    dominant_frequency: document.getElementById('dominant-freq').textContent,
                    signal_magnitude: document.getElementById('signal-magnitude').textContent,
                    raw_value: document.getElementById('raw-value').textContent,
                    voltage: document.getElementById('voltage-value').textContent
                }
            };
            
            const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `plantas_dados_${new Date().toISOString().split('T')[0]}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
        }

        // Função para reiniciar sistema
        function restartSystem() {
            if (confirm('Tem certeza que deseja reiniciar o sistema?')) {
                fetch('/api/system/restart', { method: 'POST' })
                    .then(() => {
                        alert('Sistema reiniciando... Aguarde alguns segundos e recarregue a página.');
                    })
                    .catch(error => {
                        console.error('Erro ao reiniciar:', error);
                    });
            }
        }

        // Event listeners para atalhos de teclado
        document.addEventListener('keydown', function(e) {
            if (e.ctrlKey || e.metaKey) {
                switch(e.key) {
                    case '1':
                        e.preventDefault();
                        switchTab('dashboard');
                        break;
                    case '2':
                        e.preventDefault();
                        switchTab('plants');
                        break;
                    case '3':
                        e.preventDefault();
                        switchTab('analytics');
                        break;
                    case '4':
                        e.preventDefault();
                        switchTab('settings');
                        break;
                    case 's':
                        e.preventDefault();
                        if (appState.currentTab === 'settings') {
                            saveSettings();
                        }
                        break;
                    case 'e':
                        e.preventDefault();
                        exportData();
                        break;
                }
            }
        });

        // Detectar perda de conexão
        window.addEventListener('online', function() {
            console.log('Conexão restaurada');
            updateConnectionStatus('online');
            startDataRefresh();
        });

        window.addEventListener('offline', function() {
            console.log('Conexão perdida');
            updateConnectionStatus('offline');
            if (appState.refreshInterval) {
                clearInterval(appState.refreshInterval);
            }
        });

        // Cleanup ao sair da página
        window.addEventListener('beforeunload', function() {
            if (appState.refreshInterval) {
                clearInterval(appState.refreshInterval);
            }
            
            // Destruir gráficos
            Object.values(appState.charts).forEach(chart => {
                if (chart) {
                    chart.destroy();
                }
            });
        });

        console.log('Sistema Plantas que Falam carregado com sucesso!');
        console.log('Dashboard disponível com monitoramento em tempo real');
    </script>
</body>
</html>
)rawliteral";
}
 
