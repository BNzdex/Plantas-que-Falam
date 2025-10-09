#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Configuração do Display OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Configurações LoRa
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26
#define BAND 915E6

// Configurações WiFi
const char* ssid = "Redmi Note 13"; // <<<<<<< ATUALIZE COM SEU SSID
const char* password = "12345678"; // <<<<<<< ATUALIZE COM SUA SENHA

// Servidor Web
WebServer server(80);

// Variáveis LoRa
int pacotesRecebidos = 0;
String ultimaMensagem = "";
int ultimoRSSI = -100;
unsigned long ultimaComunicacao = 0;
bool statusSensor = false;

// Dados do sensor recebidos via LoRa
struct SensorData {
  int valorBruto = 0;
  float voltagem = 0.0;
  unsigned long timestamp = 0;
  bool online = false;
} sensorData;

// Histórico para gráficos (últimos 20 pontos)
#define HISTORY_SIZE 20
struct HistoryPoint {
  unsigned long timestamp;
  int sensorValue;
  float voltage;
  int rssi;
} history[HISTORY_SIZE];
int historyIndex = 0;

// Estatísticas da sessão
struct SessionStats {
  unsigned long sessionStart = 0;
  int totalPackets = 0;
  float avgRSSI = -100;
  int maxSensorValue = 0;
  int minSensorValue = 4095;
  float successRate = 0;
} stats;

// Status do sistema
bool wifiConnected = false;
unsigned long systemUptime = 0;

// Protótipos das funções
void setupLoRa();
void setupWiFi();
void setupWebServer();
void checkLoRaMessages();
void addToHistory(int sensorValue, float voltage, int rssi);
void updateStatistics();
void updateDisplay();
void showStartupScreen();
void handleRoot();
void handleAPILoRaData();
void handleAPILoRaHistory();
void handleAPILoRaStats();

void setup() {
  Serial.begin(115200);
  
  // Inicializar display
  Wire.begin(4, 15);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println("Erro no display OLED");
    while (1);
  }
  
  // Tela de inicialização
  showStartupScreen();
  
  // Inicializar LoRa
  setupLoRa();
  
  // Conectar WiFi
  setupWiFi();
  
  // Configurar servidor web
  setupWebServer();
  
  // Inicializar estatísticas
  stats.sessionStart = millis();
  
  Serial.println("=== SISTEMA LORA WEB INICIADO ===");
  Serial.printf("- Frequência LoRa: %.0f MHz\n", BAND / 1E6);
  Serial.printf("- Spreading Factor: 7\n");
  Serial.printf("- Bandwidth: 125 kHz\n");
  if (wifiConnected) {
    Serial.printf("- Dashboard: http://%s\n", WiFi.localIP( ).toString().c_str());
  }
  Serial.println("================================");
}

void loop() {
  // Manter servidor web ativo
  server.handleClient();
  
  // Verificar mensagens LoRa
  checkLoRaMessages();
  
  // Atualizar display
  updateDisplay();
  
  // Atualizar estatísticas
  updateStatistics();
  
  delay(100);
}

void setupLoRa() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Iniciando LoRa...");
  display.display();
  
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(BAND)) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("ERRO LORA!");
    display.display();
    Serial.println("Erro ao inicializar LoRa!");
    while (1);
  }
  
  // Configurar parâmetros LoRa
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("LoRa Pronto!");
  display.println("Aguardando dados...");
  display.display();
  delay(2000);
  
  Serial.println("LoRa inicializado com sucesso!");
}

void setupWiFi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Conectando WiFi...");
  display.println(ssid);
  display.display();
  
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
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Conectado!");
    display.println(WiFi.localIP().toString().c_str());
    display.println("Dashboard ativo!");
    display.display();
    delay(3000);
  } else {
    wifiConnected = false;
    Serial.println("Falha na conexão WiFi!");
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi FALHOU!");
    display.println("Modo offline");
    display.display();
    delay(2000);
  }
}

void setupWebServer() {
  // Configurar CORS para todas as rotas
  server.onNotFound([]() {
    if (server.method() == HTTP_OPTIONS) {
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
      server.send(200);
      return;
    }
    server.send(404, "text/plain", "Not Found");
  });
  
  // Rota principal - Dashboard HTML
  server.on("/", handleRoot);
  
  // API - Dados em tempo real
  server.on("/api/lora/data", handleAPILoRaData);
  
  // API - Histórico
  server.on("/api/lora/history", handleAPILoRaHistory);
  
  // API - Estatísticas
  server.on("/api/lora/stats", handleAPILoRaStats);
  
  server.begin();
  Serial.println("Servidor web iniciado na porta 80");
}

