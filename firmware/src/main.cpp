// 2023-05-19 k24: The below code was modified to be used with a custom controller and Audi A8 D2 FL steering wheel.

/* pilot1.c
 * Ford SWC to Kenwood radio (NEC protocol) adapter
 * Author: Michal Babik <michalb1981@o2.pl>
 */

#include <Arduino.h>
#include <SoftPWM.h>

SOFTPWM_DEFINE_CHANNEL(0, DDRB, PORTB, PORTB0);
SOFTPWM_DEFINE_OBJECT_WITH_PWM_LEVELS(1, 10);
//SOFTPWM_DEFINE_EXTERN_OBJECT_WITH_PWM_LEVELS(1, 10);

#define sbi(x,y) x |= _BV(y) //set bit - using bitwise OR operator
#define cbi(x,y) x &= ~(_BV(y)) //clear bit - using bitwise AND operator
#define tbi(x,y) x ^= _BV(y) //toggle bit - using bitwise XOR operator
#define is_high(x,y) (x & _BV(y) == _BV(y)) //check if the y'th bit of

#define NEC_TIME 562.5f // base time in us
#define NEC_LINE_HI 1   // pin value to set line high
#define NEC_LINE_LO 0   // pin value to set line low
#define NEC_PORT PORTD  // kenwood radio remote pin port
#define NEC_PIN PD0     // kenwood radio remote pin
#define NEC_DDR DDRD    // kenwood radio remote direction
#define BUTTONS_PIN PIND //buttons pin register

#define BTN_VOLP 0b00001000
#define BTN_VOLM 0b00100000
#define BTN_DN   0b00010000
#define BTN_UP   0b10000000
#define BTN_LH   0b00000100
#define BTN_RH   0b01000000

//#define BTN_VOLM_DN 0b00110000
//#define BTN_RH_UP   0b11000000
//#define BTN_RH_DN   0b01010000

#define CODE_VOLP 0x14
#define CODE_VOLM 0x15
#define CODE_MUTE 0x16
#define CODE_NEXT 0x0b
#define CODE_PREV 0x0a
#define CODE_FF   0x0d
#define CODE_REV  0x0c
#define CODE_SRC  0x13

void nec_set_pin (uint8_t hilo);
void nec_base (uint8_t hm, uint8_t lm);
void nec_start (void);
void nec_finish (void);
void nec_one (void);
void nec_zero (void);
void nec_8bit (uint8_t *bits);
void nec_data (uint8_t addr, uint8_t dta);
void nec_send (uint8_t data);
void beep();

uint8_t ken_code     = 0xb9;
//uint8_t count = 0;
bool locked = false;

ISR(PCINT2_vect) {
    cli();
    if ((BUTTONS_PIN & 0b11111100) == 0b11111100) {
        locked = false;
    }
    if (!(BUTTONS_PIN & BTN_VOLP)) {
        nec_send(CODE_VOLP);
    }
    if (!(BUTTONS_PIN & BTN_VOLM)) {
        if (!(BUTTONS_PIN & BTN_DN)) {
            if (locked == false) {
                nec_send(CODE_MUTE);
                locked = true;
            }
        } else {
            if (locked == false) {
                nec_send(CODE_VOLM);
                //locked = true;
            }
        }
    }

    if (!(BUTTONS_PIN & BTN_RH)) {
        if (!(BUTTONS_PIN & BTN_UP)) {
            if (locked == false) {
                nec_send(CODE_FF);
                //locked = true;
            }
        }
        if (!(BUTTONS_PIN & BTN_DN)) {
            if (locked == false) {
                nec_send(CODE_REV);
                //locked = true;
            }
        }
    }

    if (!(BUTTONS_PIN & BTN_UP)) {
        if ((BUTTONS_PIN & BTN_RH) != 0) {
            if (locked == false) {
                nec_send(CODE_NEXT);
                locked = true;
            }
        }
    }
    if (!(BUTTONS_PIN & BTN_DN)) {
        if ((BUTTONS_PIN & BTN_RH) != 0) {
            if ((BUTTONS_PIN & BTN_VOLM) != 0) {
                if (locked == false) {
                    nec_send(CODE_PREV);
                    locked = true;
                }
            }
        }
    }

    if (!(BUTTONS_PIN & BTN_LH)) {
        nec_send(CODE_SRC);
    }
    sei();
}

void setup() {
  // put your setup code here, to run once:
  //pinMode(8, INPUT); // bodge wire
  //pinMode(9, OUTPUT);//buzzer
  //TCCR1B = TCCR1B & B11111000 | B00000010; //pwm timer 1 freq = 4kHz
  //Serial.begin(115200);
  //Serial conflicts with _delay_us?
  PCICR |= 0x04; //enable pcint bank 2
  PCMSK2 |= 0xfc; //enable pcint 23-18

  DDRD = 0x03; //out 0 i 1
  PORTD = 0xFF;
  cbi (NEC_PORT, NEC_PIN); // set nec pin low

  Palatis::SoftPWM.begin(1000);

  //nec_send(0x0c);
  beep();

  //delay(1000);
  //nec_send(0x14);

  
}

void loop() {
  //delay(100);
}

void nec_set_pin (uint8_t hilo)
{
    if (hilo) sbi (NEC_PORT, NEC_PIN); // set high
    else      cbi (NEC_PORT, NEC_PIN); // set low
}
// ----------------------------------------------------------------------------
void nec_base (uint8_t hm, uint8_t lm)
{
    nec_set_pin (NEC_LINE_HI);
    for (uint8_t i=0; i<hm; ++i) 
        _delay_us (NEC_TIME);
    nec_set_pin (NEC_LINE_LO);
    for (uint8_t i=0; i<lm; ++i) 
        _delay_us (NEC_TIME);
}
// ----------------------------------------------------------------------------
void nec_start (void)
{
    nec_base (16, 8); // 9ms - 1, 4.5ms - 0
}
// ----------------------------------------------------------------------------
void nec_finish (void)
{
    nec_base (1, 0); // 562.5us - 1, 0 - 0
}
// ----------------------------------------------------------------------------
void nec_one (void)
{
    nec_base (1, 3); // 562.5us - 1, 1.6875us - 0
}
// ----------------------------------------------------------------------------
void nec_zero (void)
{
    nec_base (1, 1); // 562.5us - 1, 562.5us - 0
}
// ----------------------------------------------------------------------------
void nec_8bit (uint8_t *bits)
{
    for (uint8_t i=0; i<8; ++i) {
        if (*bits & 0x01)
            nec_one ();
        else
            nec_zero ();
        *bits = *bits >> 1;
    }
}
// ----------------------------------------------------------------------------
void nec_data (uint8_t addr, uint8_t dta)
{
    uint8_t naddr = ~addr;
    uint8_t ndta  = ~dta;
    nec_8bit (&addr);
    nec_8bit (&naddr);
    nec_8bit (&dta);
    nec_8bit (&ndta);
}

void nec_send (uint8_t data) {
  //cbi (NEC_PORT, NEC_PIN); // set nec pin low
  nec_start();
  nec_data(ken_code, data);
  nec_finish();
  //cbi (NEC_PORT, NEC_PIN); // set nec pin low
}

void beep() {
  Palatis::SoftPWM.set(0, 5);
  delay(50);
  Palatis::SoftPWM.set(0, 0);
}