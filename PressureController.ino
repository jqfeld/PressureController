#include <Wire.h>
#include <ADS1115_WE.h> 

#define ADC_ADDRESS 0x48
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>
#include <Adafruit_MCP4725.h>


Adafruit_MCP4725 dac;




LiquidCrystal_I2C lcd(0x27, 16, 2);
const int CLK = 4;  // Definition der Pins. CLK an D4, DT an D3.
const int DT = 3;
const int SW = 2;  // Der Switch wird mit Pin D2 Verbunden. ACHTUNG : Verwenden Sie einen interrupt-Pin!
Encoder encoder(DT, CLK);

int32_t pressure_set = 20000;
int32_t pressure_cur = 0;
char pressure_set_str[10];
char pressure_cur_str[10];

bool setting_pressure = false;

int32_t control_voltage = 0;
const double kp = -0.5;

ADS1115_WE adc = ADS1115_WE(ADC_ADDRESS);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Wire.begin();
  if(!adc.init()){
    Serial.println("ADS1115 not connected!");
  }
  adc.setVoltageRange_mV(ADS1115_RANGE_6144);
  dac.begin(0x60);
  lcd.init();
  lcd.backlight();
  pinMode(SW, INPUT);
  encoder.write(pressure_set);
  attachInterrupt(digitalPinToInterrupt(SW), Interrupt, LOW);


    Serial.println("ADS1115 Example Sketch - Single Shot Mode");
  Serial.println("Channel / Voltage [V]: ");
  Serial.println();
}

void loop() {
  // put your main code here, to run repeatedly:
  lcd.setCursor(0, 0);  //Hier wird die Position des ersten Zeichens festgelegt. In diesem Fall bedeutet (0,0) das erste Zeichen in der ersten Zeile.
  lcd.print("Set:");
  snprintf(pressure_set_str, 10, "%6lu Pa", pressure_set);
  lcd.print(pressure_set_str);
  // lcd.print(" Pa");

  if (setting_pressure) {
    lcd.setCursor(15, 0);
    lcd.print("*");
    int32_t pressure_set_new = encoder.read();  // Die "neue" Position des Encoders wird definiert. Dabei wird die aktuelle Position des Encoders über die Variable.Befehl() ausgelesen.

    if (pressure_set_new != pressure_set)  // Sollte die neue Position ungleich der alten (-999) sein (und nur dann!!)...
    {
      pressure_set = pressure_set_new;
     
      // Serial.println(pressure_set);      // ...soll die aktuelle Position im seriellen Monitor ausgegeben werden.
    }
  } else {
    lcd.setCursor(15, 0);
    lcd.print(" ");
    
    // PID loop will come here
    
    int32_t error = pressure_set - pressure_cur;
    Serial.print("error="); 
    Serial.println(error); 
    control_voltage = round(control_voltage + kp * error);
    if (control_voltage < 0) {
      control_voltage = 0;
    } else if (control_voltage > 4096) {
      control_voltage = 4095;
    }
    Serial.print("control_voltage="); 
    Serial.println(control_voltage); 

    dac.setVoltage(uint16_t(control_voltage), true);
    // dac.setVoltage(uint16_t(4000), true);

    // delay(100);

  }
  pressure_cur = round(readPressure(ADS1115_COMP_0_GND));
  lcd.setCursor(0, 1);  // In diesem Fall bedeutet (0,1) das erste Zeichen in der zweiten Zeile.
  lcd.print("Cur:");
  snprintf(pressure_cur_str, 10, "%6lu Pa", pressure_cur);
  lcd.print(pressure_cur_str);
  // lcd.print(" Pa");
}

void Interrupt()  // Beginn des Interrupts. Wenn der Rotary Knopf betätigt wird, springt das Programm automatisch an diese Stelle. Nachdem...
{
  if (setting_pressure) {
    setting_pressure = false;
  } else {
    setting_pressure = true;
  }
}

float readPressure(ADS1115_MUX channel) {
  float voltage = 0.0;
  const float divider = 0.5;
  // Divider value depends on the voltage divider circuit that is used to reduce
  // the output voltage of the pressure gauge to values suitable for the ADC.
  // In case of a 10 V max output voltage it should be a 50% divider to not 
  // exceed the 5 V max of the ADC.

  unsigned long pressure = 0;
  adc.setCompareChannels(channel);
  adc.startSingleMeasurement();
  while(adc.isBusy()){}
  voltage = adc.getResult_mV() / divider; // alternative: getResult_mV for Millivolt
  pressure = round((voltage - 1000.0)*12.60);
  return pressure;
}
