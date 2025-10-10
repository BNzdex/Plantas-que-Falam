#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <ESPmDNS.h>

// ==================== PINOS TTGO LoRa32 V2 ====================
#define OLED_SDA 4
#define OLED_SCL 15
#define OLED_RST 16

#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_CS 18
#define LORA_RST 14
#define LORA_DIO0 26
#define LORA_BAND 915E6

#define LED_PIN 2

// Display OLED
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, OLED_SCL, OLED_SDA, OLED_RST);
bool displayAvailable = false;

// Configuração WiFi
const char* ssid = "7DS GRAND CROSS";
const char* password = "12345678";

WebServer server(80);

// Dados recebidos
struct PlantData {
  String plantName = "Cafesal 01";
  String plantType = "Coffea arabica";
  unsigned int packetNumber = 0;
  double voltage = 0;
  double dominantFreq = 0;
  double maxMagnitude = 0;
  double dominantDb = -80;
  double batteryVoltage = 0;
  double batteryPercent = 0;
  int status = 0;
  int rssi = 0;
  float snr = 0;
  unsigned long lastUpdate = 0;
} plantData;

// Histórico
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
  delay(2000);
  
  Serial.println("\n========================================");
  Serial.println("RECEPTOR/GATEWAY - VERSAO OTIMIZADA");
  Serial.println("========================================\n");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  for (int i = 0; i < HISTORY_SIZE; i++) {
    magnitudeHistory[i] = 0;
  }
  
  stats.sessionStart = millis();

  Serial.print("Inicializando display OLED... ");
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);
  
  if (u8g2.begin()) {
    displayAvailable = true;
    Serial.println("OK!");
    
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 15, "GATEWAY LORA");
    u8g2.drawStr(0, 30, "Iniciando...");
    u8g2.sendBuffer();
  } else {
    displayAvailable = false;
    Serial.println("NAO DETECTADO");
  }
  
  delay(1000);

  Serial.print("Inicializando LoRa em ");
  Serial.print(LORA_BAND / 1E6);
  Serial.print(" MHz... ");
  
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
  
  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("FALHOU!");
    
    if (displayAvailable) {
      u8g2.clearBuffer();
      u8g2.drawStr(0, 15, "ERRO LORA!");
      u8g2.drawStr(0, 30, "Verifique antena");
      u8g2.sendBuffer();
    }
    
    while(1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }
  
  Serial.println("OK!");
  
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();

  if (displayAvailable) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 15, "LoRa OK!");
    u8g2.drawStr(0, 30, "Iniciando WiFi...");
    u8g2.sendBuffer();
  }

  Serial.println("\nConfigurando WiFi Access Point...");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  delay(1000);
  
  IPAddress IP = WiFi.softAPIP();
  wifiConnected = true;
  
  Serial.println("\n========================================");
  Serial.println("WiFi Access Point ATIVO!");
  Serial.println("========================================");
  Serial.printf("IP: %s\n", IP.toString().c_str());
  Serial.println("========================================\n");

  if (MDNS.begin("plantasfalam")) {
    Serial.println("mDNS: http://plantasfalam.local");
  }

  server.on("/", handleRoot);
  server.on("/api/data", handleAPIData);
  server.on("/api/plants", handleAPIPlants);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Servidor HTTP iniciado!\n");

  if (displayAvailable) {
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "SISTEMA PRONTO!");
    u8g2.drawStr(0, 25, ssid);
    u8g2.drawStr(0, 40, IP.toString().c_str());
    u8g2.drawStr(0, 55, "Aguardando LoRa...");
    u8g2.sendBuffer();
  }

  Serial.println("SISTEMA PRONTO - Aguardando pacotes...\n");
  
  digitalWrite(LED_PIN, HIGH);
  delay(2000);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  server.handleClient();
  
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    receiveLoRaData();
  }
  
  if (millis() - lastDisplay > displayInterval) {
    updateDisplay();
    lastDisplay = millis();
  }
  
  if (millis() - plantData.lastUpdate > 5000 && stats.totalPackets > 0) {
    plantData.status = 0;
    stats.plantsActive = 0;
  }
  
  delay(10);
}

