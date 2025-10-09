#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26
#define BAND 915E6

#define PIEZO_PIN 34  // Pino analógico do sensor piezoelétrico

unsigned long ultimaLeitura = 0;
float ultimaTensao = 0;
float ultimaFrequencia = 0;
int ultimoEstado = 0;
unsigned long tempoUltimoPico = 0;
int contadorPicos = 0;

void setup() {
  Serial.begin(115200);
  
  Wire.begin(21, 22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erro no display");
    while(1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("TRANSMISSOR");
  display.println("Iniciando...");
  display.display();
  delay(1500);

  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(BAND)) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("ERRO LORA!");
    display.display();
    while(1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(20);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("LoRa Pronto!");
  display.display();
  delay(1000);
}

void loop() {
  int leitura = analogRead(PIEZO_PIN);
  float tensao = (leitura * 3.3) / 4095.0;

  // Detectar frequência aproximada (contando picos)
  int estadoAtual = (leitura > 2000); // 2000 é um limiar de vibração
  if (estadoAtual && !ultimoEstado) {
    unsigned long agora = micros();
    if (tempoUltimoPico != 0) {
      float periodo = (agora - tempoUltimoPico) / 1000000.0;
      ultimaFrequencia = 1.0 / periodo;
    }
    tempoUltimoPico = agora;
  }
  ultimoEstado = estadoAtual;

  // Montar mensagem LoRa
  String mensagem = "Valor:" + String(leitura) + ",Tensao:" + String(tensao, 2) + ",Freq:" + String(ultimaFrequencia, 1);
  
  LoRa.beginPacket();
  LoRa.print(mensagem);
  LoRa.endPacket();

  // Mostrar no display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TRANSMISSOR");
  display.println("================");
  display.setTextSize(1);
  display.println("Leitura:");
  display.setTextSize(2);
  display.println(leitura);
  display.setTextSize(1);
  display.print("Tensao: ");
  display.print(tensao, 2);
  display.println("V");
  display.print("Freq: ");
  display.print(ultimaFrequencia, 1);
  display.println("Hz");
  display.display();

  Serial.println("Enviado: " + mensagem);
  delay(10000);
}
