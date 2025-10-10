#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <ESPmDNS.h>

// Display OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Pinos LoRa
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26
#define BAND 915E6

// Configuração WiFi (Access Point)
const char* ssid = "7DS GRAND CROSS";
const char* password = ";

// Servidor Web
WebServer server(80);

// Dados recebidos do transmissor
struct PlantData {
  String plantName = "Aguardando...";
  String plantType = "--";
  unsigned int packetNumber = 0;
  int rawValue = 0;
  double voltage = 0;
  double dominantFreq = 0;
  double maxMagnitude = 0;
  double dominantDb = -80;
  double avgMagnitude = 0;
  double batteryVoltage = 0;
  double batteryPercent = 0;
  String status = "offline";
  int rssi = 0;
  float snr = 0;
  unsigned long lastUpdate = 0;
  
  // Bandas de frequência
  struct Band {
    String name;
    double minFreq;
    double maxFreq;
    double magnitude;
    double magnitudeDb;
  } bands[7];
  
  int numBands = 7;
} plantData;

// Histórico para gráficos
#define HISTORY_SIZE 20
double magnitudeHistory[HISTORY_SIZE];
int historyIndex = 0;

// Estatísticas
struct Statistics {
  unsigned long totalPackets = 0;
  unsigned long packetsLost = 0;
  unsigned long sessionStart = 0;
  double maxFreqRecorded = 0;
  double peakMagnitude = 0;
  int plantsActive = 0;
} stats;

// Controle
bool wifiConnected = false;
int webRequests = 0;
unsigned long lastDisplay = 0;
const unsigned long displayInterval = 200;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("RECEPTOR/GATEWAY LORA + SERVIDOR WEB");
  Serial.println("========================================\n");

  // Inicializar histórico
  for (int i = 0; i < HISTORY_SIZE; i++) {
    magnitudeHistory[i] = 0;
  }
  
  stats.sessionStart = millis();

  // Inicializar display
  Wire.begin(4, 15);
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "GATEWAY LORA");
  u8g2.drawStr(0, 30, "Iniciando...");
  u8g2.sendBuffer();
  delay(1500);

  // Inicializar LoRa
  Serial.print("Inicializando LoRa... ");
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(BAND)) {
    Serial.println("FALHOU!");
    u8g2.clearBuffer();
    u8g2.drawStr(0, 15, "ERRO LORA!");
    u8g2.drawStr(0, 30, "Verifique conexoes");
    u8g2.sendBuffer();
    while(1) delay(1000);
  }
  
  Serial.println("OK!");
  
  // Configurar LoRa (mesmos parâmetros do transmissor)
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "LoRa OK!");
  u8g2.drawStr(0, 30, "Iniciando WiFi...");
  u8g2.sendBuffer();

  // Configurar WiFi Access Point
  Serial.println("\nConfigurando WiFi Access Point...");
  Serial.printf("SSID: %s\n", ssid);
  Serial.printf("Senha: %s\n", password);
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  delay(1000);
  
  IPAddress IP = WiFi.softAPIP();
  wifiConnected = true;
  
  Serial.println("\n========================================");
  Serial.println("WiFi Access Point ATIVO!");
  Serial.println("========================================");
  Serial.printf("IP: %s\n", IP.toString().c_str());
  Serial.println("\nPara acessar o dashboard:");
  Serial.printf("1. Conecte ao WiFi: %s\n", ssid);
  Serial.printf("2. Abra: http://%s\n", IP.toString().c_str());
  Serial.println("========================================\n");

  // Configurar mDNS
  if (MDNS.begin("plantasfalam")) {
    Serial.println("mDNS: http://plantasfalam.local");
  }

  // Configurar rotas do servidor
  server.on("/", handleRoot);
  server.on("/api/data", handleAPIData);
  server.on("/api/plants", handleAPIPlants);
  server.on("/api/analytics/summary", handleAPIAnalyticsSummary);
  server.on("/api/analytics/history", handleAPIAnalyticsHistory);
  server.on("/api/analytics/bands", handleAPIAnalyticsBands);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Servidor HTTP iniciado!\n");

  u8g2.clearBuffer();
  u8g2.drawStr(0, 10, "SISTEMA PRONTO!");
  u8g2.drawStr(0, 25, ssid);
  u8g2.drawStr(0, 40, IP.toString().c_str());
  u8g2.drawStr(0, 55, "Aguardando LoRa...");
  u8g2.sendBuffer();

  Serial.println("=== AGUARDANDO PACOTES LORA ===\n");
}

