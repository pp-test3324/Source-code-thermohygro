#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>
#include <Preferences.h>

struct CalibPoint {
  float raw;
  float std;
  float correction; // Y = std - raw
};

const int MAX_POINTS = 4;

// Tabel Kalibrasi Suhu & Kelembapan
CalibPoint calibTempTable[MAX_POINTS];
CalibPoint calibHumTable[MAX_POINTS];
int numTempPoints = 0;
int numHumPoints = 0;

Preferences preferences;

// Muat data Suhu dan Kelembapan dari NVS
void loadCalibrationTable() {
  preferences.begin("calib_data", true);
  
  numTempPoints = preferences.getInt("t_count", 0);
  if (numTempPoints > 0) {
    for (int i = 0; i < numTempPoints; i++) {
      String keyR = "tr_" + String(i);
      String keyS = "ts_" + String(i);
      calibTempTable[i].raw = preferences.getFloat(keyR.c_str(), 0.0);
      calibTempTable[i].std = preferences.getFloat(keyS.c_str(), 0.0);
      calibTempTable[i].correction = calibTempTable[i].std - calibTempTable[i].raw;
    }
  }

  numHumPoints = preferences.getInt("h_count", 0);
  if (numHumPoints > 0) {
    for (int i = 0; i < numHumPoints; i++) {
      String keyR = "hr_" + String(i);
      String keyS = "hs_" + String(i);
      calibHumTable[i].raw = preferences.getFloat(keyR.c_str(), 0.0);
      calibHumTable[i].std = preferences.getFloat(keyS.c_str(), 0.0);
      calibHumTable[i].correction = calibHumTable[i].std - calibHumTable[i].raw;
    }
  }

  Serial.printf("[NVS] %d Titik Suhu & %d Titik RH dimuat!\n", numTempPoints, numHumPoints);
  preferences.end();
}

// Simpan Titik Suhu ke NVS
void saveTempPoint(int index, float rawVal, float stdVal) {
  preferences.begin("calib_data", false);
  String keyR = "tr_" + String(index);
  String keyS = "ts_" + String(index);
  preferences.putFloat(keyR.c_str(), rawVal);
  preferences.putFloat(keyS.c_str(), stdVal);

  calibTempTable[index].raw = rawVal;
  calibTempTable[index].std = stdVal;
  calibTempTable[index].correction = stdVal - rawVal;

  if (index + 1 > numTempPoints) {
    numTempPoints = index + 1;
    preferences.putInt("t_count", numTempPoints);
  }
  preferences.end();
}

// Simpan Titik Kelembapan ke NVS
void saveHumPoint(int index, float rawVal, float stdVal) {
  preferences.begin("calib_data", false);
  String keyR = "hr_" + String(index);
  String keyS = "hs_" + String(index);
  preferences.putFloat(keyR.c_str(), rawVal);
  preferences.putFloat(keyS.c_str(), stdVal);

  calibHumTable[index].raw = rawVal;
  calibHumTable[index].std = stdVal;
  calibHumTable[index].correction = stdVal - rawVal;

  if (index + 1 > numHumPoints) {
    numHumPoints = index + 1;
    preferences.putInt("h_count", numHumPoints);
  }
  preferences.end();
}

// Fungsi Interpolasi Linear Suhu
float getInterpolatedTempCorrection(float rawVal) {
  if (numTempPoints == 0) return 0.0;
  if (rawVal <= calibTempTable[0].raw) return calibTempTable[0].correction;
  if (rawVal >= calibTempTable[numTempPoints - 1].raw) return calibTempTable[numTempPoints - 1].correction;

  for (int i = 0; i < numTempPoints - 1; i++) {
    if (rawVal >= calibTempTable[i].raw && rawVal <= calibTempTable[i + 1].raw) {
      float x1 = calibTempTable[i].raw;
      float x2 = calibTempTable[i + 1].raw;
      float y1 = calibTempTable[i].correction;
      float y2 = calibTempTable[i + 1].correction;
      return y1 + ((rawVal - x1) / (x2 - x1)) * (y2 - y1);
    }
  }
  return 0.0;
}

// Fungsi Interpolasi Linear Kelembapan
float getInterpolatedHumCorrection(float rawVal) {
  if (numHumPoints == 0) return 0.0;
  if (rawVal <= calibHumTable[0].raw) return calibHumTable[0].correction;
  if (rawVal >= calibHumTable[numHumPoints - 1].raw) return calibHumTable[numHumPoints - 1].correction;

  for (int i = 0; i < numHumPoints - 1; i++) {
    if (rawVal >= calibHumTable[i].raw && rawVal <= calibHumTable[i + 1].raw) {
      float x1 = calibHumTable[i].raw;
      float x2 = calibHumTable[i + 1].raw;
      float y1 = calibHumTable[i].correction;
      float y2 = calibHumTable[i + 1].correction;
      return y1 + ((rawVal - x1) / (x2 - x1)) * (y2 - y1);
    }
  }
  return 0.0;
}

#endif // CALIBRATION_H
