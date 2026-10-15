// ============================================================
// Kontrol Motor DC + LED dengan PWM via Serial
// Perintah:
//   '1'        -> Motor ON (duty terakhir / default)
//   '0'        -> Motor OFF
//   '+'        -> Naikkan kecepatan (+10)
//   '-'        -> Turunkan kecepatan (-10)
//   'PWM:xxx'  -> Set duty cycle langsung (0-255)
// ============================================================

int ledPin = 2;
char myData = 0;

int motor1Pin1 = 27;
int motor1Pin2 = 26;
int enable1Pin = 12;

// Setting PWM properties
const int freq = 30000;
const int pwmChannel = 0;
const int resolution = 8;
int dutyCycle = 200;         // duty cycle awal
const int DUTY_STEP = 10;    // langkah naik/turun
const int DUTY_MIN  = 0;
const int DUTY_MAX  = 255;

String inputString = "";     // buffer untuk perintah "PWM:xxx"

void setup()
{
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

  pinMode(motor1Pin1, OUTPUT);
  pinMode(motor1Pin2, OUTPUT);
  pinMode(enable1Pin, OUTPUT);

  // konfigurasi PWM
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(enable1Pin, pwmChannel);

  // pastikan motor mati saat start
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, 0);

  Serial.println("ESP32 Motor Controller siap.");
  Serial.println("Perintah: 1=ON, 0=OFF, +=naik, -=turun, PWM:xxx=set");
}

void applyMotor()
{
  // Terapkan dutyCycle ke driver motor
  digitalWrite(motor1Pin1, HIGH);
  digitalWrite(motor1Pin2, LOW);
  ledcWrite(pwmChannel, dutyCycle);

  // LED indikator: nyala kalau motor bergerak
  digitalWrite(ledPin, dutyCycle > 0 ? HIGH : LOW);
}

void MotorOn()
{
  // Kalau duty masih 0, pakai default 200
  if (dutyCycle == 0) dutyCycle = 200;
  applyMotor();
  Serial.print("Motor ON, duty = ");
  Serial.println(dutyCycle);
}

void MotorOff()
{
  dutyCycle = 0;
  applyMotor();
  Serial.println("Motor OFF, duty = 0");
}

void naikkanKecepatan()
{
  dutyCycle += DUTY_STEP;
  if (dutyCycle > DUTY_MAX) dutyCycle = DUTY_MAX;
  applyMotor();
  Serial.print("Kecepatan NAIK, duty = ");
  Serial.println(dutyCycle);
}

void turunkanKecepatan()
{
  dutyCycle -= DUTY_STEP;
  if (dutyCycle < DUTY_MIN) dutyCycle = DUTY_MIN;
  applyMotor();
  Serial.print("Kecepatan TURUN, duty = ");
  Serial.println(dutyCycle);
}

void setDuty(int val)
{
  if (val < DUTY_MIN) val = DUTY_MIN;
  if (val > DUTY_MAX) val = DUTY_MAX;
  dutyCycle = val;
  applyMotor();
  Serial.print("Set duty = ");
  Serial.println(dutyCycle);
}

void loop()
{
  while (Serial.available() > 0)
  {
    char c = Serial.read();

    // Cek perintah "PWM:xxx"
    if (c == '\n' || c == '\r')
    {
      inputString.trim();
      if (inputString.startsWith("PWM:"))
      {
        int val = inputString.substring(4).toInt();
        setDuty(val);
      }
      else if (inputString.length() > 0)
      {
        // perintah satu karakter
        char cmd = inputString.charAt(0);
        if (cmd == '1')      MotorOn();
        else if (cmd == '0') MotorOff();
        else if (cmd == '+') naikkanKecepatan();
        else if (cmd == '-') turunkanKecepatan();
        else {
          Serial.print("Perintah tidak dikenal: ");
          Serial.println(inputString);
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