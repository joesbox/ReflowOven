#include <CountDown.h>
#include <EEPROM.h>
#include <Adafruit_MAX31856.h>
#include <Encoder.h>
#include <elapsedMillis.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#include <AutoPID.h>
#include <elapsedMillis.h>
#include <Bounce2.h>

#define TFT_RST        5 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         6
#define TFT_CS         7

elapsedMillis heaterFire;

// PID settings and gains
#define OUTPUT_MIN 0
#define OUTPUT_MAX 1150
#define KP 100
#define KI 10
#define KD 2

Encoder knobLeft(3, 2);
Bounce2::Button button = Bounce2::Button();
int deboucedInput;
int menuIdx;
bool enterValue;

// For 1.14", 1.3", 1.54", and 2.0" TFT with ST7789:
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
float p = 3.1415926;
long positionLeft  = -999;

Adafruit_MAX31856 maxthermo = Adafruit_MAX31856(10);
float temp;
bool checkTC;

uint8_t variables[4];
bool startSoak;
bool timerStarted;

CountDown CD;

double temperature, setPoint, outputVal;

AutoPID myPID(&temperature, &setPoint, &outputVal, OUTPUT_MIN, OUTPUT_MAX, KP, KI, KD);

enum systemStates {
  Off,
  Preheat,
  Reflow,
  Cooling
};

const char* stateStr[] = {"OFF         ", "PREHEAT", "REFLOW", "COOLING     "};

systemStates state = Off;

void setup(void) {
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
  button.attach(4, INPUT_PULLUP);
  button.interval(5);
  button.setPressedState(LOW);
  menuIdx = 0;
  enterValue = false;

  pinMode(8, OUTPUT);
  Serial.begin(9600);
  Serial.print("Setpoint");
  Serial.print("Temperature");
  maxthermo.begin();
  maxthermo.setThermocoupleType(MAX31856_TCTYPE_K);
  maxthermo.setNoiseFilter(MAX31856_NOISE_FILTER_50HZ); // UK mains
  maxthermo.setConversionMode(MAX31856_ONESHOT_NOWAIT);
  checkTC = false;

  pinMode(8, OUTPUT);
  digitalWrite(8, LOW);

  readVariables();
  startSoak = false;
  timerStarted = false;

  // OR use this initializer (uncomment) if using a 1.3" or 1.54" 240x240 TFT:
  tft.init(240, 240);           // Init ST7789 240x240

  tft.setRotation(3);
  tft.setTextSize(2);
  tft.fillScreen(ST77XX_BLACK);
  tft.fillRect(0, 0, 240, 18, 0x0273);
  tft.setCursor(80, 2);
  tft.println("PREHEAT");
  tft.setCursor(0, 22);
  tft.print("TEMP: ");
  tft.print(variables[0]);
  tft.setCursor(215, 22);
  tft.print("<-");
  tft.setCursor(0, 45);
  tft.print("DURATION (S): ");
  tft.print(variables[1]);
  tft.fillRect(0, 70, 240, 18, 0x0273);
  tft.setCursor(84, 72);
  tft.println("REFLOW");
  tft.setCursor(0, 92);
  tft.print("TEMP: ");
  tft.print(variables[2]);
  tft.setCursor(0, 115);
  tft.print("DURATION (S): ");
  tft.print(variables[3]);
  tft.fillRect(0, 140, 240, 18, 0x0273);
  tft.setCursor(84, 142);
  tft.println("STATUS");
  tft.setCursor(0, 162);
  tft.print(stateStr[state]);
  tft.setCursor(0, 185);
  tft.print("TEMPERATURE: ");
  tft.print(temperature);
  tft.drawRect(0, 220, 119, 20, ST77XX_RED);
  tft.setCursor(40, 222);
  tft.print("STOP");
  tft.drawRect(120, 220, 119, 20, ST77XX_GREEN);
  tft.setCursor(150, 222);
  tft.print("START");


  myPID.setTimeStep(1000);
  myPID.setBangBang(8);
}

