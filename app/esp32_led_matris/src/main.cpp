#include <Adafruit_HTU21DF.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>

// --- WiFi Yapılandırması ---
const char *ssid = "***";
const char *password = "***";

// --- UDP Yapılandırması ---
WiFiUDP udp;
unsigned int localUdpPort = 8266;
const char *mdnsName = "getPcStats";

// --- LED Matrix Konfigürasyonu ---
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
// #define HARDWARE_TYPE MD_MAX72XX::GENERIC_HW
#define MAX_DEVICES 4

#define CLK_PIN 12
#define DATA_PIN 11
#define CS_PIN 10

#define TOUCH_PIN 2
#define SDA_PIN 8
#define SCL_PIN 9

MD_MAX72XX mx =
    MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// Enum ve Modlar
enum DisplayMode { AMBIENT, PC_STATS, CLOCK };
volatile DisplayMode currentMode = AMBIENT;

// Global Değerler
volatile float currentCPU = 0, targetCPU = 0;
volatile float currentGPU = 0, targetGPU = 0;
volatile float currentRAM = 0, targetRAM = 0;
volatile float temp = 0, hum = 0, internalTemp = 0;
volatile int hour = 0, minute = 0, second = 0;
volatile unsigned long lastPacketTime = 0;

// Task Handle'lar
TaskHandle_t TaskNet;
TaskHandle_t TaskDisplay;
TaskHandle_t TaskSensor;

// Sensör Nesnesi (SHT21 / GY-21)
Adafruit_HTU21DF sht;

// İkonlar & Sayılar (8x8)
const uint8_t iconSad[] = {0x3C, 0x42, 0xA5, 0x81, 0x99, 0xA5, 0x42, 0x3C};
const uint8_t iconHappy[] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};
const uint8_t iconNeutral[] = {0x3C, 0x42, 0xA5, 0x81, 0xBD, 0x81, 0x42, 0x3C};
const uint8_t iconClock[] = {0x7e, 0x81, 0x91, 0x91, 0x9d, 0x81, 0x81, 0x7e};

// 3x5 font verisi (her sayı için 3 byte - dikey 5 bit)
const uint8_t font3x5[10][3] = {
    {0xF8, 0x88, 0xF8}, // 0
    {0x10, 0xF8, 0x00}, // 1 - Daha sade '1'
    {0xE8, 0xA8, 0xB8}, // 2
    {0xA8, 0xA8, 0xF8}, // 3
    {0x38, 0x20, 0xF8}, // 4
    {0xB8, 0xA8, 0xE8}, // 5
    {0xF8, 0xA8, 0xE8}, // 6
    {0x08, 0x08, 0xF8}, // 7
    {0xF8, 0xA8, 0xF8}, // 8
    {0xB8, 0xA8, 0xF8}  // 9
};

// Yağmur yapısı
struct Drop {
  int8_t x, y;
  bool active;
};
Drop drops[8];

// Fonksiyon Prototipleri
void networkingTask(void *pvParameters);
void displayTask(void *pvParameters);
void sensorTask(void *pvParameters);
void parseJson(char *packetBuffer);
void updateBar(int rowBase, float value);
void drawIcon(const uint8_t *icon, int yOffset);
void drawNumber(int num, int yOffset);
void updateRain();
void checkComfort();
void drawPixel(int x, int y, bool state);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- [START] ESP32 PC Stats & Ambient Monitor v3 ---");

  // 1. Matris Hazırlığı (HESPSİ KAPALI BAŞLAR)
  if (!mx.begin()) {
    Serial.println("[HATA] MD_MAX72XX baslatilamadi!");
    while (1)
      ;
  }
  mx.control(MD_MAX72XX::SHUTDOWN, true);
  mx.clear();
  mx.control(MD_MAX72XX::INTENSITY, 0); // DÜŞÜK PARLAKLIK
  Serial.println("[OK] Matrix baslatildi ve kapatildi (Beklemede).");

  // 2. SHT21 Başlatma
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!sht.begin()) {
    Serial.println("[HATA] SHT21 sensoru bulunamadi!");
  } else {
    Serial.println("[OK] SHT21 basariyla baslatildi.");
  }

  // 3. Pin Girişleri
  pinMode(TOUCH_PIN, INPUT);

  // 4. Tasklar
  xTaskCreatePinnedToCore(networkingTask, "TaskNet", 4096, NULL, 1, &TaskNet,
                          0);
  xTaskCreatePinnedToCore(sensorTask, "TaskSensor", 4096, NULL, 1, &TaskSensor,
                          1);
  xTaskCreatePinnedToCore(displayTask, "TaskDisplay", 4096, NULL, 1,
                          &TaskDisplay, 1);

  // 5. Sistemi Aç
  mx.control(MD_MAX72XX::SHUTDOWN, false);
  Serial.println("--- [OK] Sistem Hazir. ---");
}

