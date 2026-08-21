#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <esp_sntp.h>
#include <WiFiManager.h>
#include "config.h"
#include "calibration.h"

extern QueueHandle_t telemetryQueue;
extern volatile bool isNetworkOnline;
extern volatile bool isPortalActive;
extern volatile bool isCalibrationMode;
extern volatile bool toggleModeRequest;

WiFiClientSecure espClient;
PubSubClient client(espClient);
WiFiManager wm;

volatile bool shouldExitPortal = false;

// 1. Form Bagian Suhu
WiFiManagerParameter header_temp("<h3>1. Kalibrasi Suhu (°C) - GFTB 200</h3><hr>");
WiFiManagerParameter pt_raw_1("tr1", "Titik 1 - RAW Suhu (~20C)", "20.0", 6);
WiFiManagerParameter pt_std_1("ts1", "Titik 1 - Acuan Suhu", "20.0", 6);
WiFiManagerParameter pt_raw_2("tr2", "Titik 2 - RAW Suhu (~25C)", "25.0", 6);
WiFiManagerParameter pt_std_2("ts2", "Titik 2 - Acuan Suhu", "25.0", 6);
WiFiManagerParameter pt_raw_3("tr3", "Titik 3 - RAW Suhu (~30C)", "30.0", 6);
WiFiManagerParameter pt_std_3("ts3", "Titik 3 - Acuan Suhu", "30.0", 6);
WiFiManagerParameter pt_raw_4("tr4", "Titik 4 - RAW Suhu (~35C)", "35.0", 6);
WiFiManagerParameter pt_std_4("ts4", "Titik 4 - Acuan Suhu", "35.0", 6);

// 2. Form Bagian Kelembapan
WiFiManagerParameter header_hum("<br/><h3>2. Kalibrasi Kelembapan (%RH) - GFTB 200</h3><hr>");
WiFiManagerParameter ph_raw_1("hr1", "Titik 1 - RAW RH (~30%)", "30.0", 6);
WiFiManagerParameter ph_std_1("hs1", "Titik 1 - Acuan RH", "30.0", 6);
WiFiManagerParameter ph_raw_2("hr2", "Titik 2 - RAW RH (~45%)", "45.0", 6);
WiFiManagerParameter ph_std_2("hs2", "Titik 2 - Acuan RH", "45.0", 6);
WiFiManagerParameter ph_raw_3("hr3", "Titik 3 - RAW RH (~60%)", "60.0", 6);
WiFiManagerParameter ph_std_3("hs3", "Titik 3 - Acuan RH", "60.0", 6);
WiFiManagerParameter ph_raw_4("hr4", "Titik 4 - RAW RH (~75%)", "75.0", 6);
WiFiManagerParameter ph_std_4("hs4", "Titik 4 - Acuan RH", "75.0", 6);

// Tombol Navigasi Khusus Halaman Kalibrasi (/param)
WiFiManagerParameter p_btn_nav(
  "<br/>"
  "<button type='button' class='button' style='background-color:#6c757d; margin-top:10px; width:100%; display:block;' onclick=\"window.location.href='/'\">Kembali ke Menu Utama</button>"
  "<br/>"
  "<button type='button' class='button' style='background-color:#dc3545; margin-top:10px; width:100%; display:block;' onclick=\"window.location.href='/exit'\">Batal & Kembali ke Mode RTOS</button>"
);

// Script tombol kembali seragam di semua container
const char* auto_back_button_script = 
  "<script>"
  "window.addEventListener('DOMContentLoaded', function() {"
  "  var path = window.location.pathname;"
  "  if (path === '/wifi' || path === '/info' || path === '/wifi?') {"
  "    var container = document.querySelector('div') || document.querySelector('form') || document.body;"
  "    var btn = document.createElement('button');"
  "    btn.type = 'button';"
  "    btn.className = 'button';"
  "    btn.style = 'background-color:#6c757d; margin-top:15px; width:100%; display:block;';"
  "    btn.innerText = 'Kembali ke Menu Utama';"
  "    btn.onclick = function() { window.location.href = '/'; };"
  "    container.appendChild(btn);"
  "  }"
  "});"
  "</script>";

static bool parametersAdded = false;

// Callback saat tombol Save ditekan
void saveParamsCallback() {
  // Simpan 4 Titik Suhu
  saveTempPoint(0, atof(pt_raw_1.getValue()), atof(pt_std_1.getValue()));
  saveTempPoint(1, atof(pt_raw_2.getValue()), atof(pt_std_2.getValue()));
  saveTempPoint(2, atof(pt_raw_3.getValue()), atof(pt_std_3.getValue()));
  saveTempPoint(3, atof(pt_raw_4.getValue()), atof(pt_std_4.getValue()));

  // Simpan 4 Titik Kelembapan
  saveHumPoint(0, atof(ph_raw_1.getValue()), atof(ph_std_1.getValue()));
  saveHumPoint(1, atof(ph_raw_2.getValue()), atof(ph_std_2.getValue()));
  saveHumPoint(2, atof(ph_raw_3.getValue()), atof(ph_std_3.getValue()));
  saveHumPoint(3, atof(ph_raw_4.getValue()), atof(ph_std_4.getValue()));
  
  Serial.println(F("[Portal] Seluruh Data Suhu & RH Tersimpan ke NVS!"));
  shouldExitPortal = true;
}

