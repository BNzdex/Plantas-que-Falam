#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WebServer.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pinos LoRa
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26
#define BAND 915E6

// Configuração Wi-Fi
const char* ssid = "Bn";
const char* password = "12345687";

WebServer server(80);

String ultimaMensagem = "Aguardando...";
int ultimoRSSI = 0;
int pacotes = 0;
String valor = "--";
String tensao = "--";
String frequencia = "--";

void setup() {
  Serial.begin(115200);
  
  // --- DISPLAY ---
  Wire.begin(4, 15);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    Serial.println("Erro no display");
    while(1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("RECEPTOR");
  display.println("Iniciando...");
  display.display();

  // --- LORA ---
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(BAND)) {
    Serial.println("Erro ao iniciar LoRa");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("ERRO LORA!");
    display.display();
    while(1);
  }
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("LoRa inicializado!");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("LoRa pronto!");
  display.display();

  // --- WI-FI ---
  WiFi.softAP(ssid, password);
  Serial.println();
  Serial.print("Servidor iniciado em: ");
  Serial.println(WiFi.softAPIP());

  // --- ROTAS WEB ---
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  delay(2000);
}

void loop() {
  server.handleClient();

  int tamanho = LoRa.parsePacket();
  if (tamanho) {
    String mensagem = "";
    while (LoRa.available()) {
      mensagem += (char)LoRa.read();
    }

    ultimoRSSI = LoRa.packetRssi();
    pacotes++;
    ultimaMensagem = mensagem;

    // Quebra dos dados
    int idx1 = mensagem.indexOf("Valor:");
    int idx2 = mensagem.indexOf(",Tensao:");
    int idx3 = mensagem.indexOf(",Freq:");

    if (idx1 >= 0 && idx2 >= 0 && idx3 >= 0) {
      valor = mensagem.substring(idx1 + 6, idx2);
      tensao = mensagem.substring(idx2 + 8, idx3);
      frequencia = mensagem.substring(idx3 + 6);
    }

    // --- Display OLED ---
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("RECEPTOR");
    display.println("================");
    display.setTextSize(1);
    display.println("Valor: " + valor);
    display.println("Tensao: " + tensao + "V");
    display.println("Freq: " + frequencia + "Hz");
    display.print("RSSI: ");
    display.println(ultimoRSSI);
    display.display();

    // --- Serial ---
    Serial.println("Recebido: " + mensagem);
    Serial.println("RSSI: " + String(ultimoRSSI));
    Serial.println("Pacotes: " + String(pacotes));
    Serial.println();
  }
}

// ------------------ SERVIDOR WEB ------------------

void handleRoot() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html lang="pt-BR">
    <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Receptor LoRa - Plantas Que Falam</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          background: #0b132b;
          color: #f0f0f0;
          text-align: center;
          padding: 20px;
        }
        h1 { color: #29c7ac; }
        .card {
          background: #1c2541;
          border-radius: 12px;
          box-shadow: 0 0 10px #29c7ac;
          display: inline-block;
          padding: 20px;
          margin: 15px;
          width: 260px;
        }
        span {
          font-size: 1.8em;
          color: #29c7ac;
          display: block;
          margin-top: 10px;
        }
      </style>
      <script>
        async function atualizar() {
          const resp = await fetch('/data');
          const dados = await resp.json();
          document.getElementById('valor').textContent = dados.valor;
          document.getElementById('tensao').textContent = dados.tensao;
          document.getElementById('frequencia').textContent = dados.frequencia;
          document.getElementById('rssi').textContent = dados.rssi;
          document.getElementById('pacotes').textContent = dados.pacotes;
        }
        setInterval(atualizar, 1000);
      </script>
    </head>
    <body>
      <h1>🌱 Plantas Que Falam - Receptor LoRa</h1>
      <div class="card">
        <h2>Valor Piezo</h2>
        <span id="valor">--</span>
      </div>
      <div class="card">
        <h2>Tensão (V)</h2>
        <span id="tensao">--</span>
      </div>
      <div class="card">
        <h2>Frequência (Hz)</h2>
        <span id="frequencia">--</span>
      </div>
      <div class="card">
        <h2>RSSI (dBm)</h2>
        <span id="rssi">--</span>
      </div>
      <div class="card">
        <h2>Pacotes Recebidos</h2>
        <span id="pacotes">--</span>
      </div>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"valor\":\"" + valor + "\",";
  json += "\"tensao\":\"" + tensao + "\",";
  json += "\"frequencia\":\"" + frequencia + "\",";
  json += "\"rssi\":\"" + String(ultimoRSSI) + "\",";
  json += "\"pacotes\":\"" + String(pacotes) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}