void loop() {
  // Processar servidor web
  server.handleClient();
  
  // Verificar pacotes LoRa
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    receiveLoRaData();
  }
  
  // Atualizar display
  if (millis() - lastDisplay > displayInterval) {
    updateDisplay();
    lastDisplay = millis();
  }
  
  // Verificar timeout (5 segundos sem dados)
  if (millis() - plantData.lastUpdate > 5000 && stats.totalPackets > 0) {
    plantData.status = "offline";
    stats.plantsActive = 0;
  }
  
  delay(10);
}

void receiveLoRaData() {
  // Ler mensagem completa
  String message = "";
  while (LoRa.available()) {
    message += (char)LoRa.read();
  }
  
  // Obter RSSI e SNR
  plantData.rssi = LoRa.packetRssi();
  plantData.snr = LoRa.packetSnr();
  plantData.lastUpdate = millis();
  stats.totalPackets++;
  
  Serial.println("\n========================================");
  Serial.println("PACOTE LORA RECEBIDO #" + String(stats.totalPackets));
  Serial.println("========================================");
  Serial.println("Tamanho: " + String(message.length()) + " bytes");
  Serial.println("RSSI: " + String(plantData.rssi) + " dBm");
  Serial.println("SNR: " + String(plantData.snr) + " dB");
  Serial.println("Mensagem:");
  Serial.println(message);
  
  // Parsear JSON
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.println("ERRO ao parsear JSON: " + String(error.c_str()));
    Serial.println("========================================\n");
    return;
  }
  
  // Extrair dados
  plantData.plantName = doc["plant_name"].as<String>();
  plantData.plantType = doc["plant_type"].as<String>();
  plantData.packetNumber = doc["packet"];
  plantData.rawValue = doc["raw_value"];
  plantData.voltage = doc["voltage"];
  plantData.dominantFreq = doc["dominant_freq"];
  plantData.maxMagnitude = doc["max_magnitude"];
  plantData.dominantDb = doc["dominant_db"];
  plantData.avgMagnitude = doc["avg_magnitude"];
  plantData.batteryVoltage = doc["battery_voltage"];
  plantData.batteryPercent = doc["battery_percent"];
  plantData.status = doc["status"].as<String>();
  
  // Extrair bandas de frequência
  JsonArray bandsArray = doc["bands"];
  int bandIndex = 0;
  for (JsonObject band : bandsArray) {
    if (bandIndex < plantData.numBands) {
      plantData.bands[bandIndex].name = band["name"].as<String>();
      plantData.bands[bandIndex].minFreq = band["min"];
      plantData.bands[bandIndex].maxFreq = band["max"];
      plantData.bands[bandIndex].magnitude = band["mag"];
      plantData.bands[bandIndex].magnitudeDb = band["db"];
      bandIndex++;
    }
  }
  
  // Atualizar histórico
  magnitudeHistory[historyIndex] = plantData.maxMagnitude;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  
  // Atualizar estatísticas
  if (plantData.dominantFreq > stats.maxFreqRecorded) {
    stats.maxFreqRecorded = plantData.dominantFreq;
  }
  if (plantData.maxMagnitude > stats.peakMagnitude) {
    stats.peakMagnitude = plantData.maxMagnitude;
  }
  stats.plantsActive = (plantData.status == "online") ? 1 : 0;
  
  Serial.println("\nDados extraidos:");
  Serial.println("- Planta: " + plantData.plantName);
  Serial.println("- Freq Dominante: " + String(plantData.dominantFreq, 2) + " Hz");
  Serial.println("- Magnitude: " + String(plantData.maxMagnitude, 4));
  Serial.println("- Bateria: " + String(plantData.batteryPercent, 1) + "%");
  Serial.println("- Status: " + plantData.status);
  Serial.println("========================================\n");
}

