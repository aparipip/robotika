/*********************************************************
 * iMCLab + ESP32 + Blynk IoT + OTA
 * RPM Monitoring, Kontrol PWM, dan Update Firmware
 * secara Over-The-Air (ArduinoOTA)
 *********************************************************/

#define BLYNK_TEMPLATE_ID "TMPL6rGehYQam"
#define BLYNK_TEMPLATE_NAME "robot"
#define BLYNK_AUTH_TOKEN "DDFLFzKPinPkVqKgA_36G48y0Al5ibPc"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ArduinoOTA.h>

//char ssid[] = "HOTSPOT@UPNJATIM.AC.ID";
//char pass[] = "belanegara";

//Konfigurasi Wifi
char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "HOTSPOT@UPNJATIM.AC.ID";
char pass[] = "belanegara";

// ============================================================
// OTA
// ============================================================

// Naikkan nomor ini setiap kali upload, supaya terlihat jelas
// di Serial Monitor bahwa firmware benar-benar sudah berganti.
#define FW_VERSION "1.0.0"

const char* OTA_HOSTNAME = "imclab-arif";
const char* OTA_PASSWORD = "robotika";

// ============================================================
// Motor dan LED
// ============================================================

int ledPin = 2;

int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 12;

// ============================================================
// Sensor RPM
// ============================================================

const byte pin_rpm = 13;
const int HOLES = 2;

volatile unsigned int rev = 0;
volatile unsigned long lastPulseTime = 0;

const unsigned long DEBOUNCE_US = 500;

// ============================================================
// PWM
// ============================================================

const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;

int dutyCycle = 200;

// Nilai yang dipakai saat Switch Blynk dinyalakan dari kondisi mati.
// Angka inilah yang diubah untuk membuktikan OTA lewat dashboard.
const int DEFAULT_DUTY = 200;

const int DUTY_STEP = 10;
const int DUTY_MIN = 0;
const int DUTY_MAX = 255;

bool motorEnable = false;

// ============================================================
// RPM
// ============================================================

float rpm = 0.0;
unsigned long old_time = 0;

// ============================================================

String inputString = "";

BlynkTimer timer;

// ============================================================
// ISR Encoder
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
// Sync setelah reconnect Blynk
// ============================================================

BLYNK_CONNECTED()
{
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V3);

  // Kirim versi firmware ke widget Label (V4).
  // Inilah bukti OTA yang terlihat langsung di dashboard Blynk.
  Blynk.virtualWrite(V4, FW_VERSION);
}

// ============================================================
// Hitung RPM
// ============================================================

void hitungRPM()
{
  unsigned long cur_time = millis();

  if (cur_time - old_time >= 1000)
  {
    detachInterrupt(digitalPinToInterrupt(pin_rpm));

    float rot_per_sec = (float)rev / (float)HOLES;

    rpm = rot_per_sec * 60.0;

    rev = 0;

    attachInterrupt(
      digitalPinToInterrupt(pin_rpm),
      isr,
      FALLING);

    old_time = millis();
  }
}

// ============================================================
// Terapkan PWM ke Motor
// ============================================================

void applyMotor()
{
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);

  ledcWrite(pwmChannel, dutyCycle);

  digitalWrite(ledPin, dutyCycle > 0);

  // Sinkronisasi ke Blynk
  Blynk.virtualWrite(V1, dutyCycle);
  Blynk.virtualWrite(V2, dutyCycle);
  Blynk.virtualWrite(V3, dutyCycle > 0);
}

// ============================================================
// Matikan motor secara paksa (dipakai sebelum OTA)
// ============================================================

void motorDarurat()
{
  dutyCycle = 0;
  motorEnable = false;

  ledcWrite(pwmChannel, 0);

  digitalWrite(motor1Pin1, LOW);
  digitalWrite(motor1Pin2, LOW);
  digitalWrite(ledPin, LOW);
}

// ============================================================
// Status Serial
// ============================================================

void tampilkanStatus(const char* aksi)
{
  Serial.print(aksi);
  Serial.print(" | PWM=");
  Serial.print(dutyCycle);
  Serial.print(" | RPM=");
  Serial.println(rpm, 1);
}

// ============================================================
// Kirim Data ke Blynk
// ============================================================

void kirimKeBlynk()
{
  Blynk.virtualWrite(V0, rpm);
  Blynk.virtualWrite(V1, dutyCycle);
  Blynk.virtualWrite(V2, dutyCycle);
  Blynk.virtualWrite(V3, dutyCycle > 0);
}

// ============================================================
// Slider PWM dari Blynk (V2)
// ============================================================

BLYNK_WRITE(V2)
{
  dutyCycle = constrain(
    param.asInt(),
    DUTY_MIN,
    DUTY_MAX);

  motorEnable = (dutyCycle > 0);

  applyMotor();

  Serial.print("PWM dari Blynk = ");
  Serial.println(dutyCycle);
}

// ============================================================
// Tombol ON/OFF dari Blynk (V3)
// ============================================================

