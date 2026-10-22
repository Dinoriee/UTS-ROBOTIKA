int ledPin = 2;
char myData = 0;

int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 12;

// Setting PWM properties
const int freq = 5000;          // diturunkan agar driver motor responsif
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 205;

const byte pin_rpm = 13;
volatile unsigned int pulseCount = 0;
unsigned long lastRpmTime = 0;
float rpm = 0.0;

// Jumlah slot/pulses per putaran (sesuaikan dengan encoder/sensor Anda)
// Contoh: 1 slot = 1 pulse per rotasi
const int PULSES_PER_REV = 1;

// ===== ISR: dipanggil setiap pulse dari sensor RPM =====
void IRAM_ATTR rpmISR() {
  pulseCount++;
}

void setup()
{
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);
  pinMode(pin_rpm, INPUT_PULLUP);

  // Aktifkan interrupt pada pin RPM
  attachInterrupt(digitalPinToInterrupt(pin_rpm), rpmISR, RISING);

  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(enable1Pin, pwmChannel);

  Serial.println("Testing DC Motor...");
  lastRpmTime = millis();
}

void MotorOn()
{
  dutyCycle = 100;
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, dutyCycle);
}

void MotorOff()
{
  dutyCycle = 0;
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, dutyCycle);
}

// ===== Hitung RPM setiap 1 detik =====
void hitungRPM() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastRpmTime;

  if (elapsed >= 1000) {  // setiap 1 detik
    noInterrupts();
    unsigned int count = pulseCount;
    pulseCount = 0;
    interrupts();

    rpm = (count * 60.0) / (PULSES_PER_REV * (elapsed / 1000.0));

    // Kirim ke Serial dalam format mudah dibaca Python: "RPM:xxx.xx"
    Serial.print("RPM:");
    Serial.println(rpm, 2);

    lastRpmTime = now;
  }
}

void loop()
{
  // Baca perintah dari Python
  if (Serial.available() > 0) {
    myData = Serial.read();

    if (myData == '1') {
      Serial.println("LED is on !!!");
      digitalWrite(ledPin, HIGH);
      MotorOn();
    }
    else if (myData == '0') {
      Serial.println("LED is off !!!");
      digitalWrite(ledPin, LOW);
      MotorOff();
    }
  }

  // Update RPM terus-menerus
  hitungRPM();
}