void networkingTask(void *pvParameters) {
  Serial.printf("[WIFI] Baglaniyor: %s\n", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.print(".");
  }
  Serial.println("\n[WIFI] Baglandi! IP: " + WiFi.localIP().toString());

  if (MDNS.begin(mdnsName)) {
    MDNS.addService("getPcStats", "udp", localUdpPort);
  }
  udp.begin(localUdpPort);

  char packetBuffer[255];
  while (true) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      int len = udp.read(packetBuffer, 255);
      if (len > 0) {
        packetBuffer[len] = 0;
        parseJson(packetBuffer);
        lastPacketTime = millis();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// --- YARDIMCI GRAFİK FONKSİYONU ---
// Mantıksal (x,y) -> Fiziksel (7-x, y) eşlemesi
// x: 0..7 (Sol -> Sağ)
// y: 0..31 (Aşağı -> Yukarı) - Matris 0 en alt, Mat 3 en üst
void drawPixel(int x, int y, bool state) {
  if (x >= 0 && x < 8 && y >= 0 && y < 32) {
    mx.setPoint(7 - x, y, state);
  }
}

// --- 3x5 Font ile Sayı Çizimi ---
// yOffset: Alt satırın başlangıcı (Örn: 24)
void drawNumber(int num, int yOffset) {
  if (num > 99)
    num = 99;
  if (num < 0)
    num = 0;

  int tens = num / 10;
  int units = num % 10;

  // 180 Derece Dönüş ve Aynalama Düzeltmesi:
  // drawPixel zaten içerde (7-x) yaparak yatayda bir kez çeviriyor.
  // Bizim ekstra (2-i) yapmamız aynalamaya (mirrored) sebep oluyordu.

  // Onlar Basamağı (Fiziksel Solda: x=1,2,3)
  // Saat modunda 01, 02 gibi görünmesi için koşulu kaldırdık
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 5; j++) {
      if ((font3x5[tens][i] >> (7 - j)) & 0x01) {
        // i=0..2 (sol->sağ), j=0..4 (üst->alt)
        // y=yOffset+1+j ile baş aşağı (dikey flip) yapıyoruz.
        drawPixel(1 + i, yOffset + 1 + j, true);
      }
    }
  }

  // Birler Basamağı (Fiziksel Sağda: x=5,6,7)
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 5; j++) {
      if ((font3x5[units][i] >> (7 - j)) & 0x01) {
        drawPixel(5 + i, yOffset + 1 + j, true);
      }
    }
  }
}

void drawIcon(const uint8_t *icon, int yOffset) {
  for (int r = 0; r < 8; r++) {   // Row (icon y)
    for (int c = 0; c < 8; c++) { // Col (icon x)
      if ((icon[r] >> (7 - c)) & 0x01) {
        // Icon yukarıdan aşağıya (r=0 -> En Üst)
        // Ekrana çizerken: y = yOffset + (7-r)
        drawPixel(c, yOffset + (7 - r), true);
      }
    }
  }
}

// Bar Çizimi (PC Stats veya Ambient için yükseklik belirterek)
void updateBar(int colStart, int colEnd, float value, int maxHeight) {
  int height = (int)(value * (float)maxHeight / 100.0f);
  if (height > maxHeight)
    height = maxHeight;
  if (height < 0)
    height = 0;

  for (int y = 0; y < maxHeight; y++) {
    bool on = (y < height);
    for (int x = colStart; x <= colEnd; x++) {
      drawPixel(x, y, on);
    }
  }
}

void updateRain() {
  static unsigned long lastRainMove = 0;
  if (millis() - lastRainMove > 100) {
    for (int i = 0; i < 8; i++) {
      if (drops[i].active) {
        drops[i].y--;
        if (drops[i].y < 0)
          drops[i].active = false;
      } else if (random(100) < 15) {
        drops[i].active = true;
        // User columns: 1-2, 4-5, 7-8 -> Code x: {0,1, 3,4, 6,7}
        int colGroup = random(3);
        if (colGroup == 0)
          drops[i].x = random(2); // 0 or 1
        else if (colGroup == 1)
          drops[i].x = 3 + random(2); // 3 or 4
        else
          drops[i].x = 6 + random(2); // 6 or 7

        drops[i].y = 31;
      }
    }
    lastRainMove = millis();
  }

  for (int i = 0; i < 8; i++) {
    if (drops[i].active)
      drawPixel(drops[i].x, drops[i].y, true);
  }
}

