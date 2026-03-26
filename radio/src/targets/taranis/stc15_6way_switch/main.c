/*
 * STC15W408AS Firmware — 6-Way Mode Switch Controller
 * novaX-X7 Remote Control (U17)
 *
 * Toolchain: SDCC (Small Device C Compiler)
 * Target:    STC15W408AS-35I-SOP16
 * Clock:     11.0592 MHz internal IRC
 * VCC:       3.3V (ME6209A33M3G LDO from +5V)
 *
 * ---------------------------------------------------------------
 * Pin Map (SOP16):
 *
 *   Pin  GPIO       Dir   Function
 *   ---  ---------  ----  ----------------------------------------
 *   16   P1.1       IN    SW17  — Mode 1 button (active low)
 *   15   P1.0       IN    SW15  — Mode 2 button
 *   14   P3.7       IN    SW12  — Mode 3 button
 *   13   P3.6       IN    SW10  — Mode 4 button
 *   12   P3.3       IN    SW7   — Mode 5 button
 *   11   P3.2       IN    SW5   — Mode 6 button
 *   10   P3.1       OUT   LED1  — Mode 6 LED (active low, 220R to VCC)
 *    9   P3.0       OUT   PWM   — analog out -> R115+C111 -> R114+C110 -> PA5
 *    1   P1.2       OUT   LED7  — Mode 1 LED (active low, 220R to VCC)
 *    2   P1.3       OUT   LED6  — Mode 2 LED
 *    3   P1.4       OUT   LED5  — Mode 3 LED
 *    4   P1.5       OUT   LED3  — Mode 4 LED
 *    5   P5.4/RST   —     VCC   (reset held high)
 *    6   VCC        PWR   3.3V
 *    7   P5.5       OUT   LED2  — Mode 5 LED
 *    8   GND        PWR   ground
 *
 * ---------------------------------------------------------------
 * Analog output:
 *
 *   P3.0 generates software PWM at ~1 kHz.
 *   Two-stage RC LPF (R=10K, C=100nF, fc=159 Hz per stage)
 *   smooths it to a DC voltage read by STM32 PA5 (ADC1_IN5).
 *
 *   edgeTX DEFAULT_6POS_CALIB = {5, 13, 22, 31, 40}
 *
 *     Pos  Zone (V)          Target V   Duty/50   %
 *      0   0.000 - 0.064      0.000       0       0%
 *      1   0.065 - 0.167      0.132       2       4%
 *      2   0.168 - 0.283      0.198       3       6%
 *      3   0.284 - 0.399      0.330       5      10%
 *      4   0.400 - 0.515      0.462       7      14%
 *      5   0.516 - 3.300      1.650      25      50%
 *
 * ---------------------------------------------------------------
 * ISP programming (via NC3 connector):
 *
 *   NC3 pin2=GND, pin4=VCC, pin1/3 = P3.0(RXD)/P3.1(TXD)
 *   Use stcgal: stcgal -p /dev/ttyUSB0 -t 11059 main.ihx
 *   Power-cycle target to enter bootloader.
 * ---------------------------------------------------------------
 */

#include <8051.h>

/* ================================================================
 * STC15-specific SFRs (not in standard 8051 header)
 * ================================================================ */
__sfr __at(0x8E) AUXR;
__sfr __at(0xC8) P5;
__sfr __at(0xC9) P5M1;
__sfr __at(0xCA) P5M0;
__sfr __at(0x91) P1M1;
__sfr __at(0x92) P1M0;
__sfr __at(0xB1) P3M1;
__sfr __at(0xB2) P3M0;

/* P5 is at 0xC8 — bit-addressable */
__sbit __at(0xC8 + 5) P5_5;

/* ================================================================
 * Pin definitions
 * ================================================================ */

/* Buttons — quasi-bidirectional, internal pull-up, press = GND */
#define BTN1  P1_1   /* pin 16 — SW17 — Mode 1 */
#define BTN2  P1_0   /* pin 15 — SW15 — Mode 2 */
#define BTN3  P3_7   /* pin 14 — SW12 — Mode 3 */
#define BTN4  P3_6   /* pin 13 — SW10 — Mode 4 */
#define BTN5  P3_3   /* pin 12 — SW7  — Mode 5 */
#define BTN6  P3_2   /* pin 11 — SW5  — Mode 6 */

/* LEDs — active low: cathode -> GPIO, anode -> 220R -> VCC */
#define LED1  P1_2   /* pin 1  — LED7 — Mode 1 */
#define LED2  P1_3   /* pin 2  — LED6 — Mode 2 */
#define LED3  P1_4   /* pin 3  — LED5 — Mode 3 */
#define LED4  P1_5   /* pin 4  — LED3 — Mode 4 */
#define LED5  P5_5   /* pin 7  — LED2 — Mode 5 */
#define LED6  P3_1   /* pin 10 — LED1 — Mode 6 */

/* PWM analog output */
#define PWM_OUT P3_0 /* pin 9  — RC filtered -> PA5 */

/* ================================================================
 * Constants
 * ================================================================ */

/*
 * Timer 0 in 1T mode @ 11.0592 MHz, Mode 2 (8-bit auto-reload).
 *
 * Reload 35 -> counts 221 clocks per overflow -> 20.0 us
 * PWM period 50 steps -> 50 x 20 us = 1.0 ms -> 1 kHz
 *
 * At 1 kHz through 2-stage RC (fc 159 Hz):
 *   ripple < 30 mV pp, well within ~65 mV minimum zone width.
 */
