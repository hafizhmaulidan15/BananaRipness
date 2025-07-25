// --- LIBRARIES ---
#include <Wire.h>
#include <Adafruit_TCS34725.h>
#include <LiquidCrystal_PCF8574.h>
#include <WiFi.h>
#include "thingProperties.h"

void showStandby(LiquidCrystal_PCF8574& lcdObj);
void checkButton();
void measureColor();

// ===========================================================================================
// BAGIAN 1: GLOBAL OBJECTS & CORE VARIABLES
// ===========================================================================================

// Inisialisasi sensor dengan GAIN TETAP 16X dan objek LCD I2C.
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_600MS, TCS34725_GAIN_16X);
LiquidCrystal_PCF8574 lcd(0x27);

// Pin GPIO.
const int BTN_PIN = 4;
const int TCS_LED = 18;

// Variabel untuk menyimpan nilai mentah (raw data 16-bit).
uint16_t redRaw, greenRaw, blueRaw, clearRaw;

// Variabel status dan kontrol alur.
bool isMeasured = false;
int lastBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;
bool isMeasuring = false;
unsigned long displayEndTime = 0;
const unsigned long DISPLAY_DURATION = 5000;

// Variabel IoT Cloud.
extern int blue;
extern int green;
extern int red;
extern int clear;
extern bool button;

// Variabel manajemen WiFi.
unsigned long lastWifiCheckTime = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000;

// ===========================================================================================
// BAGIAN 2: SETUP() & LOOP()
// ===========================================================================================

void setup() {
  Serial.begin(9600);
  delay(1500);

  initProperties();

  lcd.begin(16, 2);
  lcd.setBacklight(255);
  Serial.println("Menghubungkan WiFi...");

  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(TCS_LED, OUTPUT);
  digitalWrite(TCS_LED, LOW); // Pastikan LED mati saat awal

  Serial.print("Mencoba menemukan sensor TCS34725... ");
  if (tcs.begin()) {
    Serial.println("Sensor TCS34725 Siap");
  } else {
    Serial.println("Sensor TCS34725 ERROR! Periksa kabel Anda.");
    while (1); // Berhenti jika sensor tidak ditemukan
  }

  showStandby(lcd);
}

void loop() {
  ArduinoCloud.update();

  if (millis() - lastWifiCheckTime > WIFI_CHECK_INTERVAL) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi terputus, mencoba menyambung kembali...");
    }
    lastWifiCheckTime = millis();
  }

  checkButton();

  if (!isMeasuring && isMeasured && millis() - displayEndTime > DISPLAY_DURATION) {
    isMeasured = false;
    showStandby(lcd);
  }
}

// ===========================================================================================
// BAGIAN 3: FUNGSI UTAMA PENGUKURAN
// ===========================================================================================

/**
 * Melakukan 5 kali pembacaan data mentah.
 * LED SENSOR HANYA MENYALA SESAAT KETIKA DATA DIAMBIL.
 */
void measureColor() {
  isMeasuring = true;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Membaca Sensor...");
  lcd.setCursor(0, 1);
  lcd.print("5x Pembacaan...");
  delay(1500);

  Serial.println("\n--- Memulai 5x Pembacaan Mentah ---");
  
  for (int i = 0; i < 5; i++) {

    // LED dinyalakan tepat sebelum membaca
    digitalWrite(TCS_LED, HIGH);
    delay(60); // Jeda singkat agar LED stabil

    tcs.getRawData(&redRaw, &greenRaw, &blueRaw, &clearRaw); // Baca data

    // LED dimatikan tepat setelah membaca
    digitalWrite(TCS_LED, LOW);

    // Tampilkan hasil di Serial Monitor
    Serial.print("Pembacaan "); Serial.print(i + 1); Serial.print(": ");
    Serial.print("R:"); Serial.print(redRaw);
    Serial.print(" G:"); Serial.print(greenRaw);
    Serial.print(" B:"); Serial.print(blueRaw);
    Serial.print(" C:"); Serial.println(clearRaw);

    // Tampilkan hasil di LCD
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("P:"); lcd.print(i + 1);
    lcd.setCursor(4, 0); lcd.print("R:"); lcd.print(redRaw);
    lcd.setCursor(0, 1); lcd.print("G:"); lcd.print(greenRaw);
    lcd.setCursor(8, 1); lcd.print("B:"); lcd.print(blueRaw);

    delay(1000); // Waktu untuk user melihat hasil di LCD (saat LED mati)
    delay(200);
  }

  button = false;

  Serial.println("\n--- Pembacaan 5x Selesai ---");

  isMeasured = true;
  displayEndTime = millis();
  isMeasuring = false;
}

// ===========================================================================================
// BAGIAN 4: MANAJEMEN INPUT (BUTTON)
// ===========================================================================================

void checkButton() {
  int reading = digitalRead(BTN_PIN);
  if (reading != lastBtnState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && !isMeasuring) {
      if (isMeasured) {
        isMeasured = false;
        showStandby(lcd);
      } else {
        measureColor();
      }
    }
  }
  lastBtnState = reading;
}

void onButtonChange() {
  if (button && !isMeasuring) {
    if (isMeasured) {
      isMeasured = false;
      showStandby(lcd);
    } else {
      measureColor();
    }
    button = false;
  }
}

// ===========================================================================================
// BAGIAN 5: FUNGSI BANTU
// ===========================================================================================

void showStandby(LiquidCrystal_PCF8574& lcdObj) {
  lcdObj.clear();
  lcdObj.setCursor(2, 0); lcdObj.print("Tekan Tombol");
  lcdObj.setCursor(1, 1); lcdObj.print("Utk Ukur");
}

// ===========================================================================================
// BAGIAN 6: FUNGSI CALLBACK IoT CLOUD
// ===========================================================================================
void onBlueChange() { }
void onGreenChange() { }
void onRedChange() { }
void onClearChange() { }
