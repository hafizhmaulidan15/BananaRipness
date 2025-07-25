// --- LIBRARIES ---
#include <Wire.h>               // Mengaktifkan komunikasi I2C untuk sensor dan LCD.
#include <Adafruit_TCS34725.h>  // Library khusus sensor warna TCS34725.
#include <LiquidCrystal_PCF8574.h> // Library untuk LCD I2C.
#include <WiFi.h>               // Library eksplisit untuk manajemen WiFi.
#include "thingProperties.h" // Ini mendefinisikan 'bool button;' secara konkret

// Prototipe untuk fungsi-fungsi konversi dan pemrosesan data sensor.
float mapTo100(uint16_t rawVal, uint16_t minRaw, uint16_t maxRaw);

// Prototipe untuk fungsi-fungsi logika Fuzzy.
String getLinguistic(float val, float lowStart, float lowEnd, float medStart, float medEnd, float highStart, float highEnd);
String classifyFuzzy(float r, float g, float b);

// Prototipe untuk fungsi-fungsi bantu utama (misalnya untuk LCD).
void showStandby(LiquidCrystal_PCF8574& lcdObj);

// Prototipe untuk fungsi-fungsi manajemen input (tombol).
void checkButton();
// onButtonChange() diprototipekan oleh thingProperties.h

// Prototipe untuk fungsi pengukuran utama.
void measureColor();


// ===========================================================================================
// BAGIAN 1: GLOBAL OBJECTS & CORE VARIABLES
// ===========================================================================================

// Inisialisasi objek sensor warna TCS34725.
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_600MS, TCS34725_GAIN_16X);

// Inisialisasi objek LCD I2C dengan alamat standar 0x27.
LiquidCrystal_PCF8574 lcd(0x27);

// Pin GPIO yang digunakan untuk tombol dan kontrol LED sensor.
const int BTN_PIN = 4;
const int TCS_LED = 18;

// Variabel untuk menyimpan nilai mentah (raw data 16-bit) RGB dan Clear channel dari sensor.
uint16_t redRaw, greenRaw, blueRaw, clearRaw;

// Variabel status dan kontrol alur program.
bool isMeasured = false;
int lastBtnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50;
bool isMeasuring = false;
// Variabel untuk mengatur durasi tampilan hasil akhir di LCD.
unsigned long displayEndTime = 0;
const unsigned long DISPLAY_DURATION = 5000; // 5 detik

// Variabel untuk menyimpan rata-rata nilai RGB yang sudah dinormalisasi (0-100 desimal) dari MASING-MASING 3 titik pengukuran.
float r_p1, g_p1, b_p1;
float r_p2, g_p2, b_p2;
float r_p3, g_p3, b_p3;

int currentPoint = 0;

// Variabel IoT Cloud (sesuai yang dideklarasikan di thingProperties.h)
extern String klasifikasi;
extern float blue;
extern float green;
extern float red;
extern bool button;

// Variabel tambahan untuk manajemen koneksi WiFi
unsigned long lastWifiCheckTime = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000; // Cek status WiFi setiap 10 detik.

// ===========================================================================================
// BAGIAN 2: SETUP() & LOOP() - FUNGSI UTAMA ARDUINO
// ===========================================================================================

/**
 * Fungsi setup dijalankan sekali saat mikrokontroler menyala.
 * Melakukan inisialisasi hardware dan mencoba koneksi WiFi/IoT Cloud secara tangguh.
 */
void setup() {
  // --- Langkah 1: Inisialisasi Komunikasi Serial ---
  Serial.begin(9600);
  delay(1500);

  // --- Langkah 2: Inisialisasi Arduino IoT Cloud ---
  initProperties();

  // --- Langkah 3: Inisialisasi LCD dan Koneksi WiFi Awal ---
  lcd.begin(16, 2);
  lcd.setBacklight(255);
  Serial.println("Menghubungkan WiFi...");

  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  // --- Langkah 4: Konfigurasi Pin GPIO ---
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(TCS_LED, OUTPUT);
  digitalWrite(TCS_LED, LOW);

   // --- Langkah 5: Inisialisasi Sensor Warna ---
   if (tcs.begin()) {
     Serial.println("Sensor Siap");
   } else {
     Serial.println("Sensor ERROR");
     while (1);
   }

  // --- Langkah 6: Tampilkan Layar Siaga Awal ---
  showStandby(lcd);
}

/**
 * Fungsi loop dijalankan terus menerus setelah `setup()` selesai.
 * Ini adalah alur kendali utama yang berulang dan non-blokir.
 */
void loop() {
  ArduinoCloud.update();

  if (millis() - lastWifiCheckTime > WIFI_CHECK_INTERVAL) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi terputus, mencoba menyambung kembali...");
      WiFi.begin(SSID, PASS);
    }
    lastWifiCheckTime = millis();
  }

  checkButton();

  if (isMeasured && millis() - displayEndTime > DISPLAY_DURATION) {
    isMeasured = false;
    showStandby(lcd);
  }
}