void checkLoRaMessages() {
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    String mensagem = "";
    
    // Ler mensagem
    while (LoRa.available()) {
      mensagem += (char)LoRa.read();
    }
    
    // Obter RSSI
    int rssi = LoRa.packetRssi();
    
    // Processar mensagem (formato esperado: "Piezo: XXXX")
    if (mensagem.startsWith("Piezo: ")) {
      String valorStr = mensagem.substring(7);
      int valor = valorStr.toInt();
      
      // Atualizar dados do sensor
      sensorData.valorBruto = valor;
      sensorData.voltagem = (valor * 3.3) / 4095.0;
      sensorData.timestamp = millis();
      sensorData.online = true;
      
      // Atualizar variáveis de comunicação
      ultimaMensagem = mensagem;
      ultimoRSSI = rssi;
      ultimaComunicacao = millis();
      pacotesRecebidos++;
      statusSensor = true;
      
      // Adicionar ao histórico
      addToHistory(valor, sensorData.voltagem, rssi);
      
      Serial.println("LoRa recebido: " + mensagem);
      Serial.println("RSSI: " + String(rssi) + " dBm");
      Serial.println("Pacotes: " + String(pacotesRecebidos));
    }
  }
  
  // Verificar timeout (considerar offline após 5 segundos sem comunicação)
  if (millis() - ultimaComunicacao > 5000) {
    sensorData.online = false;
    statusSensor = false;
  }
}

void addToHistory(int sensorValue, float voltage, int rssi) {
  history[historyIndex].timestamp = millis();
  history[historyIndex].sensorValue = sensorValue;
  history[historyIndex].voltage = voltage;
  history[historyIndex].rssi = rssi;
  
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

void updateStatistics() {
  systemUptime = millis();
  stats.totalPackets = pacotesRecebidos;
  
  if (pacotesRecebidos > 0) {
    // Calcular RSSI médio
    float totalRSSI = 0;
    int validPoints = 0;
    for (int i = 0; i < HISTORY_SIZE; i++) {
      if (history[i].timestamp > 0) {
        totalRSSI += history[i].rssi;
        validPoints++;
      }
    }
    if (validPoints > 0) {
      stats.avgRSSI = totalRSSI / validPoints;
    }
    
    // Encontrar valores máximo e mínimo
    for (int i = 0; i < HISTORY_SIZE; i++) {
      if (history[i].timestamp > 0) {
        if (history[i].sensorValue > stats.maxSensorValue) {
          stats.maxSensorValue = history[i].sensorValue;
        }
        if (history[i].sensorValue < stats.minSensorValue) {
          stats.minSensorValue = history[i].sensorValue;
        }
      }
    }
    
    // Calcular taxa de sucesso (assumindo 1 pacote por segundo ideal)
    unsigned long sessionDuration = (millis() - stats.sessionStart) / 1000;
    if (sessionDuration > 0) {
      stats.successRate = (float)pacotesRecebidos / sessionDuration * 100;
      if (stats.successRate > 100) stats.successRate = 100;
    }
  }
}

void updateDisplay() {
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate < 500) return; // Atualizar a cada 500ms
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  
  display.println("RECEPTOR LORA WEB");
  display.println("================");
  
  if (sensorData.online) {
    display.println("Status: ONLINE");
    display.print("Sensor: ");
    display.println(sensorData.valorBruto);
    display.print("Voltagem: ");
    display.print(sensorData.voltagem, 2);
    display.println("V");
  } else {
    display.println("Status: OFFLINE");
    display.println("Aguardando dados...");
  }
  
  display.print("RSSI: ");
  display.print(ultimoRSSI);
  display.println(" dBm");
  
  display.print("Pacotes: ");
  display.println(pacotesRecebidos);
  
  if (wifiConnected) {
    display.print("IP: ");
    display.println(WiFi.localIP().toString().c_str());
  }
  
  display.display();
  lastDisplayUpdate = millis();
}

void showStartupScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("SISTEMA LORA WEB");
  display.println("Iniciando...");
  display.println("Freq: 915 MHz");
  display.println("SF: 7, BW: 125kHz");
  display.display();
  delay(2000);
}

