/*
 * ESP32 Piezoelectric Sensor - WEB DASHBOARD COMPLETO
 * Para ESP32 LoRa LilyGO com display OLED
 * Sensor conectado no GPIO 34 SEM resistor pull-down
 * 
 * RECURSOS:
 * - Display OLED com dados em tempo real
 * - Servidor Web com dashboard moderno
 * - Interface responsiva e bonita
 * - Auto-refresh via JavaScript (sem WebSocket)
 * - Funciona com bibliotecas padrÃ£o do ESP32
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "arduinoFFT.h"

// ConfiguraÃ§Ã£o do Display OLED (LilyGO)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ConfiguraÃ§Ãµes WiFi
const char* ssid = "iPhone";
const char* password = "123456789";

// ConfiguraÃ§Ãµes FFT
#define SAMPLES 512              // NÃºmero de amostras (deve ser potÃªncia de 2)
#define SAMPLING_FREQUENCY 10000 // FrequÃªncia de amostragem em Hz
#define PIEZO_PIN 34            // Pino analÃ³gico do sensor

// VariÃ¡veis FFT
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);

// Servidor Web
WebServer server(80);

// VariÃ¡veis de controle
unsigned long lastDisplay = 0;
const unsigned long displayInterval = 200; // Atualizar display a cada 200ms

// VariÃ¡veis globais para dados
double dominantFreq = 0;
double maxMagnitude = 0;
double dominantDb = -80;
int rawSensorValue = 0;
double sensorVoltage = 0;
double avgMagnitude = 0;

// Status de conexÃ£o
bool wifiConnected = false;
int webRequests = 0;

// HistÃ³rico para grÃ¡fico simples
#define HISTORY_SIZE 20
double magnitudeHistory[HISTORY_SIZE];
int historyIndex = 0;

// Bandas de frequÃªncia para anÃ¡lise (em Hz)
struct FrequencyBand {
  String name;
  double minFreq;
  double maxFreq;
  double magnitude;
  double magnitudeDb;
};

FrequencyBand bands[] = {
  {"Sub Bass", 20, 60, 0, -80},
  {"Bass", 60, 250, 0, -80},
  {"Low Mid", 250, 500, 0, -80},
  {"Mid", 500, 2000, 0, -80},
  {"High Mid", 2000, 4000, 0, -80},
  {"Presence", 4000, 6000, 0, -80},
  {"Brilliance", 6000, 20000, 0, -80}
};

const int numBands = sizeof(bands) / sizeof(bands[0]);

void setup() {
  Serial.begin(115200);
  
  // Inicializar display
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Configurar pino do sensor
  pinMode(PIEZO_PIN, INPUT);
  
  // Inicializar histÃ³rico
  for (int i = 0; i < HISTORY_SIZE; i++) {
    magnitudeHistory[i] = 0;
  }
  
  // Tela de inicializaÃ§Ã£o
  showStartupScreen();
  
  // Conectar WiFi
  setupWiFi();
  
  // Configurar servidor web
  setupWebServer();
  
  Serial.println("=== SISTEMA WEB INICIADO ===");
  Serial.println("ConfiguraÃ§Ãµes:");
  Serial.printf("- Pino sensor: GPIO %d\n", PIEZO_PIN);
  Serial.printf("- FrequÃªncia amostragem: %d Hz\n", SAMPLING_FREQUENCY);
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
  u8g2.drawStr(0, 15, "ESP32 Piezo Web");
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
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
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
    Serial.println("Falha na conexÃ£o WiFi!");
    
    // Mostrar erro no display
    u8g2.clearBuffer();
    u8g2.drawStr(0, 15, "WiFi FALHOU!");
    u8g2.drawStr(0, 30, "Modo offline");
    u8g2.sendBuffer();
    delay(2000);
  }
}

void setupWebServer() {
  // PÃ¡gina principal
  server.on("/", handleRoot);
  
  // API para dados JSON
  server.on("/api/data", handleAPI);
  
  // Iniciar servidor
  server.begin();
  Serial.println("Servidor web iniciado na porta 80");
}

void handleRoot() {
  webRequests++;
  String html = getHTMLPage();
  server.send(200, "text/html", html);
}

void handleAPI() {
  String json = getJSONData();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
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
  
  magnitudeHistory[historyIndex] = maxMagnitude;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
}

void updateDisplay() {
  u8g2.clearBuffer();
  
  // Status de conexÃ£o e requests
  u8g2.setFont(u8g2_font_5x7_tf);
  String statusStr = "";
  if (wifiConnected) {
    statusStr += "WiFi:OK ";
  } else {
    statusStr += "WiFi:-- ";
  }
  statusStr += "Req:" + String(webRequests);
  u8g2.drawStr(0, 8, statusStr.c_str());
  
  // Valor bruto do sensor
  u8g2.setFont(u8g2_font_6x10_tf);
  char rawStr[32];
  sprintf(rawStr, "RAW: %d (%.2fV)", rawSensorValue, sensorVoltage);
  u8g2.drawStr(0, 20, rawStr);
  
  // FrequÃªncia dominante
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
    sprintf(statusActivityStr, "STATUS: ATIVO!");
  } else if (maxMagnitude > 0.001) {
    sprintf(statusActivityStr, "STATUS: baixo");
  } else {
    sprintf(statusActivityStr, "STATUS: silencio");
  }
  u8g2.drawStr(0, 56, statusActivityStr);
  
  // GrÃ¡fico simples no lado direito
  int graphX = 90;
  int graphY = 64;
  int graphW = 38;
  int graphH = 30;
  
  double maxHist = 0.001;
  for (int i = 0; i < HISTORY_SIZE; i++) {
    if (magnitudeHistory[i] > maxHist) {
      maxHist = magnitudeHistory[i];
    }
  }
  
  for (int i = 0; i < HISTORY_SIZE - 1; i++) {
    int x1 = graphX + (i * graphW) / HISTORY_SIZE;
    int x2 = graphX + ((i + 1) * graphW) / HISTORY_SIZE;
    
    int y1 = graphY - (magnitudeHistory[i] * graphH) / maxHist;
    int y2 = graphY - (magnitudeHistory[(i + 1) % HISTORY_SIZE] * graphH) / maxHist;
    
    if (y1 > graphY) y1 = graphY;
    if (y2 > graphY) y2 = graphY;
    if (y1 < graphY - graphH) y1 = graphY - graphH;
    if (y2 < graphY - graphH) y2 = graphY - graphH;
    
    u8g2.drawLine(x1, y1, x2, y2);
  }
  
  u8g2.drawFrame(graphX - 1, graphY - graphH - 1, graphW + 2, graphH + 2);
  
  u8g2.sendBuffer();
}

String getJSONData() {
  DynamicJsonDocument doc(2048);
  
  doc["timestamp"] = millis();
  doc["raw_value"] = rawSensorValue;
  doc["voltage"] = sensorVoltage;
  doc["dominant_frequency"] = dominantFreq;
  doc["dominant_magnitude"] = maxMagnitude;
  doc["dominant_magnitude_db"] = dominantDb;
  doc["average_magnitude"] = avgMagnitude;
  
  // HistÃ³rico para grÃ¡fico
  JsonArray historyArray = doc.createNestedArray("history");
  for (int i = 0; i < HISTORY_SIZE; i++) {
    int idx = (historyIndex + i) % HISTORY_SIZE;
    historyArray.add(magnitudeHistory[idx]);
  }
  
  // Bandas de frequÃªncia
  JsonArray bandsArray = doc.createNestedArray("bands");
  for (int i = 0; i < numBands; i++) {
    JsonObject band = bandsArray.createNestedObject();
    band["name"] = bands[i].name;
    band["min_freq"] = bands[i].minFreq;
    band["max_freq"] = bands[i].maxFreq;
    band["magnitude"] = bands[i].magnitude;
    band["magnitude_db"] = bands[i].magnitudeDb;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

String getHTMLPage() {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Piezo Analyzer</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            color: white;
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        
        .header h1 {
            font-size: 2.5rem;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        
        .status {
            display: inline-block;
            padding: 8px 16px;
            background: rgba(255,255,255,0.1);
            border-radius: 20px;
            backdrop-filter: blur(10px);
        }
        
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .card {
            background: rgba(255,255,255,0.1);
            border-radius: 15px;
            padding: 20px;
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.2);
            box-shadow: 0 8px 32px rgba(0,0,0,0.1);
        }
        
        .card h3 {
            margin-bottom: 15px;
            color: #64b5f6;
            font-size: 1.2rem;
        }
        
        .metric {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
            padding: 8px 0;
            border-bottom: 1px solid rgba(255,255,255,0.1);
        }
        
        .metric:last-child {
            border-bottom: none;
        }
        
        .metric-label {
            font-weight: 500;
        }
        
        .metric-value {
            font-weight: bold;
            color: #81c784;
        }
        
        .frequency-display {
            text-align: center;
            padding: 20px;
            background: linear-gradient(45deg, #ff6b6b, #ee5a24);
            border-radius: 15px;
            margin-bottom: 20px;
        }
        
        .frequency-value {
            font-size: 3rem;
            font-weight: bold;
            margin-bottom: 5px;
        }
        
        .frequency-unit {
            font-size: 1.2rem;
            opacity: 0.8;
        }
        
        .bands-container {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
        }
        
        .band {
            text-align: center;
            padding: 15px;
            background: rgba(255,255,255,0.05);
            border-radius: 10px;
            border: 1px solid rgba(255,255,255,0.1);
        }
        
        .band-name {
            font-size: 0.9rem;
            margin-bottom: 8px;
            color: #90caf9;
        }
        
        .band-value {
            font-size: 1.1rem;
            font-weight: bold;
            color: #a5d6a7;
        }
        
        .band-db {
            font-size: 0.8rem;
            opacity: 0.7;
            margin-top: 4px;
        }
        
        .chart-container {
            position: relative;
            height: 300px;
            margin-top: 20px;
        }
        
        .activity-indicator {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 50%;
            margin-right: 8px;
        }
        
        .active { background: #4caf50; }
        .low { background: #ff9800; }
        .silent { background: #f44336; }
        
        @keyframes pulse {
            0% { transform: scale(1); }
            50% { transform: scale(1.1); }
            100% { transform: scale(1); }
        }
        
        .pulse {
            animation: pulse 1s infinite;
        }
        
        .loading {
            text-align: center;
            padding: 20px;
            opacity: 0.7;
        }
        
        @media (max-width: 768px) {
            .header h1 {
                font-size: 2rem;
            }
            
            .frequency-value {
                font-size: 2rem;
            }
            
            .grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ðŸŽµ ESP32 Piezo Analyzer</h1>
            <div class="status">
                <span class="activity-indicator" id="activityIndicator"></span>
                <span id="statusText">Carregando...</span>
            </div>
        </div>
        
        <div class="frequency-display">
            <div class="frequency-value" id="dominantFreq">0</div>
            <div class="frequency-unit">Hz</div>
        </div>
        
        <div class="grid">
            <div class="card">
                <h3>ðŸ“Š Dados do Sensor</h3>
                <div class="metric">
                    <span class="metric-label">Valor Bruto:</span>
                    <span class="metric-value" id="rawValue">0</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Voltagem:</span>
                    <span class="metric-value" id="voltage">0.00 V</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Magnitude:</span>
                    <span class="metric-value" id="magnitude">0.000</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Magnitude dB:</span>
                    <span class="metric-value" id="magnitudeDb">-80 dB</span>
                </div>
                <div class="metric">
                    <span class="metric-label">Magnitude MÃ©dia:</span>
                    <span class="metric-value" id="avgMagnitude">0.000</span>
                </div>
            </div>
            
            <div class="card">
                <h3>ðŸ“ˆ GrÃ¡fico em Tempo Real</h3>
                <div class="chart-container">
                    <canvas id="magnitudeChart"></canvas>
                </div>
            </div>
        </div>
        
        <div class="card">
            <h3>ðŸŽ›ï¸ Bandas de FrequÃªncia</h3>
            <div class="bands-container" id="bandsContainer">
                <div class="loading">Carregando dados...</div>
            </div>
        </div>
    </div>

    <script>
        // Chart setup
        const ctx = document.getElementById('magnitudeChart').getContext('2d');
        const chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: Array.from({length: 20}, (_, i) => i),
                datasets: [{
                    label: 'Magnitude',
                    data: Array(20).fill(0),
                    borderColor: '#64b5f6',
                    backgroundColor: 'rgba(100, 181, 246, 0.1)',
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
                        display: false
                    }
                },
                scales: {
                    x: {
                        display: false
                    },
                    y: {
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(255,255,255,0.1)'
                        },
                        ticks: {
                            color: 'white'
                        }
                    }
                }
            }
        });
        
        function fetchData() {
            fetch('/api/data')
                .then(response => response.json())
                .then(data => updateDisplay(data))
                .catch(error => {
                    console.error('Erro ao buscar dados:', error);
                    document.getElementById('statusText').textContent = 'Erro de conexÃ£o';
                });
        }
        
        function updateDisplay(data) {
            // Atualizar frequÃªncia dominante
            const freq = data.dominant_frequency;
            document.getElementById('dominantFreq').textContent = 
                freq < 1000 ? freq.toFixed(1) : (freq/1000).toFixed(2) + 'k';
            
            // Atualizar dados do sensor
            document.getElementById('rawValue').textContent = data.raw_value;
            document.getElementById('voltage').textContent = data.voltage.toFixed(3) + ' V';
            document.getElementById('magnitude').textContent = data.dominant_magnitude.toFixed(6);
            document.getElementById('magnitudeDb').textContent = data.dominant_magnitude_db.toFixed(1) + ' dB';
            document.getElementById('avgMagnitude').textContent = data.average_magnitude.toFixed(6);
            
            // Atualizar indicador de atividade
            const indicator = document.getElementById('activityIndicator');
            const statusText = document.getElementById('statusText');
            
            if (data.dominant_magnitude > 0.01) {
                indicator.className = 'activity-indicator active pulse';
                statusText.textContent = 'ATIVO!';
            } else if (data.dominant_magnitude > 0.001) {
                indicator.className = 'activity-indicator low';
                statusText.textContent = 'Sinal Baixo';
            } else {
                indicator.className = 'activity-indicator silent';
                statusText.textContent = 'SilÃªncio';
            }
            
            // Atualizar grÃ¡fico
            if (data.history) {
                chart.data.datasets[0].data = data.history;
                chart.update('none');
            }
            
            // Atualizar bandas
            updateBands(data.bands);
        }
        
        function updateBands(bands) {
            const container = document.getElementById('bandsContainer');
            container.innerHTML = '';
            
            bands.forEach(band => {
                const bandElement = document.createElement('div');
                bandElement.className = 'band';
                bandElement.innerHTML = `
                    <div class="band-name">${band.name}</div>
                    <div class="band-value">${band.magnitude.toFixed(3)}</div>
                    <div class="band-db">${band.magnitude_db.toFixed(1)} dB</div>
                `;
                container.appendChild(bandElement);
            });
        }
        
        // Buscar dados a cada 200ms
        setInterval(fetchData, 200);
        
        // Primeira busca
        fetchData();
    </script>
</body>
</html>
)rawliteral";
}