void checkComfort() {
  const uint8_t *icon = iconNeutral;
  bool isHappy = (temp >= 19 && temp <= 27 && hum >= 35 && hum <= 65);
  bool isSad = (temp < 16 || temp > 30 || hum < 25 || hum > 80);

  if (isHappy)
    icon = iconHappy;
  else if (isSad)
    icon = iconSad;

  // Matris 0 (y=0..7) -> Yüz (DIN Tarafı - Alt)
  drawIcon(icon, 0);

  // Matris 1 (y=8..15) -> İç Sıcaklık (ESP32-S3)
  drawNumber((int)(internalTemp + 0.5), 8);
  drawPixel(0, 15, true); // Sol üst nokta 1
  drawPixel(1, 15, true); // Sol üst nokta 2
  drawPixel(2, 15, true); // Sol üst nokta 3

  // Matris 2 (y=16..23) -> Nem (User'ın 3. matrisi - 2 Nokta Göstergesi)
  drawNumber((int)(hum + 0.5), 16);
  drawPixel(0, 23, true); // Sol üst nokta 1
  drawPixel(1, 23, true); // Sol üst nokta 2

  // Matris 3 (y=24..31) -> Sıcaklık (User'ın 4. matrisi - 1 Nokta Göstergesi)
  drawNumber((int)(temp + 0.5), 24);
  drawPixel(0, 31, true); // Sol üst nokta 1
}

void parseJson(char *packetBuffer) {
  JsonDocument doc;
  if (!deserializeJson(doc, packetBuffer)) {
    targetCPU = doc["cpu"] | 0.0f;
    targetRAM = doc["mem"] | 0.0f;
    targetGPU = doc["gpu"] | 0.0f;

    const char *timeStr = doc["time"];
    if (timeStr) {
      sscanf(timeStr, "%d:%d:%d", &hour, &minute, &second);
    }
  }
}

void sensorTask(void *pvParameters) {
  static bool lastTouch = false;
  unsigned long lastDiagLog = 0;

  while (true) {
    float t = sht.readTemperature();
    float h = sht.readHumidity();

    if (!isnan(t) && t < 100 && t > -50)
      temp = t;
    if (!isnan(h) && h <= 100 && h >= 0)
      hum = h;

    internalTemp = temperatureRead();

    if (millis() - lastDiagLog > 5000) {
      Serial.printf("[DIAG] Temp: %.2f C | Hum: %.2f%%\n", t, h);
      lastDiagLog = millis();
    }

    bool touch = digitalRead(TOUCH_PIN);
    if (touch && !lastTouch) {
      if (currentMode == AMBIENT)
        currentMode = PC_STATS;
      else if (currentMode == PC_STATS)
        currentMode = CLOCK;
      else
        currentMode = AMBIENT;

      Serial.printf("[MOD] %s\n", (currentMode == AMBIENT)    ? "Ambient"
                                  : (currentMode == PC_STATS) ? "PC Stats"
                                                              : "Clock");
      mx.clear();
    }
    lastTouch = touch;
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void displayTask(void *pvParameters) {
  while (true) {
    mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx.clear();

    if (currentMode == AMBIENT) {
      checkComfort();
    } else if (currentMode == CLOCK) {
      // CLOCK MODE (Face, Internal Temp, Minute, Hour)
      drawIcon(iconClock, 0); // Matris 0: Clock Icon

      drawNumber(second, 8); // Matris 1: Second
      drawPixel(0, 15, true);
      drawPixel(1, 15, true);
      drawPixel(2, 15, true);

      drawNumber(minute, 16); // Matris 2: Minute
      drawPixel(0, 23, true);
      drawPixel(1, 23, true);

      drawNumber(hour, 24); // Matris 3: Hour
      drawPixel(0, 31, true);
    } else {
      // PC STATS MODE (32x8 Dikey Çubuklar)
      currentCPU += (targetCPU - currentCPU) * 0.1;
      currentRAM += (targetRAM - currentRAM) * 0.1;
      currentGPU += (targetGPU - currentGPU) * 0.1;

      // Tüm matrisleri (y=0..31) kapsayan barlar
      updateBar(0, 1, currentCPU, 32);
      updateBar(3, 4, currentGPU, 32);
      updateBar(6, 7, currentRAM, 32);

      // Bağlantı Kontrolü
      bool connected =
          (WiFi.status() == WL_CONNECTED) && (millis() - lastPacketTime < 7000);
      if (!connected) {
        updateRain();
      }
    }

    mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
    vTaskDelay(pdMS_TO_TICKS(100)); // 10 FPS
  }
}

void loop() { vTaskDelete(NULL); }