// Handlers das APIs

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Sistema LoRa - Dashboard</title>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; }
        .card { background: white; padding: 20px; margin: 10px 0; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .status-online { color: #28a745; font-weight: bold; }
        .status-offline { color: #dc3545; font-weight: bold; }
        .metric { display: inline-block; margin: 10px 20px 10px 0; }
        .metric-value { font-size: 24px; font-weight: bold; color: #007bff; }
        .metric-label { font-size: 14px; color: #666; }
        h1 { color: #333; text-align: center; }
        h2 { color: #555; border-bottom: 2px solid #007bff; padding-bottom: 5px; }
        .refresh-btn { background: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 4px; cursor: pointer; }
        .refresh-btn:hover { background: #0056b3; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔗 Sistema LoRa - Monitoramento de Sensor Piezoelétrico</h1>
        
        <div class="card">
            <h2>Status do Sistema</h2>
            <button class="refresh-btn" onclick="location.reload()">&#x1F504; Atualizar</button>
            <div id="status-info">Carregando...</div>
        </div>
        
        <div class="card">
            <h2>📊 Dados do Sensor</h2>
            <div id="sensor-data">Carregando...</div>
        </div>
        
        <div class="card">
            <h2>📡 Comunicação LoRa</h2>
            <div id="lora-data">Carregando...</div>
        </div>
        
        <div class="card">
            <h2>📈 Estatísticas da Sessão</h2>
            <div id="stats-data">Carregando...</div>
        </div>
    </div>

    <script>
        function updateData() {
            fetch("/api/lora/data")
                .then(response => response.json())
                .then(data => {
                    // Status
                    document.getElementById("status-info").innerHTML = 
                        "<div class=\"metric\"><div class=\"metric-value " + 
                        (data.sensor.status === "online" ? "status-online\">ONLINE" : "status-offline\">OFFLINE") + 
                        "</div><div class=\"metric-label\">Status do Sensor</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.system.ip_address + 
                        "</div><div class=\"metric-label\">Endereço IP</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + Math.floor(data.system.uptime/1000) + 
                        "s</div><div class=\"metric-label\">Uptime</div></div>";
                    
                    // Sensor
                    document.getElementById("sensor-data").innerHTML = 
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.sensor.raw_value + 
                        "</div><div class=\"metric-label\">Valor Bruto</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.sensor.voltage.toFixed(2) + 
                        "V</div><div class=\"metric-label\">Voltagem</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + new Date(data.timestamp).toLocaleTimeString() + 
                        "</div><div class=\"metric-label\">Última Leitura</div></div>";
                    
                    // LoRa
                    document.getElementById("lora-data").innerHTML = 
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.communication.rssi + 
                        " dBm</div><div class=\"metric-label\">RSSI</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.communication.packets_received + 
                        "</div><div class=\"metric-label\">Pacotes Recebidos</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.communication.signal_quality.toUpperCase() + 
                        "</div><div class=\"metric-label\">Qualidade do Sinal</div></div>";
                })
                .catch(error => console.error("Erro:", error));
            
            fetch("/api/lora/stats")
                .then(response => response.json())
                .then(data => {
                    document.getElementById("stats-data").innerHTML = 
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.total_packets + 
                        "</div><div class=\"metric-label\">Total de Pacotes</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.avg_rssi.toFixed(0) + 
                        " dBm</div><div class=\"metric-label\">RSSI Médio</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.max_sensor_value + 
                        "</div><div class=\"metric-label\">Valor Máximo</div></div>" +
                        "<div class=\"metric\"><div class=\"metric-value\">" + data.success_rate.toFixed(1) + 
                        "%</div><div class=\"metric-label\">Taxa de Sucesso</div></div>";
                })
                .catch(error => console.error("Erro:", error));
        }
        
        // Atualizar dados a cada 2 segundos
        updateData();
        setInterval(updateData, 2000);
    </script>
</body>
</html>
)rawliteral";
  
  server.send(200, "text/html", html);
}

void handleAPILoRaData() {
  DynamicJsonDocument doc(1024);
  
  doc["timestamp"] = millis();
  
  // Dados do sensor
  JsonObject sensor = doc.createNestedObject("sensor");
  sensor["raw_value"] = sensorData.valorBruto;
  sensor["voltage"] = sensorData.voltagem;
  sensor["status"] = sensorData.online ? "online" : "offline";
  
  // Dados de comunicação
  JsonObject communication = doc.createNestedObject("communication");
  communication["rssi"] = ultimoRSSI;
  communication["packets_received"] = pacotesRecebidos;
  communication["last_packet"] = ultimaComunicacao;
  
  // Determinar qualidade do sinal
  String signalQuality = "poor";
  if (ultimoRSSI > -60) signalQuality = "excellent";
  else if (ultimoRSSI > -70) signalQuality = "good";
  else if (ultimoRSSI > -80) signalQuality = "fair";
  
  communication["signal_quality"] = signalQuality;
  
  // Dados do sistema
  JsonObject system = doc.createNestedObject("system");
  system["uptime"] = systemUptime;
  system["wifi_status"] = wifiConnected ? "connected" : "disconnected";
  system["ip_address"] = wifiConnected ? WiFi.localIP().toString() : "0.0.0.0";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPILoRaHistory() {
  DynamicJsonDocument doc(2048);
  
  JsonArray historyArray = doc.createNestedArray("history");
  
  for (int i = 0; i < HISTORY_SIZE; i++) {
    int index = (historyIndex + i) % HISTORY_SIZE;
    if (history[index].timestamp > 0) {
      JsonObject point = historyArray.createNestedObject();
      point["timestamp"] = history[index].timestamp;
      point["sensor_value"] = history[index].sensorValue;
      point["voltage"] = history[index].voltage;
      point["rssi"] = history[index].rssi;
      point["time"] = String(i); // Para compatibilidade com o frontend
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPILoRaStats() {
  DynamicJsonDocument doc(512);
  
  doc["total_packets"] = stats.totalPackets;
  doc["success_rate"] = stats.successRate;
  doc["session_duration"] = (millis() - stats.sessionStart) / 1000;
  doc["avg_rssi"] = stats.avgRSSI;
  doc["max_sensor_value"] = stats.maxSensorValue;
  doc["min_sensor_value"] = stats.minSensorValue;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}
