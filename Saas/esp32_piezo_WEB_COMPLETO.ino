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
  
  // Atualizar display
  if (millis() - lastDisplay > displayInterval) {
    updateDisplay();
    lastDisplay = millis();
  }
  
  delay(10);
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
  if (maxMagnitude > 0.01) {
    doc["status"] = "online";
  } else {
    doc["status"] = "offline";
  }
  
  doc["plant_name"] = "Samambaia Falante";
  
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
  DynamicJsonDocument doc(1024);
  
  JsonArray plantsArray = doc.to<JsonArray>();
  JsonObject plant1 = plantsArray.createNestedObject();
  plant1["id"] = 1;
  plant1["name"] = "Samambaia Principal";
  plant1["type"] = "Nephrolepis exaltata";
  plant1["location"] = "Sensor GPIO 34";
  plant1["status"] = (maxMagnitude > 0.01) ? "online" : "offline";
  plant1["communication_frequency"] = dominantFreq;
  plant1["health_score"] = (maxMagnitude > 0.01) ? 95 : 60;
  plant1["last_communication"] = "2025-09-08T10:30:00Z";

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
  doc["plant_id"] = plantId;
  doc["raw_value"] = rawSensorValue;
  doc["dominant_frequency"] = dominantFreq;
  doc["status"] = (maxMagnitude > 0.01) ? "online" : "offline";

  String jsonString;
  serializeJson(doc, jsonString);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsSummary() {
  DynamicJsonDocument doc(1024);
  doc["total_plants"] = 1;
  doc["active_plants"] = (maxMagnitude > 0.01) ? 1 : 0;
  doc["total_communications_today"] = webRequests;
  doc["average_frequency"] = dominantFreq;

  JsonArray trends = doc.createNestedArray("communication_trends");
  for(int i = 0; i < 24; i++) {
      JsonObject trend_point = trends.createNestedObject();
      trend_point["hour"] = i;
      trend_point["communications"] = random(5, 20);
  }

  JsonArray distribution = doc.createNestedArray("frequency_distribution");
  JsonObject dist1 = distribution.createNestedObject();
  dist1["range"] = "Baixa";
  dist1["percentage"] = 40;
  JsonObject dist2 = distribution.createNestedObject();
  dist2["range"] = "Média";
  dist2["percentage"] = 50;
  JsonObject dist3 = distribution.createNestedObject();
  dist3["range"] = "Alta";
  dist3["percentage"] = 10;

  String jsonString;
  serializeJson(doc, jsonString);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
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
  if (maxMagnitude > 0.01) {
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
    <title>🌱 Plantas que Falam - Dashboard</title>
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
        
        @media (max-width: 768px) {
            .sidebar {
                transform: translateX(-100%);
                transition: transform 0.3s ease;
            }
            
            .main-content {
                margin-left: 0;
                padding: 20px;
            }
            
            .stats-grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="sidebar">
        <div class="logo">
            <h1>🌱 Plantas que Falam</h1>
            <p>Comunicação Vegetal em Tempo Real</p>
        </div>
        
        <ul class="nav-menu">
            <li class="nav-item">
                <button class="nav-btn active" data-tab="dashboard">
                    <span class="icon">📊</span>
                    Dashboard
                </button>
            </li>
            <li class="nav-item">
                <button class="nav-btn" data-tab="plants">
                    <span class="icon">🌿</span>
                    Plantas
                </button>
            </li>
            <li class="nav-item">
                <button class="nav-btn" data-tab="analytics">
                    <span class="icon">📈</span>
                    Analytics
                </button>
            </li>
            <li class="nav-item">
                <button class="nav-btn" data-tab="settings">
                    <span class="icon">⚙️</span>
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
                <h3>📈 Magnitude da Comunicação em Tempo Real</h3>
                <div class="chart-wrapper">
                    <canvas id="magnitudeChart"></canvas>
                </div>
            </div>
            
            <div class="chart-container">
                <h3>🎛️ Bandas de Frequência</h3>
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
            <div id="plants-list" class="loading">Carregando plantas...</div>
        </div>
        
        <!-- Analytics Tab -->
        <div id="analytics" class="tab-content">
            <div class="header">
                <h2>Analytics</h2>
                <p>Análise detalhada dos dados de comunicação</p>
            </div>
            <div class="loading">Dados de analytics em desenvolvimento...</div>
        </div>
        
        <!-- Settings Tab -->
        <div id="settings" class="tab-content">
            <div class="header">
                <h2>Configurações</h2>
                <p>Ajustes do sistema de monitoramento</p>
            </div>
            <div class="loading">Painel de configurações em desenvolvimento...</div>
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
                magnitude: null
            },
            isConnected: false
        };

        document.addEventListener('DOMContentLoaded', function() {
            initializeApp();
        });

        function initializeApp() {
            setupNavigation();
            startDataRefresh();
            loadInitialData();
            console.log('🌱 Plantas que Falam - Aplicação iniciada');
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
    </script>
</body>
</html>
)rawliteral";
}
