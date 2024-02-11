#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
const int CLK = 4;  // Definition der Pins. CLK an D6, DT an D5.
const int DT = 3;
const int SW = 2;  // Der Switch wird mit Pin D2 Verbunden. ACHTUNG : Verwenden Sie einen interrupt-Pin!
Encoder encoder(DT, CLK);

long pressure_set = 20000;
long pressure_cur = 0;
char pressure_set_str[6];
char pressure_cur_str[6];

bool setting_pressure = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  pinMode(SW, INPUT);
  encoder.write(pressure_set);
  attachInterrupt(digitalPinToInterrupt(SW), Interrupt, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  lcd.setCursor(0, 0);  //Hier wird die Position des ersten Zeichens festgelegt. In diesem Fall bedeutet (0,0) das erste Zeichen in der ersten Zeile.
  lcd.print("Set: ");
  snprintf(pressure_set_str, 6, "%d", pressure_set);
  lcd.print(pressure_set_str);
  lcd.print(" Pa");

  if (setting_pressure) {
    lcd.setCursor(15, 0);
    lcd.print("*");
    long pressure_set_new = encoder.read();  // Die "neue" Position des Encoders wird definiert. Dabei wird die aktuelle Position des Encoders über die Variable.Befehl() ausgelesen.

    if (pressure_set_new != pressure_set)  // Sollte die neue Position ungleich der alten (-999) sein (und nur dann!!)...
    {
      pressure_set = pressure_set_new;
      // Serial.println(pressure_set);      // ...soll die aktuelle Position im seriellen Monitor ausgegeben werden.
    }
  } else {
    lcd.setCursor(15, 0);
    lcd.print(" ");
    // PID loop will come here
    pressure_cur = pressure_set;
  }

  lcd.setCursor(0, 1);  // In diesem Fall bedeutet (0,1) das erste Zeichen in der zweiten Zeile.
  lcd.print("Cur: ");
  snprintf(pressure_cur_str, 6, "%d", pressure_cur);
  lcd.print(pressure_cur_str);
  lcd.print(" Pa");
}

void Interrupt()  // Beginn des Interrupts. Wenn der Rotary Knopf betätigt wird, springt das Programm automatisch an diese Stelle. Nachdem...
{
  if (setting_pressure) {
    setting_pressure = false;
  } else {
    setting_pressure = true;
  }
}