void receiveLoRaData() {
  digitalWrite(LED_PIN, HIGH);
  
  String message = "";
  while (LoRa.available()) {
    message += (char)LoRa.read();
  }
  
  plantData.rssi = LoRa.packetRssi();
  plantData.snr = LoRa.packetSnr();
  plantData.lastUpdate = millis();
  stats.totalPackets++;
  
  Serial.println("\n========================================");
  Serial.println("RX PACOTE #" + String(stats.totalPackets));
  Serial.println("========================================");
  Serial.println("Tamanho: " + String(message.length()) + " bytes");
  Serial.println("RSSI: " + String(plantData.rssi) + " dBm");
  Serial.println("SNR: " + String(plantData.snr) + " dB");
  Serial.println("JSON: " + message);
  
  // Usar buffer menor para JSON compacto
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.println("ERRO ao parsear JSON: " + String(error.c_str()));
    Serial.println("========================================\n");
    digitalWrite(LED_PIN, LOW);
    return;
  }
  
  // Processar dados compactos
  plantData.packetNumber = doc["pk"];
  plantData.dominantFreq = doc["f"].as<int>();  // Inteiro -> Double
  plantData.maxMagnitude = doc["m"].as<int>() / 1000.0;  // Dividir por 1000
  plantData.dominantDb = doc["db"].as<int>();
  plantData.batteryVoltage = doc["bv"].as<int>() / 100.0;  // Dividir por 100
  plantData.batteryPercent = doc["bp"].as<int>();
  plantData.status = doc["st"].as<int>();
  
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
  stats.plantsActive = (plantData.status == 1) ? 1 : 0;
  
  Serial.println("\nDados recebidos:");
  Serial.println("Planta: " + plantData.plantName);
  Serial.println("Freq: " + String(plantData.dominantFreq, 2) + " Hz");
  Serial.println("Mag: " + String(plantData.maxMagnitude, 4));
  Serial.println("Bat: " + String(plantData.batteryPercent, 1) + "%");
  Serial.println("Status: " + String(plantData.status ? "ON" : "OFF"));
  Serial.println("========================================\n");
  
  digitalWrite(LED_PIN, LOW);
}

void updateDisplay() {
  if (!displayAvailable) return;
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  
  String statusStr = "WiFi:OK Req:" + String(webRequests);
  u8g2.drawStr(0, 8, statusStr.c_str());
  u8g2.drawLine(0, 10, 128, 10);
  
  u8g2.setFont(u8g2_font_6x10_tf);
  String plantStr = plantData.plantName.substring(0, 16);
  u8g2.drawStr(0, 22, plantStr.c_str());
  
  String pktStr = "Pkt:" + String(plantData.packetNumber) + " R:" + String(plantData.rssi);
  u8g2.drawStr(0, 34, pktStr.c_str());
  
  String freqStr = "F:" + String(plantData.dominantFreq, 0) + "Hz";
  u8g2.drawStr(0, 46, freqStr.c_str());
  
  String magStr = "M:" + String(plantData.maxMagnitude, 3);
  u8g2.drawStr(0, 58, magStr.c_str());
  
  String batStr = "B:" + String(plantData.batteryPercent, 0) + "%";
  u8g2.drawStr(70, 58, batStr.c_str());
  
  u8g2.sendBuffer();
}

void handleRoot() {
  webRequests++;
  String html = getHTMLPage();
  server.send(200, "text/html", html);
}