// ===========================================================================================
// BAGIAN 3: FUNGSI UTAMA PENGUKURAN DAN KLASIFIKASI BUAH
// ===========================================================================================

/**
 * Mengelola alur pengukuran lengkap: 5x pembacaan per titik (16-bit raw),
 * menyimpan rata-rata per titik, lalu menghitung rata-rata final dari 3 titik.
 * Rata-rata final dikonversi dan diklasifikasikan dengan fuzzy logic.
 */
void measureColor() {
  isMeasuring = true;

  digitalWrite(TCS_LED, HIGH);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Mengukur Titik ");
  lcd.print(currentPoint + 1);
  lcd.print("...");
  delay(1500);

  float rSumMapped = 0, gSumMapped = 0, bSumMapped = 0;

  Serial.print("\n--- Pembacaan Titik ");
  Serial.print(currentPoint + 1);
  Serial.println(" ---");

  for (int i = 0; i < 5; i++) {
    tcs.getRawData(&redRaw, &greenRaw, &blueRaw, &clearRaw);

    float rMapped = mapTo100(redRaw, 20199, 65535);
    float gMapped = mapTo100(greenRaw, 22566, 65535);
    float bMapped = mapTo100(blueRaw, 9450, 62744);

    Serial.print("P"); Serial.print(i + 1);
    Serial.print(" R:"); Serial.print(rMapped, 2);
    Serial.print(" G:"); Serial.print(gMapped, 2);
    Serial.print(" B:"); Serial.println(bMapped, 2);

    rSumMapped += rMapped;
    gSumMapped += gMapped;
    bSumMapped += bMapped;

    delay(200);
  }
  digitalWrite(TCS_LED, LOW);

  float rAvgCurr = rSumMapped / 5.0;
  float gAvgCurr = gSumMapped / 5.0;
  float bAvgCurr = bSumMapped / 5.0;

  Serial.print("Rata-rata Titik "); Serial.print(currentPoint + 1);
  Serial.print(" adalah: R="); Serial.print(rAvgCurr, 2);
  Serial.print(", G="); Serial.print(gAvgCurr, 2);
  Serial.print(", B="); Serial.println(bAvgCurr, 2);
  Serial.println("--------------------");

  //r=read, p=point/titik
  if (currentPoint == 0) {
    r_p1 = rAvgCurr; g_p1 = gAvgCurr; b_p1 = bAvgCurr;
  } else if (currentPoint == 1) {
    r_p2 = rAvgCurr; g_p2 = gAvgCurr; b_p2 = bAvgCurr;
  } else if (currentPoint == 2) {
    r_p3 = rAvgCurr; g_p3 = gAvgCurr; b_p3 = bAvgCurr;
  }

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Titik "); lcd.print(currentPoint + 1); lcd.print(" Selesai");
  lcd.setCursor(0, 1); lcd.print("Tekan Lagi...");

  currentPoint++;

  if (currentPoint >= 3) {
    float final_r_avg = (r_p1 + r_p2 + r_p3) / 3.0;
    float final_g_avg = (g_p1 + g_p2 + g_p3) / 3.0;
    float final_b_avg = (b_p1 + b_p2 + b_p3) / 3.0;

    final_r_avg = round(final_r_avg * 100.0) / 100.0;
    final_g_avg = round(final_g_avg * 100.0) / 100.0;
    final_b_avg = round(final_b_avg * 100.0) / 100.0;

    String finalMaturity = classifyFuzzy(final_r_avg, final_g_avg, final_b_avg);

    red = final_r_avg;
    green = final_g_avg;
    blue = final_b_avg;
    klasifikasi = finalMaturity;
    button = false;

    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Kematangan Buah:");
    lcd.setCursor(0, 1); lcd.print(finalMaturity);
    isMeasured = true;
    displayEndTime = millis();

    currentPoint = 0;

    Serial.println("\n--- KLASIFIKASI PISANG AKHIR ---");
    Serial.print("R_final(0-100): "); Serial.print(final_r_avg, 2);
    Serial.print(" G_final(0-100): "); Serial.print(final_g_avg, 2);
    Serial.print(" B_final(0-100): "); Serial.print(final_b_avg, 2);
    Serial.print(" -> Kematangan: "); Serial.println(finalMaturity);
    Serial.println("-----------------------------------\n");
  }
  isMeasuring = false;
}

// ===========================================================================================
// BAGIAN 4: MANAJEMEN INPUT (BUTTON)
// Fungsi ini bertanggung jawab untuk mendeteksi penekanan tombol fisik atau dari IoT Cloud
// dan memicu tindakan yang sesuai (mulai pengukuran atau reset tampilan).
//===========================================================================================

