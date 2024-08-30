const int VERSION = 2;

#include <Wire.h>
#include <ADS1115_WE.h>

#define ADC_ADDRESS 0x48
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>
#include <Adafruit_MCP4725.h>

#include <avr/interrupt.h>
#include <Vrekrer_scpi_parser.h>
#include <EEPROM.h>

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

volatile bool setting_pressure = false;

volatile int32_t control_voltage = 0;
const int kP_addr = 0x10;
const int kI_addr = 0x20;
const int kD_addr = 0x30;
const int dt_addr = 0x40;
const int offset_addr = 0x50;
const int scale_addr = 0x60;

volatile uint32_t dt_ms = 100;
volatile double kP = 1e-4;
volatile double kI = 0.1 / dt_ms;
volatile double kD = 1e-3 * dt_ms;
volatile double voltage = 0.0;

volatile double offset = 0.0;
volatile double scale = 10.0;
volatile bool reverse = true;

int magic = 0;


volatile double I = 0.0;
volatile int32_t prev_error = 0;

volatile bool debug = false;


ADS1115_WE adc = ADS1115_WE(ADC_ADDRESS);

SCPI_Parser instrument;

void setup() {
  // setup serial communication
  Serial.begin(9600);

  // setup I2C
  Wire.begin();

  // setup ADC
  if (!adc.init()) {
    Serial.println("ADS1115 not connected!");
  }
  adc.setVoltageRange_mV(ADS1115_RANGE_6144);
  adc.setCompareChannels(ADS1115_COMP_0_GND);
  dac.begin(0x60);

  // setup LCD discply
  lcd.init();
  lcd.backlight();

  // setup button of rotary encoder
  pinMode(SW, INPUT);
  encoder.write(pressure_set);
  attachInterrupt(digitalPinToInterrupt(SW), buttonInterrupt, LOW);


  // setup timer of PIC loop
  cli();
  // with Prescaler=256 the timer is increase by 256* (1/16MHz) = 256 * 6.25 ns = 16 us
  TCCR1A = 0;
  TCCR1B = 0;
  OCR1A = uint16_t(dt_ms * 1000 / 16);
  TCCR1B = (1 << WGM12) | (1 << CS12);  // set CS1 to 100 (Prescaler=256)
  TIMSK1 = (1 << OCIE1A);
  sei();
  Serial.print("OCR1A:");
  Serial.println(OCR1A, HEX);


  instrument.SetCommandTreeBase(F("PREssure"));
  instrument.RegisterCommand(F(":CURrent?"), &GetCurrentPressure);
  instrument.RegisterCommand(F(":SET"), &SetSetPressure);
  instrument.RegisterCommand(F(":SET?"), &GetSetPressure);

  instrument.SetCommandTreeBase(F("SYSTem"));
  instrument.RegisterCommand(F(":DEBug"), &SetDebug);
  instrument.RegisterCommand(F(":RESet"), &ResetEEPROM);

  instrument.SetCommandTreeBase(F("SYSTem:PID"));
  instrument.RegisterCommand(F(":P"), &SetP);
  instrument.RegisterCommand(F(":P?"), &GetP);
  instrument.RegisterCommand(F(":I"), &SetI);
  instrument.RegisterCommand(F(":I?"), &GetI);
  instrument.RegisterCommand(F(":D"), &SetD);
  instrument.RegisterCommand(F(":D?"), &GetD);
  instrument.RegisterCommand(F(":DT"), &SetDT);
  instrument.RegisterCommand(F(":DT?"), &GetDT);
  instrument.SetCommandTreeBase(F("SYSTem:PREssure"));
  instrument.RegisterCommand(F(":OFFSet"), &SetOffset);
  instrument.RegisterCommand(F(":OFFSet?"), &GetOffset);
  instrument.RegisterCommand(F(":SCALe"), &SetScale);
  instrument.RegisterCommand(F(":SCALe?"), &GetScale);

  EEPROM.get(0, magic);

  if (magic == VERSION) {
    EEPROM.get(kP_addr, kP);
    EEPROM.get(kI_addr, kI);
    EEPROM.get(kD_addr, kD);
    EEPROM.get(dt_addr, dt_ms);
    EEPROM.get(offset_addr, offset);
    EEPROM.get(scale_addr, scale);

    Serial.println("Configuration loaded!");

  } else {
    EEPROM.put(kP_addr, kP);
    EEPROM.put(kI_addr, kI);
    EEPROM.put(kD_addr, kD);
    EEPROM.put(dt_addr, dt_ms);
    EEPROM.put(offset_addr, offset);
    EEPROM.put(scale_addr, scale);
    EEPROM.put(0, VERSION);
    Serial.println("No configuration found, set defaults!");
  }

  Serial.println("Setup completed!");
}

