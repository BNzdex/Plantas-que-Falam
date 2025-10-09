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

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);  // ESP32 suporta até 12 bits (0-4095)

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erro no display");
    while (1);
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
    while (1);
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(20);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("LoRa Pronto!");
  display.display();
  delay(1500);
}

void loop() {
  // Leitura do sensor piezoelétrico
  int leitura = analogRead(PIEZO_PIN);

  // Monta mensagem
  String mensagem = "Piezo: " + String(leitura);

  // Envia via LoRa
  LoRa.beginPacket();
  LoRa.print(mensagem);
  LoRa.endPacket();

  // Exibe no display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("TRANSMISSOR");
  display.println("================");
  display.setTextSize(2);
  display.println("Enviado:");
  display.setTextSize(1);
  display.println(mensagem);
  display.display();

  Serial.println("Enviado: " + mensagem);

  delay(1000);  // Ajuste conforme a frequência desejada de leitura
}