BLYNK_WRITE(V3)
{
  motorEnable = param.asInt();

  if (motorEnable)
  {
    if (dutyCycle == 0)
      dutyCycle = DEFAULT_DUTY;
  }
  else
  {
    dutyCycle = 0;
  }

  applyMotor();

  Serial.print("Motor ");
  Serial.println(motorEnable ? "ON" : "OFF");
}

// ============================================================
// Konfigurasi OTA
// ============================================================

void setupOTA()
{
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]()
  {
    // Motor WAJIB dimatikan dulu. Board akan restart di tengah
    // proses, jadi motor tidak boleh dibiarkan berputar.
    motorDarurat();

    Serial.println();
    Serial.println("================================");
    Serial.println("OTA dimulai, motor dimatikan");
    Serial.println("================================");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    Serial.printf("OTA: %u%%\r", (progress * 100) / total);
  });

  ArduinoOTA.onEnd([]()
  {
    Serial.println();
    Serial.println("OTA selesai. Board restart...");
  });

  ArduinoOTA.onError([](ota_error_t error)
  {
    Serial.printf("OTA Error [%u]: ", error);

    if (error == OTA_AUTH_ERROR)         Serial.println("Auth gagal");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin gagal");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect gagal");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive gagal");
    else if (error == OTA_END_ERROR)     Serial.println("End gagal");
  });

  ArduinoOTA.begin();

  Serial.println("--------------------------------");
  Serial.print("OTA siap. Hostname : ");
  Serial.println(OTA_HOSTNAME);
  Serial.print("Alamat IP          : ");
  Serial.println(WiFi.localIP());
  Serial.println("--------------------------------");
}

// ============================================================
// Setup
// ============================================================

void setup()
{
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);

  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

  pinMode(pin_rpm, INPUT_PULLUP);

  attachInterrupt(
    digitalPinToInterrupt(pin_rpm),
    isr,
    FALLING);

  ledcSetup(
    pwmChannel,
    freq,
    resolution);

  ledcAttachPin(
    enable1Pin,
    pwmChannel);

  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);

  ledcWrite(pwmChannel, 0);

  old_time = millis();

  // Koneksi ke Blynk (sekaligus menyambungkan WiFi)
  Blynk.begin(auth, ssid, pass);

  // OTA dipasang SETELAH WiFi tersambung
  setupOTA();

  timer.setInterval(
    1000L,
    kirimKeBlynk);

  Serial.println("================================");
  Serial.print("iMCLab + Blynk + OTA Ready  v");
  Serial.println(FW_VERSION);
  Serial.println("================================");
  Serial.println("1 = ON");
  Serial.println("0 = OFF");
  Serial.println("+ = PWM naik");
  Serial.println("- = PWM turun");
  Serial.println("PWM:200");
  Serial.println("R = lihat RPM");
  Serial.println("V = lihat versi firmware & IP");
}

// ============================================================
// Loop
// ============================================================

void loop()
{
  Blynk.run();

  // Melayani permintaan upload OTA dari Arduino IDE
  ArduinoOTA.handle();

  timer.run();

  hitungRPM();

  while (Serial.available() > 0)
  {
    char c = Serial.read();

    if (c == '\n' || c == '\r')
    {
      inputString.trim();

      if (inputString.startsWith("PWM:"))
      {
        int val = inputString.substring(4).toInt();

        val = constrain(
          val,
          DUTY_MIN,
          DUTY_MAX);

        dutyCycle = val;
        motorEnable = (dutyCycle > 0);

        applyMotor();

        tampilkanStatus("Set PWM");
      }

      else if (inputString.length() > 0)
      {
        char cmd = inputString.charAt(0);

        if (cmd == '1')
        {
          if (dutyCycle == 0)
            dutyCycle = DEFAULT_DUTY;

          motorEnable = true;

          applyMotor();

          tampilkanStatus("Motor ON");
        }

        else if (cmd == '0')
        {
          dutyCycle = 0;
          motorEnable = false;

          applyMotor();

          Serial.println("Motor OFF");
        }

        else if (cmd == '+')
        {
          dutyCycle += DUTY_STEP;

          if (dutyCycle > DUTY_MAX)
            dutyCycle = DUTY_MAX;

          motorEnable = true;

          applyMotor();

          tampilkanStatus("PWM NAIK");
        }

        else if (cmd == '-')
        {
          dutyCycle -= DUTY_STEP;

          if (dutyCycle < DUTY_MIN)
            dutyCycle = DUTY_MIN;

          motorEnable = (dutyCycle > 0);

          applyMotor();

          tampilkanStatus("PWM TURUN");
        }

        else if (cmd == 'R' || cmd == 'r')
        {
          Serial.print("RPM = ");
          Serial.print(rpm, 1);
          Serial.print(" | PWM = ");
          Serial.println(dutyCycle);
        }

        else if (cmd == 'V' || cmd == 'v')
        {
          Serial.print("Firmware v");
          Serial.print(FW_VERSION);
          Serial.print(" | Hostname: ");
          Serial.print(OTA_HOSTNAME);
          Serial.print(" | IP: ");
          Serial.println(WiFi.localIP());
        }
      }

      inputString = "";
    }
    else
    {
      inputString += c;
    }
  }
}