void loop() {
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  long newLeft;
  long test;
  long increment;
  newLeft = knobLeft.read();
  test = newLeft % 4;
  if (test == 0)
  {
    if (newLeft != positionLeft)
    {
      positionLeft = newLeft;
      increment = positionLeft / 4;
      if (enterValue)
      {
        switch (menuIdx)
        {
          case 0:
            variables[0] += increment;
            tft.setCursor(0, 22);
            tft.print("TEMP: ");
            tft.print(variables[0]);
            break;

          case 1:
            variables[1] += increment;
            tft.setCursor(0, 45);
            tft.print("DURATION (S): ");
            tft.print(variables[1]);

            break;

          case 2:
            variables[2] += increment;
            tft.setCursor(0, 92);
            tft.print("TEMP: ");
            tft.print(variables[2]);

            break;

          case 3:
            variables[3] += increment;
            tft.setCursor(0, 115);
            tft.print("DURATION (S): ");
            tft.print(variables[3]);
            break;

        }

        tft.print("  ");
        knobLeft.write(0);
      }
      else
      {
        menuIdx += increment;
        knobLeft.write(0);
        if (menuIdx > 5)
        {
          menuIdx = 0;
        }
        else if (menuIdx < 0)
        {
          menuIdx = 5;
        }

        switch (menuIdx)
        {
          case 0:
            tft.setCursor(215, 22);
            tft.print("<-");
            tft.setCursor(215, 45);
            tft.print("  ");
            tft.setCursor(215, 92);
            tft.print("  ");
            tft.setCursor(215, 115);
            tft.print("  ");
            tft.fillRect(0, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(0, 220, 119, 20, ST77XX_RED);
            tft.setCursor(40, 222);
            tft.print("STOP");
            tft.fillRect(120, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(120, 220, 119, 20, ST77XX_GREEN);
            tft.setCursor(150, 222);
            tft.print("START");

            break;

          case 1:
            tft.setCursor(215, 22);
            tft.print("  ");
            tft.setCursor(215, 45);
            tft.print("<-");
            tft.setCursor(215, 92);
            tft.print("  ");
            tft.setCursor(215, 115);
            tft.print("  ");
            tft.fillRect(0, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(0, 220, 119, 20, ST77XX_RED);
            tft.setCursor(40, 222);
            tft.print("STOP");
            tft.fillRect(120, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(120, 220, 119, 20, ST77XX_GREEN);
            tft.setCursor(150, 222);
            tft.print("START");

            break;

          case 2:
            tft.setCursor(215, 22);
            tft.print("  ");
            tft.setCursor(215, 45);
            tft.print("  ");
            tft.setCursor(215, 92);
            tft.print("<-");
            tft.setCursor(215, 115);
            tft.print("  ");
            tft.fillRect(0, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(0, 220, 119, 20, ST77XX_RED);
            tft.setCursor(40, 222);
            tft.print("STOP");
            tft.fillRect(120, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(120, 220, 119, 20, ST77XX_GREEN);
            tft.setCursor(150, 222);
            tft.print("START");

            break;

          case 3:
            tft.setCursor(215, 22);
            tft.print("  ");
            tft.setCursor(215, 45);
            tft.print("  ");
            tft.setCursor(215, 92);
            tft.print("  ");
            tft.setCursor(215, 115);
            tft.print("<-");
            tft.fillRect(0, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(0, 220, 119, 20, ST77XX_RED);
            tft.setCursor(40, 222);
            tft.print("STOP");
            tft.fillRect(120, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(120, 220, 119, 20, ST77XX_GREEN);
            tft.setCursor(150, 222);
            tft.print("START");

            break;

          case 4:
            tft.setCursor(215, 22);
            tft.print("  ");
            tft.setCursor(215, 45);
            tft.print("  ");
            tft.setCursor(215, 92);
            tft.print("  ");
            tft.setCursor(215, 115);
            tft.print("  ");
            tft.fillRect(0, 220, 119, 20, ST77XX_RED);
            tft.setCursor(40, 222);
            tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
            tft.print("STOP");
            tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
            tft.fillRect(120, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(120, 220, 119, 20, ST77XX_GREEN);
            tft.setCursor(150, 222);
            tft.print("START");

            break;

          case 5:
            tft.setCursor(215, 22);
            tft.print("  ");
            tft.setCursor(215, 45);
            tft.print("  ");
            tft.setCursor(215, 92);
            tft.print("  ");
            tft.setCursor(215, 115);
            tft.print("  ");
            tft.fillRect(0, 220, 119, 20, ST77XX_BLACK);
            tft.drawRect(0, 220, 119, 20, ST77XX_RED);
            tft.setCursor(40, 222);
            tft.print("STOP");
            tft.fillRect(120, 220, 119, 20, ST77XX_GREEN);
            tft.setCursor(150, 222);
            tft.setTextColor(ST77XX_WHITE, ST77XX_GREEN);
            tft.print("START");
            tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

            break;
        }
      }
    }
  }
  button.update();

  if ( button.pressed() ) 
  {
    if (state == Off && menuIdx < 4)
    {
      if (enterValue == false)
      {
        enterValue = true;
        tft.setTextColor(ST77XX_BLACK, ST77XX_WHITE);
      }
      else
      {
        enterValue = false;
        tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
      }
    }

    switch (menuIdx)
    {
      case 0:
        tft.setCursor(215, 22);
        tft.print("<-");
        break;

      case 1:
        tft.setCursor(215, 45);
        tft.print("<-");
        break;

      case 2:
        tft.setCursor(215, 92);
        tft.print("<-");
        break;

      case 3:
        tft.setCursor(215, 115);
        tft.print("<-");
        break;

      case 4:
        state = Off;
        break;

      case 5:
        state = Preheat;

        // Update variables when we start a profile
        writeVariables();
        break;
    }
  }

  if (!checkTC)
  {
    maxthermo.triggerOneShot();
    checkTC = true;
  }

  if (checkTC && maxthermo.conversionComplete())
  {
    temperature = maxthermo.readThermocoupleTemperature();
    checkTC = false;
    tft.setCursor(0, 162);
    tft.print(stateStr[state]);
    if (CD.isRunning())
    {
      tft.print(", SOAK: ");
      tft.print(CD.remaining());
      tft.print("  ");
    }
    else
    {
      tft.print("           ");
    }
    tft.setCursor(0, 185);
    tft.print("TEMPERATURE: ");
    tft.print(temperature);
    tft.print("   ");
  }

  switch (state)
  {
    case Off:
      setPoint = 0;
      startSoak = false;
      CD.stop();
      myPID.stop();
      break;

    case Preheat:
      setPoint = variables[0];
      myPID.run();
      if (!startSoak)
      {
        startSoak = myPID.atSetPoint(2);
      }
      else
      {
        if (CD.isRunning())
        {
          timerStarted = true;
        }
        else
        {
          if (timerStarted)
          {
            CD.stop();
            startSoak = false;
            state = Reflow;
            timerStarted = false;
          }
          else
          {
            CD.start(0, 0, 0, variables[1]);
          }
        }
      }
      break;

    case Reflow:
      setPoint = variables[2];
      myPID.run();
      if (!startSoak)
      {
        startSoak = myPID.atSetPoint(2);
      }
      else
      {
        if (CD.isRunning())
        {
          timerStarted = true;
        }
        else
        {
          if (timerStarted)
          {
            CD.stop();
            startSoak = false;
            state = Cooling;
            timerStarted = false;
          }
          else
          {
            CD.start(0, 0, 0, variables[3]);
          }
        }
      }
      break;

    case Cooling:
      setPoint = 0;
      myPID.stop();
      break;
  }

  if (!myPID.isStopped())
  {
    if (heaterFire < outputVal)
    {
      digitalWrite(8, HIGH);
    }
    else if (heaterFire > outputVal)
    {
      digitalWrite(8, LOW);
    }

    if (heaterFire > 1000)
    {
      heaterFire = 0;
    }
  }
  else
  {
    digitalWrite(8, LOW);
  }
  Serial.print(setPoint);
  Serial.print(",");
  Serial.println(temperature);
  
}

void writeVariables()
{
  for (int i = 0; i < 4; i++)
  {
    EEPROM.write(i, variables[i]);
  }
}

void readVariables()
{
  for (int i = 0; i < 4; i++)
  {
    variables[i] = EEPROM.read(i);
  }
}