#define PWM_PERIOD   50
#define TH0_RELOAD   35
#define DEBOUNCE_TH  30   /* ~30 ms at 1 ms loop rate */
#define NO_BUTTON    0xFFu

/* PWM duty per position (out of PWM_PERIOD = 50) */
static const __code unsigned char duty_table[6] = {
    0,   /* pos 0 -> 0.000 V */
    2,   /* pos 1 -> 0.132 V */
    3,   /* pos 2 -> 0.198 V */
    5,   /* pos 3 -> 0.330 V */
    7,   /* pos 4 -> 0.462 V */
    25   /* pos 5 -> 1.650 V */
};

/* ================================================================
 * Globals (kept minimal for 256-byte RAM)
 * ================================================================ */
static volatile unsigned char g_duty    = 0;  /* active PWM duty   */
static volatile unsigned char g_pwm_cnt = 0;  /* ISR cycle counter */

/* ================================================================
 * Timer 0 ISR — software PWM on P3.0
 *
 * Executes every ~20 us.  ~40 clocks in 1T mode -> 3.6 us -> 18% CPU.
 * ================================================================ */
void timer0_isr(void) __interrupt(1)
{
    unsigned char cnt = g_pwm_cnt + 1;

    if (cnt >= PWM_PERIOD)
        cnt = 0;

    g_pwm_cnt = cnt;

    /* g_duty == 0 keeps output permanently low (0 V) */
    PWM_OUT = (cnt < g_duty) ? 1 : 0;
}

/* ================================================================
 * LED control — only the selected mode's LED is lit
 * ================================================================ */
static void set_leds(unsigned char mode)
{
    LED1 = 1;  LED2 = 1;  LED3 = 1;
    LED4 = 1;  LED5 = 1;  LED6 = 1;

    switch (mode) {
    case 0: LED1 = 0; break;
    case 1: LED2 = 0; break;
    case 2: LED3 = 0; break;
    case 3: LED4 = 0; break;
    case 4: LED5 = 0; break;
    case 5: LED6 = 0; break;
    }
}

/* ================================================================
 * Button scanner — returns 0-5 for pressed, 0xFF for none.
 * ================================================================ */
static unsigned char scan_buttons(void)
{
    if (!BTN1) return 0;
    if (!BTN2) return 1;
    if (!BTN3) return 2;
    if (!BTN4) return 3;
    if (!BTN5) return 4;
    if (!BTN6) return 5;
    return NO_BUTTON;
}

/* ================================================================
 * Rough millisecond delay (11.0592 MHz, 1T mode)
 * ================================================================ */
static void delay_ms(unsigned int ms)
{
    unsigned int i;
    volatile unsigned char j;

    for (i = 0; i < ms; i++)
        for (j = 230; j; j--)
            ;
}

/* ================================================================
 * Hardware initialisation
 * ================================================================ */
static void hw_init(void)
{
    /* P1 [5:2] push-pull (LEDs), [1:0] quasi-bidir (buttons) */
    P1M1 &= ~0x3C;
    P1M0 |=  0x3C;

    /* P3 [1:0] push-pull (LED6 + PWM), [7:6,3:2] quasi-bidir (buttons) */
    P3M1 &= ~0x03;
    P3M0 |=  0x03;

    /* P5.5 push-pull (LED5) */
    P5M1 &= ~0x20;
    P5M0 |=  0x20;

    /* activate internal pull-ups on button pins */
    BTN1 = 1;  BTN2 = 1;  BTN3 = 1;
    BTN4 = 1;  BTN5 = 1;  BTN6 = 1;

    /* all LEDs off, PWM low */
    LED1 = 1;  LED2 = 1;  LED3 = 1;
    LED4 = 1;  LED5 = 1;  LED6 = 1;
    PWM_OUT = 0;

    /* Timer 0: 1T mode, Mode 2 (8-bit auto-reload) */
    AUXR |= 0x80;                     /* T0x12 = 1 -> 1T (no /12)    */
    TMOD  = (TMOD & 0xF0) | 0x02;     /* Mode 2: 8-bit auto-reload   */
    TH0   = TH0_RELOAD;
    TL0   = TH0_RELOAD;
    ET0   = 1;                         /* enable Timer 0 interrupt    */
    TR0   = 1;                         /* start Timer 0               */
}

/* ================================================================
 * Main
 * ================================================================ */
void main(void)
{
    unsigned char btn;
    unsigned char prev_btn  = NO_BUTTON;
    unsigned char deb_cnt   = 0;
    unsigned char cur_mode  = 0;

    hw_init();

    /* power-on default: Mode 1 */
    g_duty = duty_table[0];
    set_leds(0);

    EA = 1;  /* global interrupt enable */

    for (;;) {
        btn = scan_buttons();

        if (btn != NO_BUTTON && btn != cur_mode) {
            if (btn == prev_btn) {
                if (++deb_cnt >= DEBOUNCE_TH) {
                    cur_mode = btn;
                    g_duty   = duty_table[btn];
                    set_leds(btn);
                    deb_cnt  = 0;

                    while (scan_buttons() != NO_BUTTON)
                        ;
                    delay_ms(20);
                }
            } else {
                prev_btn = btn;
                deb_cnt  = 1;
            }
        } else {
            prev_btn = NO_BUTTON;
            deb_cnt  = 0;
        }

        delay_ms(1);
    }
}