/**
 * Memeriksa status tombol fisik dengan debounce dan memicu pengukuran atau mengatur ulang tampilan.
 */
void checkButton() {
  int reading = digitalRead(BTN_PIN);

  if (reading != lastBtnState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && !isMeasuring) {
      if (isMeasured) {
        isMeasured = false;
        currentPoint = 0;
        showStandby(lcd);
      } else {
        measureColor();
      }
    }
  }
  lastBtnState = reading;
}

/**
 * Fungsi callback yang dipicu oleh perubahan pada variabel 'button' dari dasbor IoT Cloud.
 * Memiliki fungsi yang sama dengan tombol fisik.
 */
void onButtonChange() {
  if (button && !isMeasuring) {
    if (isMeasured) {
        isMeasured = false;
        currentPoint = 0;
        showStandby(lcd);
    } else {
      measureColor();
    }
    button = false;
  }
}

// ===========================================================================================
// BAGIAN 5: IMPLEMENTASI FUNGSI BANTU UTAMA
// ===========================================================================================

/**
 * Mengonversi nilai 16-bit mentah sensor ke rentang 0-100 desimal.
 */
float mapTo100(uint16_t rawVal, uint16_t minRaw, uint16_t maxRaw) {
  if (minRaw == maxRaw) {
    return constrain(map((long)rawVal, 0, 65535, 0, 10000) / 100.0, 0.0, 100.0);
  }
  float mapped_val = map((long)rawVal, (long)minRaw, (long)maxRaw, 0, 10000) / 100.0;
  return constrain(mapped_val, 0.0, 100.0);
}

/**
 * Menampilkan layar instruksi "Tekan Tombol Utk Ukur T.1" di LCD.
 */
void showStandby(LiquidCrystal_PCF8574& lcdObj) {
  lcdObj.clear();
  lcdObj.setCursor(2, 0); lcdObj.print("Tekan Tombol");
  lcdObj.setCursor(1, 1); lcdObj.print("Utk Ukur T.1");
}

// ===========================================================================================
// BAGIAN 6: IMPLEMENTASI FUNGSI MODEL FUZZY LOGIC
// ===========================================================================================

/**
 * Menentukan kategori linguistik (LOW, MEDIUM, HIGH) berdasarkan rentang nilai.
 */
String getLinguistic(float val, float lowStart, float lowEnd, 
float medStart, float medEnd, float highStart, float highEnd) {
  if (val >= lowStart && val <= lowEnd) return "LOW";
  if (val >= medStart && val <= medEnd) return "MEDIUM";
  if (val >= highStart && val <= highEnd) return "HIGH";
  return "Tidak Valid";
}


/**
 * Melakukan inferensi Fuzzy Mamdani untuk klasifikasi kematangan pisang.
 */
String classifyFuzzy(float r, float g, float b) {
  // Range terbaru dari kamu
  String r_ling = getLinguistic(r, 30.82, 55.03, 52.73, 78.09, 75.79, 100.00);
  String g_ling = getLinguistic(g, 34.43, 57.38, 55.20, 79.24, 77.06, 100.00);
  String b_ling = getLinguistic(b, 14.42, 42.89, 40.17, 70.00, 67.28, 95.74);

  // 9 aturan
  if (r_ling == "HIGH" && g_ling == "HIGH" && b_ling == "HIGH") return "Matang";     // HHH
  if (r_ling == "HIGH" && g_ling == "HIGH" && b_ling == "LOW") return "Mentah";      // HHL
  if (r_ling == "HIGH" && g_ling == "HIGH" && b_ling == "MEDIUM") return "Matang";   // HHM
  if (r_ling == "LOW"  && g_ling == "HIGH" && b_ling == "LOW") return "Mentah";      // LHL
  if (r_ling == "LOW"  && g_ling == "LOW"  && b_ling == "LOW") return "Mengkal";     // LLL
  if (r_ling == "LOW"  && g_ling == "MEDIUM" && b_ling == "LOW") return "Mengkal";   // LML
  if (r_ling == "MEDIUM" && g_ling == "HIGH" && b_ling == "LOW") return "Mentah";    // MHL
  if (r_ling == "MEDIUM" && g_ling == "HIGH" && b_ling == "MEDIUM") return "Mentah"; // MHM
  if (r_ling == "MEDIUM" && g_ling == "MEDIUM" && b_ling == "LOW") return "Mengkal"; // MML
  
  return "Tidak Terdeteksi";
}


// ===========================================================================================
// BAGIAN 7: IMPLEMENTASI FUNGSI CALLBACK IoT CLOUD (KOSONG)
// ===========================================================================================
void onKlasifikasiChange() { }
void onBlueChange() { }
void onGreenChange() { }
void onRedChange() { }