void handleAPIData() {
  StaticJsonDocument<512> doc;
  
  doc["timestamp"] = millis();
  doc["voltage"] = plantData.voltage;
  doc["dominant_frequency"] = plantData.dominantFreq;
  doc["dominant_magnitude"] = plantData.maxMagnitude;
  doc["dominant_magnitude_db"] = plantData.dominantDb;
  doc["battery_voltage"] = plantData.batteryVoltage;
  doc["battery_percentage"] = plantData.batteryPercent;
  doc["status"] = (plantData.status == 1) ? "on" : "off";
  doc["plant_name"] = plantData.plantName;
  doc["rssi"] = plantData.rssi;
  doc["snr"] = plantData.snr;
  
  JsonArray historyArray = doc.createNestedArray("history");
  for (int i = 0; i < HISTORY_SIZE; i++) {
    historyArray.add(magnitudeHistory[(historyIndex + i) % HISTORY_SIZE]);
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleAPIPlants() {
  StaticJsonDocument<1024> doc;
  JsonArray plantsArray = doc.to<JsonArray>();
  
  JsonObject plant1 = plantsArray.createNestedObject();
  plant1["id"] = 1;
  plant1["name"] = plantData.plantName;
  plant1["type"] = plantData.plantType;
  plant1["status"] = (plantData.status == 1) ? "on" : "off";
  plant1["communication_frequency"] = plantData.dominantFreq;
  plant1["battery_percentage"] = plantData.batteryPercent;
  plant1["battery_voltage"] = plantData.batteryVoltage;
  plant1["rssi"] = plantData.rssi;
  plant1["snr"] = plantData.snr;
  plant1["packets"] = stats.totalPackets;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonString);
}

void handleNotFound() {
  server.send(404, "text/plain", "Pagina nao encontrada");
}

String getHTMLPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Plantas que Falam - Dashboard</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #0f4c3a 0%, #2d5a27 50%, #1a472a 100%);
            color: white;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        header {
            text-align: center;
            margin-bottom: 30px;
            padding: 20px;
            background: rgba(0, 0, 0, 0.3);
            border-radius: 15px;
        }
        h1 {
            font-size: 2em;
            color: #22c55e;
            text-shadow: 0 0 15px rgba(34, 197, 94, 0.5);
        }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
        }
        .card {
            background: rgba(0, 0, 0, 0.3);
            border-radius: 15px;
            padding: 25px;
            border: 1px solid rgba(255, 255, 255, 0.1);
        }
        .card h3 {
            color: #94a3b8;
            font-size: 0.9em;
            margin-bottom: 10px;
        }
        .card-value {
            font-size: 2em;
            color: #22c55e;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🌱 Plantas que Falam</h1>
            <p>Gateway LoRa - TTGO OLED</p>
        </header>
        <div class="grid">
            <div class="card">
                <h3>PLANTA</h3>
                <div class="card-value" style="font-size: 1.3em;" id="plantName">--</div>
            </div>
            <div class="card">
                <h3>FREQUÊNCIA (Hz)</h3>
                <div class="card-value" id="frequency">--</div>
            </div>
            <div class="card">
                <h3>MAGNITUDE</h3>
                <div class="card-value" id="magnitude">--</div>
            </div>
            <div class="card">
                <h3>BATERIA (%)</h3>
                <div class="card-value" id="battery">--</div>
            </div>
            <div class="card">
                <h3>RSSI (dBm)</h3>
                <div class="card-value" id="rssi">--</div>
            </div>
            <div class="card">
                <h3>PACOTES</h3>
                <div class="card-value" id="packets">0</div>
            </div>
        </div>
    </div>
    <script>
        async function atualizar() {
            try {
                const resp = await fetch('/api/data');
                const dados = await resp.json();
                document.getElementById('plantName').textContent = dados.plant_name;
                document.getElementById('frequency').textContent = dados.dominant_frequency.toFixed(1);
                document.getElementById('magnitude').textContent = dados.dominant_magnitude.toFixed(4);
                document.getElementById('battery').textContent = dados.battery_percentage.toFixed(0);
                document.getElementById('rssi').textContent = dados.rssi;
                
                const respPlantas = await fetch('/api/plants');
                const plantas = await respPlantas.json();
                if (plantas.length > 0) {
                    document.getElementById('packets').textContent = plantas[0].packets;
                }
            } catch (e) {
                console.error('Erro:', e);
            }
        }
        setInterval(atualizar, 1000);
        window.addEventListener('load', atualizar);
    </script>
</body>
</html>
  )rawliteral";
}