void handleExitRoute() {
  wm.server->send(200, "text/html", "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;text-align:center;padding:30px;}</style></head><body><h2>Keluar dari Mode Kalibrasi</h2><p>ESP32 sedang kembali ke Mode Kerja RTOS...</p></body></html>");
  shouldExitPortal = true;
}

void bindServerCallback() {
  wm.server->on("/exit", handleExitRoute);
}

bool checkInternetAccess() {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClient testClient;
  testClient.setTimeout(2);
  if (testClient.connect("8.8.8.8", 53)) {
    testClient.stop();
    return true;
  }
  return false;
}

void startPortalAsync() {
  isPortalActive = true;
  isNetworkOnline = false;
  isCalibrationMode = true;
  shouldExitPortal = false;

  if (!parametersAdded) {
    // Form Suhu
    wm.addParameter(&header_temp);
    wm.addParameter(&pt_raw_1);
    wm.addParameter(&pt_std_1);
    wm.addParameter(&pt_raw_2);
    wm.addParameter(&pt_std_2);
    wm.addParameter(&pt_raw_3);
    wm.addParameter(&pt_std_3);
    wm.addParameter(&pt_raw_4);
    wm.addParameter(&pt_std_4);

    // Form Kelembapan
    wm.addParameter(&header_hum);
    wm.addParameter(&ph_raw_1);
    wm.addParameter(&ph_std_1);
    wm.addParameter(&ph_raw_2);
    wm.addParameter(&ph_std_2);
    wm.addParameter(&ph_raw_3);
    wm.addParameter(&ph_std_3);
    wm.addParameter(&ph_raw_4);
    wm.addParameter(&ph_std_4);

    wm.addParameter(&p_btn_nav);
    
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setWebServerCallback(bindServerCallback);
    wm.setCustomHeadElement(auto_back_button_script);

    std::vector<const char *> menu = {"param", "wifi", "info", "sep", "exit"};
    wm.setMenu(menu);
    wm.setCustomMenuHTML("<form action='/param' method='get'><button class='button'>Menu Kalibrasi (Suhu & RH)</button></form><br/>");

    parametersAdded = true;
  }

  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(300);
  
  String apName = "CALIB-ESP32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  wm.startConfigPortal(apName.c_str());
  
  Serial.println(F("[Portal] Mode Kalibrasi Aktif. Akses via 192.168.4.1"));
}

void stopPortalAsync() {
  shouldExitPortal = true;
}

void reconnectMqtt() {
  if (client.connected() || WiFi.status() != WL_CONNECTED) return;

  String clientId = "ESP32_" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
    Serial.println(F("[Core 0] MQTT TLS Berhasil Terhubung!"));
  }
}

// ==========================================
// TASK 1: JARINGAN & MQTT (CORE 0)
// ==========================================
void TaskNetwork(void *pvParameters) {
  Serial.println(F("[Core 0] Task Jaringan Aktif"));

  WiFi.mode(WIFI_STA);
  WiFi.begin();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);

  unsigned long lastPingTime = 0;
  unsigned long disconnectedStart = 0;
  unsigned long noInternetStart = 0;
  unsigned long lastWifiRetry = 0;

  for (;;) {
    unsigned long now = millis();

    if (toggleModeRequest) {
      toggleModeRequest = false;
      if (!isCalibrationMode) {
        startPortalAsync();
      } else {
        stopPortalAsync();
      }
    }

    if (shouldExitPortal) {
      shouldExitPortal = false;
      vTaskDelay(pdMS_TO_TICKS(250));

      Serial.println(F("[Portal] Menutup Portal -> Kembali ke Mode Kerja RTOS..."));
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      WiFi.begin();

      isCalibrationMode = false;
      isPortalActive = false;
    }

    if (isPortalActive) {
      wm.process();
    } else {
      if (WiFi.status() != WL_CONNECTED) {
        isNetworkOnline = false;

        if (disconnectedStart == 0) disconnectedStart = now;

        if (now - lastWifiRetry >= 10000) {
          lastWifiRetry = now;
          WiFi.reconnect();
        }

        if (now - disconnectedStart >= 60000) {
          startPortalAsync();
          disconnectedStart = millis();
        }
      } else {
        disconnectedStart = 0;

        if (now - lastPingTime >= 10000) {
          lastPingTime = now;
          if (checkInternetAccess()) {
            noInternetStart = 0;
            isNetworkOnline = true;
          } else {
            if (noInternetStart == 0) noInternetStart = now;
            if (now - noInternetStart >= 300000) isNetworkOnline = false;
          }
        }

        if (isNetworkOnline) {
          if (!client.connected()) reconnectMqtt();
          client.loop();
        }
      }

      if (isNetworkOnline && client.connected()) {
        TelemetryData item;
        while (xQueueReceive(telemetryQueue, &item, 0) == pdTRUE) {
          String payload = "{\"temperature\": " + String(item.temperature, 2) + 
                           ", \"humidity\": " + String(item.humidity, 1) + "}";

          client.publish(mqtt_topic, payload.c_str());
          vTaskDelay(pdMS_TO_TICKS(150));
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

#endif // NETWORK_MANAGER_H