void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  
  // Status WiFi e requests
  String statusStr = wifiConnected ? "WiFi:OK " : "WiFi:-- ";
  statusStr += "Req:" + String(webRequests);
  u8g2.drawStr(0, 8, statusStr.c_str());
  
  u8g2.drawLine(0, 10, 128, 10);
  
  // Nome da planta
  u8g2.setFont(u8g2_font_6x10_tf);
  String plantStr = plantData.plantName.substring(0, 16);
  u8g2.drawStr(0, 22, plantStr.c_str());
  
  // Pacotes
  String pktStr = "Pkt:" + String(plantData.packetNumber) + " RSSI:" + String(plantData.rssi);
  u8g2.drawStr(0, 34, pktStr.c_str());
  
  // Frequência
  String freqStr = "Freq:" + String(plantData.dominantFreq, 0) + "Hz";
  u8g2.drawStr(0, 46, freqStr.c_str());
  
  // Magnitude e Bateria
  String magStr = "Mag:" + String(plantData.maxMagnitude, 3);
  u8g2.drawStr(0, 58, magStr.c_str());
  
  String batStr = "Bat:" + String(plantData.batteryPercent, 0) + "%";
  u8g2.drawStr(70, 58, batStr.c_str());
  
  u8g2.sendBuffer();
}

// ==================== HANDLERS DO SERVIDOR WEB ====================

void handleRoot() {
  webRequests++;
  String html = getHTMLPage();
  server.send(200, "text/html", html);
  Serial.println("Pagina principal servida");
}

