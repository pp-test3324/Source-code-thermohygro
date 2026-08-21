#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#include "config.h"
#include "calibration.h"
#include "network_manager.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHTPIN, DHTTYPE);

QueueHandle_t telemetryQueue;
const int QUEUE_CAPACITY = 120;

volatile bool isNetworkOnline = false;
volatile bool isPortalActive = false;
volatile bool isCalibrationMode = false;
volatile bool toggleModeRequest = false;

volatile float latestTemp = 0.0;
volatile float latestHum = 0.0;

// ==========================================
// TASK 2: SENSOR DHT22 & OLED (CORE 1)
// ==========================================
void TaskSensorDisplay(void *pvParameters) {
  Serial.println(F("[Core 1] Task Sensor & Display Aktif"));

  unsigned long lastDisplayUpdate = 0;
  unsigned long lastDataPush = 0;
  bool lastButtonState = HIGH;
  unsigned long lastDebounceTime = 0;

  for (;;) {
    unsigned long now = millis();

    // 1. Deteksi Toggle GPIO 14
    bool reading = digitalRead(CALIB_BUTTON_PIN);
    if (reading == LOW && lastButtonState == HIGH && (now - lastDebounceTime > 400)) {
      lastDebounceTime = now;
      toggleModeRequest = true;
      Serial.println(F("[GPIO 14] Tombol Disentuh -> Mengubah Mode..."));
    }
    lastButtonState = reading;

    // 2. Baca Sensor & Update OLED
    if (now - lastDisplayUpdate >= 2000 || lastDisplayUpdate == 0) {
      lastDisplayUpdate = now;

      float rawH = dht.readHumidity();
      float rawT = dht.readTemperature();

      if (!isnan(rawH) && !isnan(rawT)) {
        if (isCalibrationMode) {
          // MODE KALIBRASI: Tampilkan Nilai Mentah (RAW)
          latestTemp = rawT;
          latestHum = rawH;
        } else {
          // MODE KERJA RTOS: Terapkan Interpolasi Suhu & RH
          float Y_Temp = getInterpolatedTempCorrection(rawT);
          float Y_Hum  = getInterpolatedHumCorrection(rawH);
          
          latestTemp = rawT + Y_Temp;
          latestHum  = rawH + Y_Hum;
        }
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);

      // Header Status
      display.setCursor(0, 0);
      if (isCalibrationMode || isPortalActive) {
        display.print(F("[MODE KALIBRASI: RAW]"));
      } else if (!isNetworkOnline) {
        display.print(F("[OFFLINE - BUFFER]"));
      } else {
        display.print(F("EMQX THERMOHYGRO"));
      }
      display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

      // Suhu
      display.setCursor(0, 16);
      display.print(F("T:"));
      display.setTextSize(2);
      display.setCursor(24, 13);
      display.print(latestTemp, (isCalibrationMode ? 1 : 2));
      display.setTextSize(1);
      display.cp437(true);
      display.write(167);
      display.print(F("C"));

      // Kelembapan
      display.setCursor(0, 35);
      display.print(F("H:"));
      display.setTextSize(2);
      display.setCursor(24, 32);
      display.print(latestHum, 1);
      display.setTextSize(1);
      display.print(F(" %"));

      // Keterangan IP / Queue
      display.setTextSize(1);
      if (isCalibrationMode || isPortalActive) {
        display.setCursor(0, 54);
        display.print(F("AP: 192.168.4.1"));
      } else {
        UBaseType_t queuedCount = uxQueueMessagesWaiting(telemetryQueue);
        if (queuedCount > 0) {
          display.setCursor(85, 54);
          display.print(F("Q:"));
          display.print(queuedCount);
        }
      }

      display.display();
    }

    // 3. Masukkan ke Queue Telemetri Tiap 5 Menit
    if (!isCalibrationMode && !isPortalActive && (now - lastDataPush >= 300000 || lastDataPush == 0)) {
      lastDataPush = now;

      if (!isnan(latestTemp) && !isnan(latestHum) && (latestTemp != 0.0 || latestHum != 0.0)) {
        TelemetryData dataPoint = { latestTemp, latestHum, time(nullptr) };

        if (xQueueSend(telemetryQueue, &dataPoint, 0) != pdPASS) {
          TelemetryData dummy;
          xQueueReceive(telemetryQueue, &dummy, 0);
          xQueueSend(telemetryQueue, &dataPoint, 0);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(CALIB_BUTTON_PIN, INPUT_PULLUP);

  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  loadCalibrationTable();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 25);
  display.println(F("Starting RTOS System..."));
  display.display();
  delay(500);

  telemetryQueue = xQueueCreate(QUEUE_CAPACITY, sizeof(TelemetryData));

  xTaskCreatePinnedToCore(TaskNetwork, "NetTask", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskSensorDisplay, "SensTask", 4096, NULL, 2, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);
}
