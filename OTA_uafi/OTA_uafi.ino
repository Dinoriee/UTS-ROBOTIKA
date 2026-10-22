
/*********************************************************
 * iMCLab + ESP32 + Blynk.Edgent + Blynk Air
 * Monitoring RPM, Kontrol PWM, dan Motor ON/OFF
 *
 * V0 = RPM
 * V1 = Nilai PWM
 * V2 = Slider PWM (0-255)
 * V3 = Switch Motor ON/OFF
 *********************************************************/

// Identitas Blynk Template
#define BLYNK_TEMPLATE_ID   "TMPL6Zh3pGdg3"
#define BLYNK_TEMPLATE_NAME "Tes"

// Naikkan setiap membuat firmware baru.
// Gunakan versi yang lebih tinggi dari firmware terakhir.
#define BLYNK_FIRMWARE_VERSION "0.1.2"

#define BLYNK_PRINT Serial
#define APP_DEBUG

#include <esp_arduino_version.h>
#include "BlynkEdgent.h"

// ============================================================
// PIN MOTOR DAN LED
// ============================================================

const int ledPin = 2;

const int motor1Pin1 = 27;
const int motor1Pin2 = 26;
const int enable1Pin = 12;

// ============================================================
// SENSOR RPM
// ============================================================

const byte pin_rpm = 13;

// Jumlah pulsa sensor untuk setiap satu putaran
const int HOLES = 2;

volatile unsigned long rev = 0;
volatile unsigned long lastPulseTime = 0;

const unsigned long DEBOUNCE_US = 500;

float rpm = 0.0;
unsigned long old_time = 0;

// ============================================================
// KONFIGURASI PWM
// ============================================================

const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;

int dutyCycle = 200;

const int DUTY_MIN = 0;
const int DUTY_MAX = 255;
const int DUTY_STEP = 10;

bool isMotorOn = false;
bool motorStateChanged = true;

// ============================================================
// SERIAL DAN TIMER
// ============================================================

String inputString = "";

BlynkTimer timer;

const unsigned long SEND_INTERVAL_MS = 1000;

// ============================================================
// PWM - KOMPATIBEL DENGAN ESP32 CORE 2.x DAN 3.x
// ============================================================

void setupMotorPWM()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(enable1Pin, freq, resolution);
#else
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(enable1Pin, pwmChannel);
#endif

  writeMotorPWM(0);
}

void writeMotorPWM(int duty)
{
  duty = constrain(duty, DUTY_MIN, DUTY_MAX);

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(enable1Pin, duty);
#else
  ledcWrite(pwmChannel, duty);
#endif
}

// Nilai PWM aktual yang diberikan ke driver motor
int getActualPWM()
{
  if (isMotorOn && dutyCycle > 0)
    return dutyCycle;

  return 0;
}

// ============================================================
// INTERRUPT SENSOR RPM
// ============================================================

void IRAM_ATTR isr()
{
  unsigned long now = micros();

  if (now - lastPulseTime > DEBOUNCE_US)
  {
    rev++;
    lastPulseTime = now;
  }
}

// ============================================================
// HITUNG RPM
// ============================================================

void hitungRPM()
{
  unsigned long cur_time = millis();
  unsigned long elapsed = cur_time - old_time;

  if (elapsed >= 1000)
  {
    // Mengambil jumlah pulsa dengan menghentikan interrupt
    // sesaat untuk menghindari konflik saat membaca/reset.
    detachInterrupt(digitalPinToInterrupt(pin_rpm));

    unsigned long pulseCount = rev;
    rev = 0;

    attachInterrupt(
      digitalPinToInterrupt(pin_rpm),
      isr,
      FALLING
    );

    // RPM = pulsa / pulsa per putaran / waktu (menit)
    rpm = ((float)pulseCount * 60000.0f) /
          ((float)HOLES * (float)elapsed);

    old_time = cur_time;
  }
}

// ============================================================
// TERAPKAN PWM DAN ARAH MOTOR
// ============================================================

void applyMotor()
{
  int actualPWM = getActualPWM();

  if (actualPWM > 0)
  {
    // Arah motor seperti konfigurasi awal
    digitalWrite(motor1Pin1, HIGH);
    digitalWrite(motor1Pin2, LOW);
  }
  else
  {
    // Motor OFF
    digitalWrite(motor1Pin1, LOW);
    digitalWrite(motor1Pin2, LOW);
  }

  writeMotorPWM(actualPWM);

  // LED hanya menyala ketika PWM aktual > 0
  digitalWrite(ledPin, actualPWM > 0 ? HIGH : LOW);

  // Sinkronisasi kontrol jika Blynk terhubung
  if (Blynk.connected())
  {
    Blynk.virtualWrite(V2, dutyCycle);
    Blynk.virtualWrite(V3, isMotorOn ? 1 : 0);
  }
}

// ============================================================
// PERBARUI HARDWARE JIKA ADA PERUBAHAN
// ============================================================

void updateDevice()
{
  if (motorStateChanged)
  {
    applyMotor();
    motorStateChanged = false;
  }
}

// ============================================================
// KONEKSI BLYNK
// ============================================================

BLYNK_CONNECTED()
{
  Serial.println("Blynk terhubung.");

  // Keamanan: motor tidak otomatis hidup saat reconnect
  isMotorOn = false;
  motorStateChanged = true;

  // Atur switch dashboard ke OFF
  Blynk.virtualWrite(V3, 0);

  // Ambil nilai slider PWM tersimpan dari Blynk
  Blynk.syncVirtual(V2);
}

