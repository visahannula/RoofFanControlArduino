/***
 * Fan Control Code to Arduino 
 * 
 * This code has been made for controlling Vilpe Eco 220P roof fan
 * with Arduino UNO (Atmega328P).
 * 
 * Arduino controls fan with PWM to NPN transistor.
 * 
 * PWM duty is controlled using potentiometer (note: PWM uses Timer2).
 * 
 * LCD can be used to display the set speed value and the tacho signal
 * coming from the fan circuit (actual spinning RPM)
 * 
 * https://www.vilpe.com/product/e220p-o160-500-roof-fan/
 * 
 * See wiring instructions on on Vilpe site.
 * 
 * 
 * Made by Visa Hannula, 2021.
 * 
 * 
 * This code is provided AS IS and does not promise anything,
 * USE AT YOUR OWN RISK. You are fully free to copy and modify the code.
 * 
 * If you find errors or implement nice new features let me know. :)
 * 
 ****
 ****
 * 
 * Some notes about Timers.
 * 
 * http://www.righto.com/2009/07/secrets-of-arduino-pwm.html
 * 
 * - Timer/Counter Control Registers TCCRnA and TCCRnB hold the main control bits for the timer
 * - Output Compare Registers OCRnA and OCRnB set the levels at which outputs A and B will be affected
 * 
 * This code has been made for Arduino UNO
 * 
 * The Arduino uses Timer 0 internally for the millis() and delay() functions, so be warned that
 * changing the frequency of this timer will cause those functions to be erroneous. Using the PWM
 * outputs is safe if you don't change the frequency, though.
 * 
 * Timer output Arduino output  Chip pin    Pin name
 * OC0A	        6	            12          PD6
 * OC0B	        5	            11          PD5
 * OC1A	        9	            15          PB1
 * OC1B	        10	            16          PB2
 * OC2A	        11	            17          PB3
 * OC2B	        3	            5           PD3
 * 
 * */



#include <Arduino.h>
#include <stdbool.h>
// include the library code:
#include <LiquidCrystal.h>

#define CONTROL_PIN_FAN 3 // PWM control signal
#define PIN_POT A5 // PWM Speed setting read (potentiometer)
#define PIN_TACHO 2


unsigned long ledMillis = 0;
unsigned long currMillis = 0;

volatile uint16_t tachoPulses_vol = 0; // Tacho counter
uint16_t tachoPulses_non_vol = 0; // For reducing access to volatile value
unsigned long tachoMillis = 0;

uint8_t ledState = LOW;
uint8_t fanState = HIGH;

uint8_t fanDuty = 255; // timer val for PWM
uint8_t fanDutyDisplay = 0; // 0-100 for display

uint16_t potVal = 0;    // potentiometer value
uint16_t currPotVal = 0; // pot lates val

char lcdRow0Empty [17] = "Nopeus:         ";
char lcdRow1Empty [17] = "RPM:            ";

/*** LCD PINS (4 bit mode)
    Display : Arduino
    DB7 (14): D11
    DB6 (13): D10
    DB5 (12): D9
    DB4 (11): D8
    EN 	 (6): D7 (Enable)
    RS   (4): D6 (Register Select)
***/
#define PIN_LCD_RS 6
#define PIN_LCD_EN 7
#define PIN_LCD_DB7 11
#define PIN_LCD_DB6 10
#define PIN_LCD_DB5 9
#define PIN_LCD_DB4 8

// Initialize the LCD library
//LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_DB4, PIN_LCD_DB5, PIN_LCD_DB6, PIN_LCD_DB7);


// SETUP
void setup() {
    pinMode(CONTROL_PIN_FAN, OUTPUT);
    digitalWrite(CONTROL_PIN_FAN, LOW);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // Set timers
    // https://playground.arduino.cc/Main/TimerPWMCheatsheet/
    TCCR2A = _BV(COM2B1) | _BV(WGM20);
    TCCR2B = _BV(CS21);
    OCR2B = 255;

    /*
    Explanation:
    - Clear OC2B on Compare Match when up-counting. Set OC2B on Compare Match when down-counting
    - PWM - Phase Correct mode
    - clkT2S/8 (From Prescaler) = 16MHz / 8 / 255 / 2 ~ 3922 MHz
    - Compare match = 255
    */

    lcd.begin(16, 2);
    lcd.print(F("START"));

    ledMillis = millis();
    tachoMillis = millis();

    Serial.setTimeout(200);
    Serial.begin(115200);
    delay(100);

    Serial.print(F("Start. TCCR2B: "));
    Serial.println(TCCR2B, BIN);

    // Tacho interrupt reading
    pinMode(PIN_TACHO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_TACHO), ISRincrementTachoPulses, FALLING);

    // Initialize LCD display rows
    lcd.setCursor(0,0);
    lcd.print(lcdRow0Empty); // len 8
}

// LOOP
void loop() {
    currMillis = millis();
    if (currMillis > ledMillis + 300) {
        ledMillis = currMillis;
        toggleLED(LED_BUILTIN);

        // Read potentiometer and act if changed
        if (potVal != *getPotValSmooth(&currPotVal, PIN_POT)) {
            potVal = currPotVal;
            fanDuty = potVal >> 2; // convert 1024 to 255

            unsigned char fanDutyDisplay = map(fanDuty, 0, 255, 100, 0); // show as 0-100

            setFanSpeed(CONTROL_PIN_FAN, &fanDuty);
            printFanSpeed(&fanDutyDisplay);
        }
    }

    currMillis = millis();

    if (currMillis > tachoMillis + 1000) {
        tachoMillis = currMillis;
        printTacho(&tachoPulses_vol);
        tachoPulses_vol = 0; // start counting again
    }
}

uint16_t *getPotValSmooth(uint16_t *potVariable_ptr, const char pot_pin) {
    *potVariable_ptr = analogRead(pot_pin) / 10 * 10; // smooth out a little bit (lose precision)
    return potVariable_ptr;
}

void ISRincrementTachoPulses() {
    tachoPulses_vol++;
}

void printTacho(volatile uint16_t *tachoPulses_vol_ptr) {
    tachoPulses_non_vol = *tachoPulses_vol_ptr * 60;

    Serial.print("Tacho: ");
    Serial.println(tachoPulses_non_vol);

    lcd.setCursor(0,1);
    lcd.print(lcdRow1Empty);
    lcd.setCursor(5,1);
    lcd.print(tachoPulses_non_vol);
}

void printFanSpeed(uint8_t *fanDuty_ptr) {
    //Serial.print("Timer value: ");
    //Serial.print(TCNT2); // timer val
    Serial.print(F("Voltage value: 10bit: "));
    Serial.print(currPotVal);
    Serial.print(F(", 8bit (PWM duty): "));
    Serial.print(fanDuty); // real fan duty number 0-255
    Serial.print(F(", %: "));
    Serial.println(fanDutyDisplay);

    lcd.setCursor(8,0); // after text position
    lcd.print(fanDutyDisplay);
    
    Serial.print(F("Fan duty: "));
    Serial.println(*fanDuty_ptr);
}

void setFanSpeed(uint8_t fanPort, uint8_t *fanDuty_ptr) {
    if (*fanDuty_ptr < 11) {
        *fanDuty_ptr = 0;
    }

    OCR2B = *fanDuty_ptr;
}

void toggleLED(int ledPort) {
    ledState = !ledState;
    digitalWrite(ledPort, ledState);
}