void loop() {
  // put your main code here, to run repeatedly:
  lcd.setCursor(0, 0);
  lcd.print("Set:");
  snprintf(pressure_set_str, 10, "%6lu Pa", pressure_set);
  lcd.print(pressure_set_str);

  if (setting_pressure) {
    lcd.setCursor(15, 0);
    lcd.print("*");

    int32_t pressure_set_new = encoder.read();

    if (pressure_set_new != pressure_set) {
      pressure_set = pressure_set_new;
    }
  } else {
    lcd.setCursor(15, 0);
    lcd.print(" ");
  }
  pressure_cur = round(readPressure(ADS1115_COMP_0_GND));
  lcd.setCursor(0, 1);  // In diesem Fall bedeutet (0,1) das erste Zeichen in der zweiten Zeile.
  lcd.print("Cur:");
  snprintf(pressure_cur_str, 10, "%6lu Pa", pressure_cur);
  lcd.print(pressure_cur_str);

  // set the control_voltage (calculated in the PID loop)
  dac.setVoltage(uint16_t(control_voltage), true);



  // debug output
  if (debug) {
        Serial.print("voltage:");
    Serial.print(voltage);
        Serial.print(",");

    Serial.print("control_voltage:");
    Serial.print(control_voltage);
    Serial.print(",");
    Serial.print("error:");
    Serial.print(prev_error);
    Serial.print(",");
    Serial.print("pressure_set:");
    Serial.print(pressure_set);
    Serial.print(",");
    Serial.print("pressure_cur:");
    Serial.println(pressure_cur);
  }
  instrument.ProcessInput(Serial, "\n");
}


// One iteration of PID loop
ISR(TIMER1_COMPA_vect) {
  // PID loop will come here

  int32_t error = pressure_set - pressure_cur;
  // Serial.print("error=");
  // Serial.println(error);
  double P, D;
  if (reverse) {
      P = -kP * error;
      I -= kI * error * dt_ms;
      D = kD * (error - prev_error) / dt_ms;

  } else {
      P = kP * error;
      I += kI * error * dt_ms;
      D = -kD * (error - prev_error) / dt_ms;
  }
  
  control_voltage = round(P + I + D);

  prev_error = error;

  // Limit to valid DAC values
  // if (control_voltage < 0) {
  //   control_voltage = 0;
  // } else if (control_voltage > 4096) {
  //   control_voltage = 4095;
  // }
  control_voltage = constrain(control_voltage, 0, 4095);
  I = constrain(I, 0, 4095);
  // Serial.print("control_voltage=");
  // Serial.println(control_voltage);
}



void buttonInterrupt()  // Beginn des Interrupts. Wenn der Rotary Knopf betätigt wird, springt das Programm automatisch an diese Stelle. Nachdem...
{
  if (setting_pressure) {
    setting_pressure = false;

  } else {
    setting_pressure = true;
  }
}

float readPressure(ADS1115_MUX channel) {
  const float divider = 0.5;

  // Divider value depends on the voltage divider circuit that is used to reduce
  // the output voltage of the pressure gauge to values suitable for the ADC.
  // In case of a 10 V max output voltage it should be a 50% divider to not
  // exceed the 5 V max of the ADC.

  unsigned long pressure = 0;
  adc.setCompareChannels(channel);
  adc.startSingleMeasurement();
  while (adc.isBusy()) {}
  voltage = adc.getResult_mV();           // alternative: getResult_mV for Millivolt
  double rel_voltage = (voltage / divider - offset);  // The output starts with 1V for zero pressure
  if (rel_voltage < 0) {
    rel_voltage = 0;
  }
  pressure = round(rel_voltage * scale);

  return pressure;
}


void GetCurrentPressure(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(pressure_cur);
}

void SetSetPressure(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    pressure_set = constrain(String(parameters[0]).toInt(), 0, 100000);
      encoder.write(pressure_set);

  }
}
void GetSetPressure(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(pressure_set);
}

void SetP(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    kP = constrain(String(parameters[0]).toDouble(), 0, 100);
    EEPROM.put(kP_addr, kP);
  }
}
void GetP(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(String(kP, 5));
}

void SetI(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    kI = constrain(String(parameters[0]).toDouble(), 0, 100);
    EEPROM.put(kI_addr, kI);
  }
}
void GetI(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(String(kI, 5));
}

void SetD(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    kD = constrain(String(parameters[0]).toDouble(), 0, 100);
    EEPROM.put(kD_addr, kD);
  }
}
void GetD(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(String(kD, 5));
}

void SetDT(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    dt_ms = constrain(String(parameters[0]).toDouble(), 0, 10000);
    EEPROM.put(dt_addr, dt_ms);
  }
}
void GetDT(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(dt_ms);
}

void SetOffset(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    offset = constrain(String(parameters[0]).toDouble(), 0, 10000);
    EEPROM.put(offset_addr, offset);
  }
}
void GetOffset(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(offset);
}
void SetScale(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    scale = constrain(String(parameters[0]).toDouble(), 0, 10000);
    EEPROM.put(scale_addr, scale);
  }
}
void GetScale(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  interface.println(scale);
}



void SetDebug(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  if (parameters.Size() > 0) {
    debug = constrain(String(parameters[0]).toInt(), 0, 2);
  }
}


void ResetEEPROM(SCPI_C commands, SCPI_P parameters, Stream& interface) {
  EEPROM.put(kP_addr, double(0));
  EEPROM.put(kI_addr, double(0));
  EEPROM.put(kD_addr, double(0));
  EEPROM.put(dt_addr, int32_t(0));
  EEPROM.put(0, 0);
  interface.println("EEPROM resetted. Please restart device.");
}

void DoNothing(SCPI_C commands, SCPI_P parameters, Stream& interface) {
}