// ============================================================
// V2: SLIDER PWM (0-255)
// ============================================================

BLYNK_WRITE(V2)
{
  dutyCycle = constrain(
    param.asInt(),
    DUTY_MIN,
    DUTY_MAX
  );

  // Jika PWM 0, matikan motor
  if (dutyCycle == 0)
  {
    isMotorOn = false;
  }

  motorStateChanged = true;

  Serial.print("PWM dari Blynk: ");
  Serial.println(dutyCycle);
}

// ============================================================
// V3: SWITCH MOTOR ON/OFF
// ============================================================

BLYNK_WRITE(V3)
{
  bool requestedState = (param.asInt() != 0);

  if (requestedState)
  {
    if (dutyCycle == 0)
    {
      dutyCycle = 200;
    }

    isMotorOn = true;
  }
  else
  {
    // Motor mati, nilai slider tetap tersimpan
    isMotorOn = false;
  }

  motorStateChanged = true;

  Serial.print("Motor ");
  Serial.println(isMotorOn ? "ON" : "OFF");
}

// ============================================================
// KIRIM DATA KE BLYNK
// ============================================================

void kirimKeBlynk()
{
  if (!Blynk.connected())
    return;

  // V0 = RPM aktual dari sensor
  Blynk.virtualWrite(V0, rpm);

  // V1 = PWM aktual yang diterapkan ke motor.
  // Nilainya 0 ketika motor OFF.
  Blynk.virtualWrite(V1, getActualPWM());
}

// ============================================================
// STATUS SERIAL
// ============================================================

void tampilkanStatus(const char* aksi)
{
  Serial.print(aksi);
  Serial.print(" | RPM=");
  Serial.print(rpm, 1);

  Serial.print(" | PWM Set=");
  Serial.print(dutyCycle);

  Serial.print(" | PWM Aktual=");
  Serial.print(getActualPWM());

  Serial.print(" | Motor=");
  Serial.println(isMotorOn ? "ON" : "OFF");
}

// ============================================================
// PEMROSESAN PERINTAH SERIAL
// ============================================================

void prosesSerial()
{
  while (Serial.available() > 0)
  {
    char c = Serial.read();

    if (c == '\n' || c == '\r')
    {
      inputString.trim();

      if (inputString.startsWith("PWM:"))
      {
        int val = inputString.substring(4).toInt();

        dutyCycle = constrain(
          val,
          DUTY_MIN,
          DUTY_MAX
        );

        isMotorOn = (dutyCycle > 0);
        motorStateChanged = true;

        tampilkanStatus("Set PWM");
      }
      else if (inputString.length() > 0)
      {
        char cmd = inputString.charAt(0);

        if (cmd == '1')
        {
          if (dutyCycle == 0)
            dutyCycle = 200;

          isMotorOn = true;
          motorStateChanged = true;

          tampilkanStatus("Motor ON");
        }
        else if (cmd == '0')
        {
          isMotorOn = false;
          motorStateChanged = true;

          tampilkanStatus("Motor OFF");
        }
        else if (cmd == '+')
        {
          dutyCycle = constrain(
            dutyCycle + DUTY_STEP,
            DUTY_MIN,
            DUTY_MAX
          );

          if (dutyCycle > 0)
            isMotorOn = true;

          motorStateChanged = true;

          tampilkanStatus("PWM NAIK");
        }
        else if (cmd == '-')
        {
          dutyCycle = constrain(
            dutyCycle - DUTY_STEP,
            DUTY_MIN,
            DUTY_MAX
          );

          if (dutyCycle == 0)
            isMotorOn = false;

          motorStateChanged = true;

          tampilkanStatus("PWM TURUN");
        }
        else if (cmd == 'R' || cmd == 'r')
        {
          tampilkanStatus("Status");
        }
      }

      inputString = "";
    }
    else if (inputString.length() < 40)
    {
      inputString += c;
    }
  }
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(115200);
  delay(100);

  // Inisialisasi pin
  pinMode(ledPin, OUTPUT);

  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

  pinMode(pin_rpm, INPUT_PULLUP);

  // Pastikan motor mati ketika boot
  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(ledPin, LOW);

  setupMotorPWM();

  attachInterrupt(
    digitalPinToInterrupt(pin_rpm),
    isr,
    FALLING
  );

  old_time = millis();

  // Timer pengiriman RPM dan PWM
  timer.setInterval(
    SEND_INTERVAL_MS,
    kirimKeBlynk
  );

  Serial.println("==============================");
  Serial.println("iMCLab + Blynk.Edgent");
  Serial.println("RPM + PWM + Motor Control");
  Serial.print("Firmware versi: ");
  Serial.println(BLYNK_FIRMWARE_VERSION);
  Serial.println("==============================");
  Serial.println("1       = Motor ON");
  Serial.println("0       = Motor OFF");
  Serial.println("+       = PWM naik");
  Serial.println("-       = PWM turun");
  Serial.println("PWM:200 = Set PWM dan hidupkan motor");
  Serial.println("R       = Lihat RPM dan PWM");
  Serial.println("==============================");

  // Koneksi Edgent dan pengelolaan Blynk Air
  BlynkEdgent.begin();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
  // Koneksi Blynk dan mekanisme Blynk Air
  BlynkEdgent.run();

  // Pengukuran RPM dan pengiriman data
  hitungRPM();
  timer.run();

  // Perintah kontrol melalui Serial Monitor
  prosesSerial();

  // Terapkan perubahan motor
  updateDevice();
}