void handleAPIData() {
  DynamicJsonDocument doc(2048);
  
  doc["timestamp"] = millis();
  doc["raw_value"] = plantData.rawValue;
  doc["voltage"] = plantData.voltage;
  doc["dominant_frequency"] = plantData.dominantFreq;
  doc["dominant_magnitude"] = plantData.maxMagnitude;
  doc["dominant_magnitude_db"] = plantData.dominantDb;
  doc["average_magnitude"] = plantData.avgMagnitude;
  doc["battery_voltage"] = plantData.batteryVoltage;
  doc["battery_percentage"] = plantData.batteryPercent;
  doc["status"] = plantData.status;
  doc["plant_name"] = plantData.plantName;
  doc["rssi"] = plantData.rssi;
  doc["snr"] = plantData.snr;
  
  // Histórico
  JsonArray historyArray = doc.createNestedArray("history");
  for (int i = 0; i < HISTORY_SIZE; i++) {
    JsonObject point = historyArray.createNestedObject();
    point["time"] = String(i);
    point["magnitude"] = magnitudeHistory[(historyIndex + i) % HISTORY_SIZE];
  }
  
  // Bandas
  JsonArray bandsArray = doc.createNestedArray("bands");
  for (int i = 0; i < plantData.numBands; i++) {
    JsonObject band = bandsArray.createNestedObject();
    band["name"] = plantData.bands[i].name;
    band["range"] = String(plantData.bands[i].minFreq) + "-" + String(plantData.bands[i].maxFreq) + " Hz";
    band["magnitude"] = plantData.bands[i].magnitude;
    band["magnitude_db"] = plantData.bands[i].magnitudeDb;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIPlants() {
  DynamicJsonDocument doc(2048);
  JsonArray plantsArray = doc.to<JsonArray>();
  
  JsonObject plant1 = plantsArray.createNestedObject();
  plant1["id"] = 1;
  plant1["name"] = plantData.plantName;
  plant1["type"] = plantData.plantType;
  plant1["location"] = "Sensor LoRa";
  plant1["status"] = plantData.status;
  plant1["communication_frequency"] = plantData.dominantFreq;
  plant1["health_score"] = (plantData.status == "online") ? 95 : 60;
  plant1["last_communication"] = "Agora";
  plant1["signal_strength"] = (plantData.rssi > -70) ? "Forte" : (plantData.rssi > -90) ? "Medio" : "Fraco";
  plant1["daily_communications"] = stats.totalPackets;
  plant1["peak_frequency"] = stats.maxFreqRecorded;
  plant1["sensor_voltage"] = plantData.voltage;
  plant1["battery_percentage"] = plantData.batteryPercent;
  plant1["battery_voltage"] = plantData.batteryVoltage;
  plant1["rssi"] = plantData.rssi;
  plant1["snr"] = plantData.snr;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsSummary() {
  DynamicJsonDocument doc(2048);
  
  doc["total_plants"] = 1;
  doc["active_plants"] = stats.plantsActive;
  doc["total_communications_today"] = stats.totalPackets;
  doc["average_frequency"] = plantData.dominantFreq;
  doc["peak_magnitude"] = stats.peakMagnitude;
  doc["max_frequency_recorded"] = stats.maxFreqRecorded;
  doc["packets_lost"] = stats.packetsLost;
  doc["uptime_seconds"] = (millis() - stats.sessionStart) / 1000;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsHistory() {
  DynamicJsonDocument doc(2048);
  JsonArray historyArray = doc.to<JsonArray>();
  
  // Simular histórico de 24 horas
  for (int i = 0; i < 24; i++) {
    JsonObject point = historyArray.createNestedObject();
    point["hour"] = i;
    point["communications"] = random(5, 30);
    point["avgFreq"] = random(100, 2000);
    point["avgMagnitude"] = random(1, 100) / 1000.0;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIAnalyticsBands() {
  DynamicJsonDocument doc(2048);
  JsonArray bandsArray = doc.to<JsonArray>();
  
  for (int i = 0; i < plantData.numBands; i++) {
    JsonObject band = bandsArray.createNestedObject();
    band["name"] = plantData.bands[i].name;
    band["minFreq"] = plantData.bands[i].minFreq;
    band["maxFreq"] = plantData.bands[i].maxFreq;
    band["magnitude"] = plantData.bands[i].magnitude;
    band["magnitudeDb"] = plantData.bands[i].magnitudeDb;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleNotFound() {
  server.send(404, "text/plain", "Pagina nao encontrada");
}

// ==================== PÁGINA HTML ====================

String getHTMLPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Plantas que Falam - Dashboard LoRa</title>
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
            padding: 20px;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        
        header {
            text-align: center;
            margin-bottom: 40px;
            padding: 30px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 20px;
            backdrop-filter: blur(10px);
        }
        
        h1 {
            font-size: 2.5em;
            color: #22c55e;
            margin-bottom: 10px;
            text-shadow: 0 0 20px rgba(34, 197, 94, 0.5);
        }
        
        .subtitle {
            color: #a0aec0;
            font-size: 1.1em;
        }
        
        .status-bar {
            display: flex;
            justify-content: space-between;
            padding: 20px;
            background: rgba(34, 197, 94, 0.1);
            border-radius: 15px;
            margin-bottom: 30px;
            border: 2px solid rgba(34, 197, 94, 0.3);
            flex-wrap: wrap;
            gap: 15px;
        }
        
        .status-item {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            animation: pulse 2s ease-in-out infinite;
        }
        
        .status-dot.online { background: #22c55e; box-shadow: 0 0 15px #22c55e; }
        .status-dot.offline { background: #ef4444; box-shadow: 0 0 15px #ef4444; }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }
        
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
            gap: 25px;
            margin-bottom: 30px;
        }
        
        .card {
            background: rgba(0, 0, 0, 0.3);
            border-radius: 20px;
            padding: 30px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
            transition: transform 0.3s ease;
        }
        
        .card:hover {
            transform: translateY(-5px);
            border-color: rgba(34, 197, 94, 0.5);
        }
        
        .card-icon {
            font-size: 2.5em;
            margin-bottom: 15px;
            display: block;
        }
        
        .card-title {
            font-size: 0.9em;
            color: #a0aec0;
            margin-bottom: 10px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .card-value {
            font-size: 2.5em;
            color: #22c55e;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        
        .card-unit {
            color: #718096;
            font-size: 1em;
            margin-top: 5px;
        }
        
        .chart-container {
            background: rgba(0, 0, 0, 0.3);
            border-radius: 20px;
            padding: 30px;
            margin-bottom: 30px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        
        .chart-title {
            font-size: 1.3em;
            color: #22c55e;
            margin-bottom: 20px;
        }
        
        .bands-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
            margin-top: 20px;
        }
        
        .band-card {
            background: rgba(0, 0, 0, 0.2);
            padding: 15px;
            border-radius: 10px;
            border-left: 4px solid #22c55e;
        }
        
        .band-name {
            font-weight: bold;
            margin-bottom: 5px;
            color: #22c55e;
        }
        
        .band-value {
            font-size: 1.2em;
            color: white;
            font-family: 'Courier New', monospace;
        }
        
        footer {
            text-align: center;
            padding: 20px;
            color: #718096;
            margin-top: 40px;
        }
        
        @media (max-width: 768px) {
            h1 { font-size: 1.8em; }
            .card-value { font-size: 2em; }
            .grid { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🌱 Plantas que Falam - Gateway LoRa</h1>
            <p class="subtitle">Sistema de Monitoramento via LoRa em Tempo Real</p>
        </header>
        
        <div class="status-bar">
            <div class="status-item">
                <div class="status-dot online" id="statusDot"></div>
                <span id="statusText">Sistema Ativo</span>
            </div>
            <div class="status-item">
                <span id="lastUpdate">Última atualização: --</span>
            </div>
            <div class="status-item">
                <span>📡 LoRa 915 MHz</span>
            </div>
        </div>
        
        <div class="grid">
            <div class="card">
                <span class="card-icon">🌿</span>
                <div class="card-title">Planta</div>
                <div class="card-value" style="font-size: 1.5em;" id="plantName">--</div>
                <div class="card-unit" id="plantType">--</div>
            </div>
            
            <div class="card">
                <span class="card-icon">〰️</span>
                <div class="card-title">Frequência Dominante</div>
                <div class="card-value" id="frequency">--</div>
                <div class="card-unit">Hz</div>
            </div>
            
            <div class="card">
                <span class="card-icon">📊</span>
                <div class="card-title">Magnitude</div>
                <div class="card-value" id="magnitude">--</div>
                <div class="card-unit">Amplitude</div>
            </div>
            
            <div class="card">
                <span class="card-icon">⚡</span>
                <div class="card-title">Tensão Sensor</div>
                <div class="card-value" id="voltage">--</div>
                <div class="card-unit">Volts</div>
            </div>
            
            <div class="card">
                <span class="card-icon">🔋</span>
                <div class="card-title">Bateria</div>
                <div class="card-value" id="battery">--</div>
                <div class="card-unit" id="batteryVoltage">-- V</div>
            </div>
            
            <div class="card">
                <span class="card-icon">📡</span>
                <div class="card-title">Sinal LoRa (RSSI)</div>
                <div class="card-value" id="rssi">--</div>
                <div class="card-unit">dBm (SNR: <span id="snr">--</span> dB)</div>
            </div>
            
            <div class="card">
                <span class="card-icon">📦</span>
                <div class="card-title">Pacotes Recebidos</div>
                <div class="card-value" id="packets">0</div>
                <div class="card-unit">Total</div>
            </div>
            
            <div class="card">
                <span class="card-icon">📈</span>
                <div class="card-title">Valor ADC</div>
                <div class="card-value" id="rawValue">--</div>
                <div class="card-unit">0-4095</div>
            </div>
        </div>
        
        <div class="chart-container">
            <div class="chart-title">📈 Histórico de Magnitude</div>
            <canvas id="magnitudeChart"></canvas>
        </div>
        
        <div class="chart-container">
            <div class="chart-title">🎵 Bandas de Frequência</div>
            <div class="bands-grid" id="bandsContainer">
                 Bandas serão inseridas aqui 
            </div>
        </div>
        
        <footer>
            <p>Sistema Gateway LoRa | ESP32 + Sensor Piezoelétrico</p>
            <p style="margin-top: 10px;">Monitoramento de Comunicação entre Plantas</p>
        </footer>
    </div>
    
    <script>
        // Configurar gráfico
        const ctx = document.getElementById('magnitudeChart').getContext('2d');
        const magnitudeChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'Magnitude',
                    data: [],
                    borderColor: '#22c55e',
                    backgroundColor: 'rgba(34, 197, 94, 0.1)',
                    tension: 0.4,
                    fill: true
                }]
            },
            options: {
                responsive: true,
                plugins: {
                    legend: { display: false }
                },
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: { color: 'rgba(255, 255, 255, 0.1)' },
                        ticks: { color: '#a0aec0' }
                    },
                    x: {
                        grid: { color: 'rgba(255, 255, 255, 0.1)' },
                        ticks: { color: '#a0aec0' }
                    }
                }
            }
        });
        
        async function atualizarDados() {
            try {
                const resposta = await fetch('/api/data');
                const dados = await resposta.json();
                
                // Atualizar valores
                document.getElementById('plantName').textContent = dados.plant_name;
                document.getElementById('frequency').textContent = dados.dominant_frequency.toFixed(1);
                document.getElementById('magnitude').textContent = dados.dominant_magnitude.toFixed(4);
                document.getElementById('voltage').textContent = dados.voltage.toFixed(2);
                document.getElementById('battery').textContent = dados.battery_percentage.toFixed(0) + '%';
                document.getElementById('batteryVoltage').textContent = dados.battery_voltage.toFixed(2) + ' V';
                document.getElementById('rssi').textContent = dados.rssi;
                document.getElementById('snr').textContent = dados.snr.toFixed(1);
                document.getElementById('rawValue').textContent = dados.raw_value;
                
                // Atualizar status
                const statusDot = document.getElementById('statusDot');
                const statusText = document.getElementById('statusText');
                if (dados.status === 'online') {
                    statusDot.className = 'status-dot online';
                    statusText.textContent = 'Planta Comunicando';
                } else {
                    statusDot.className = 'status-dot offline';
                    statusText.textContent = 'Aguardando Sinal';
                }
                
                // Atualizar gráfico
                if (dados.history && dados.history.length > 0) {
                    magnitudeChart.data.labels = dados.history.map(p => p.time);
                    magnitudeChart.data.datasets[0].data = dados.history.map(p => p.magnitude);
                    magnitudeChart.update();
                }
                
                // Atualizar bandas
                if (dados.bands && dados.bands.length > 0) {
                    const bandsContainer = document.getElementById('bandsContainer');
                    bandsContainer.innerHTML = '';
                    dados.bands.forEach(band => {
                        const bandCard = document.createElement('div');
                        bandCard.className = 'band-card';
                        bandCard.innerHTML = `
                            <div class="band-name">${band.name}</div>
                            <div class="band-value">${band.magnitude.toFixed(3)}</div>
                            <div class="card-unit">${band.range}</div>
                            <div class="card-unit">${band.magnitude_db.toFixed(1)} dB</div>
                        `;
                        bandsContainer.appendChild(bandCard);
                    });
                }
                
                // Atualizar timestamp
                const agora = new Date();
                document.getElementById('lastUpdate').textContent = 
                    'Última atualização: ' + agora.toLocaleTimeString('pt-BR');
                
                // Buscar estatísticas de plantas
                const respostaPlantas = await fetch('/api/plants');
                const plantas = await respostaPlantas.json();
                if (plantas.length > 0) {
                    document.getElementById('plantType').textContent = plantas[0].type;
                    document.getElementById('packets').textContent = plantas[0].daily_communications;
                }
                
            } catch (erro) {
                console.error('Erro ao atualizar dados:', erro);
            }
        }
        
        // Atualizar a cada 1 segundo
        setInterval(atualizarDados, 1000);
        
        // Atualizar imediatamente
        window.addEventListener('load', atualizarDados);
        
        console.log('%c🌱 Dashboard Plantas que Falam - Gateway LoRa', 
                    'color: #22c55e; font-size: 16px; font-weight: bold;');
    </script>
</body>
</html>
  )rawliteral";
}
