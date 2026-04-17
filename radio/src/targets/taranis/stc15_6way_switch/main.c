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
 * Pin Map (SOP16) — bench-verified on Boxer V22 board:
 *
 *   Pin  GPIO       Dir   Function
 *   ---  ---------  ----  ----------------------------------------
 *   15   P1.0       IN    SW (phys pos 1, leftmost) — Mode 1 button
 *   14   P3.7       IN    SW (phys pos 2)           — Mode 2 button
 *   13   P3.6       IN    SW (phys pos 3)           — Mode 3 button
 *   12   P3.3       IN    SW (phys pos 4)           — Mode 4 button
 *    9   P3.0       I/O   SW (phys pos 5) shared with PWM output —
 *                         see "P3.0 dual use" note
 *    7   P5.5       I/O   SW (phys pos 6) shared with LED5 driver  —
 *                         see "P5.5 dual use" note
 *
 *   NOTE — P3.0 dual use: the panel switch at pos 5 grounds the same
 *   pin that drives the PWM analog signal to STM32 PA5.  Detection
 *   is done by suspending the PWM ISR every 250 ms, switching P3.0 to
 *   quasi-bidirectional input with internal weak pull-up, waiting a
 *   few ms for the first RC stage to settle, then sampling.  After
 *   the sample, P3.0 is restored to push-pull and PWM resumes.  STM32
 *   sees a brief voltage glitch every 250 ms which the EMA filter
 *   absorbs.
 *
 *   NOTE — P5.5 dual use: the panel switch at pos 6 grounds the same
 *   pin that drives LED5 (panel pos-5 indicator).  Detection works
 *   the same way: every 250 ms, save the LED5 drive state, flip P5.5
 *   to quasi-bidirectional input with weak pull-up, wait ~2 ms,
 *   sample, then restore push-pull and the original LED state.  When
 *   LED5 is OFF (the common case) the sampler does not blink it; when
 *   LED5 is ON the user sees a tiny ~0.8 % duty drop.
 *
 *   10   P3.1       OUT   LED1  — Mode 6 LED (active low, 220R to VCC)
 *    9   P3.0       OUT   PWM   — analog out -> R115+C111 -> R114+C110 -> PA5
 *    1   P1.2       OUT   LED7  — Mode 1 LED (active low, 220R to VCC)
 *    2   P1.3       OUT   LED6  — Mode 2 LED
 *    3   P1.4       OUT   LED5  — Mode 3 LED
 *    4   P1.5       OUT   LED3  — Mode 4 LED
 *    5   P5.4/RST   —     (unused; keep RST function enabled in ISP)
 *    6   VCC        PWR   3.3V
 *    7   P5.5       I/O   LED2  — Mode 5 LED + pos-6 button (sampled)
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

/* Buttons — quasi-bidirectional, internal pull-up, press = GND.
 *   BTN1..4 are plain GPIO reads.
 *   BTN5 is P3.0, shared with PWM; sample_btn5() fills g_btn5_pressed.
 *   BTN6 is P5.5, shared with LED5; sample_btn6() fills g_btn6_pressed.
 * BTN4 requires the ISP option `reset_pin_enabled=False` so that P5.4
 * acts as a GPIO instead of the dedicated RST pin. */
#define BTN1  P1_0   /* pin 15 — phys pos 1 — Mode 1 */
#define BTN2  P3_7   /* pin 14 — phys pos 2 — Mode 2 */
#define BTN3  P3_6   /* pin 13 — phys pos 3 — Mode 3 */
#define BTN4  P3_3   /* pin 12 — phys pos 4 — Mode 4 (bench-verified) */
/* BTN5: P3.0 — see g_btn5_pressed                 */
/* BTN6: P5.5 — see g_btn6_pressed                 */

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
static volatile unsigned char g_duty         = 0;  /* active PWM duty     */
static volatile unsigned char g_pwm_cnt      = 0;  /* ISR cycle counter   */
static volatile unsigned int  g_ms_tick      = 0;  /* 1 ms tick, wraps OK */
static volatile unsigned char g_btn5_pressed = 0;  /* set by sample_btn5  */
static volatile unsigned char g_btn5_sample  = 0;  /* 1 = ISR halts PWM   */
static volatile unsigned char g_btn6_pressed = 0;  /* set by sample_btn6  */

/* ================================================================
 * Timer 0 ISR — software PWM on P3.0 + 1 ms tick
 *
 * Executes every ~20 us.  PWM_PERIOD (50) ISRs = 1 ms -> bump ms tick.
 * ~40 clocks in 1T mode -> 3.6 us -> 18 % CPU.
 * ================================================================ */
void timer0_isr(void) __interrupt(1)
{
    unsigned char cnt = g_pwm_cnt + 1;

    if (cnt >= PWM_PERIOD) {
        cnt = 0;
        g_ms_tick++;
    }

    g_pwm_cnt = cnt;

    /* While sample_btn5() is reading P3.0, leave the pin alone.
     * g_duty == 0 keeps output permanently low (0 V).             */
    if (!g_btn5_sample) {
        PWM_OUT = (cnt < g_duty) ? 1 : 0;
    }
}

/* ================================================================
 * LED control
 *
 * Physical panel layout (left -> right):
 *   position 1  = schematic LED7 = code LED1 (P1.2)
 *   position 2  = schematic LED6 = code LED2 (P1.3)
 *   position 3  = schematic LED5 = code LED3 (P1.4)
 *   position 4  = schematic LED3 = code LED4 (P1.5)
 *   position 5  = schematic LED2 = code LED5 (P5.5)
 *   position 6  = schematic LED1 = code LED6 (P3.1)
 *
 * If the boot animation scans in a visibly wrong order, swap the
 * LEDn macros at the top of this file rather than the case arms here.
 * ================================================================ */
static void leds_off(void)
{
    LED1 = 1;  LED2 = 1;  LED3 = 1;
    LED4 = 1;  LED5 = 1;  LED6 = 1;
}

static void set_leds(unsigned char mode)
{
    leds_off();

    switch (mode) {
    case 0: LED1 = 0; break;  /* leftmost  */
    case 1: LED2 = 0; break;
    case 2: LED3 = 0; break;
    case 3: LED4 = 0; break;
    case 4: LED5 = 0; break;
    case 5: LED6 = 0; break;  /* rightmost */
    }
}

/* ================================================================
 * Button scanner — returns 0-5 for pressed, 0xFF for none.
 * ================================================================ */
static unsigned char scan_buttons(void)
{
    if (!BTN1)          return 0;
    if (!BTN2)          return 1;
    if (!BTN3)          return 2;
    if (!BTN4)          return 3;   /* P5.4, needs RST pin disabled */
    if (g_btn5_pressed) return 4;   /* sampled by sample_btn5() */
    if (g_btn6_pressed) return 5;   /* sampled by sample_btn6() */
    return NO_BUTTON;
}

static void delay_ms(unsigned int ms);

/* Briefly switch P3.0 to quasi-bidirectional input, engage the weak
 * pull-up, wait for the first RC stage to settle, and read the pin.
 * Pin reads LOW only if an external switch (panel pos 5) is grounding
 * it.  Causes a short PWM glitch that the EMA filter on the STM32 side
 * will absorb.  Called from the main loop every ~250 ms. */
static void sample_btn5(void)
{
    g_btn5_sample = 1;          /* ISR stops touching P3.0      */
    P3M0 &= ~0x01;              /* P3.0 -> quasi-bidir          */
    PWM_OUT = 1;                /* engage internal pull-up      */
    delay_ms(2);                /* settle through 10K + 100nF   */
    g_btn5_pressed = !PWM_OUT;
    P3M0 |= 0x01;               /* P3.0 -> push-pull again      */
    g_btn5_sample = 0;          /* ISR resumes PWM              */
}

/* Same trick for P5.5 / pos 6.  P5.5 drives LED5; we save the current
 * LED5 drive level, flip P5.5 to quasi-bidir with pull-up engaged,
 * sample, then restore push-pull and the original LED level.  The ISR
 * does not touch P5.5, so no sample flag is needed. */
static void sample_btn6(void)
{
    unsigned char led5_save = LED5;
    P5M0 &= ~0x20;              /* P5.5 -> quasi-bidir          */
    P5_5  = 1;                  /* engage internal pull-up      */
    delay_ms(2);                /* settle through 10K + 100nF   */
    g_btn6_pressed = !P5_5;
    P5M0 |= 0x20;               /* P5.5 -> push-pull            */
    LED5   = led5_save;         /* restore LED5 drive           */
}

/* ================================================================
 * Millisecond delay based on Timer 0 ISR tick.
 *
 * Requires EA=1 and Timer 0 running.  Calling this before hw_init()
 * starts the timer will block forever — caller must enable interrupts
 * first (main() does so).
 * ================================================================ */
static void delay_ms(unsigned int ms)
{
    unsigned int start = g_ms_tick;
    while ((unsigned int)(g_ms_tick - start) < ms)
        ;
}

/* ================================================================
 * Boot animation: scan LED position 1 -> 6 once (left to right),
 * then all off briefly.  g_duty stays at 0 throughout, so PA5 still
 * reads Mode 1 while the animation plays.
 * ================================================================ */
static void boot_animation(void)
{
    unsigned char i;

    for (i = 0; i < 6; i++) {
        set_leds(i);
        delay_ms(120);
    }

    leds_off();
    delay_ms(150);
}

/* ================================================================
 * Hardware initialisation
 * ================================================================ */
static void hw_init(void)
{
    /* P1 [5:2] push-pull (LEDs), [1:0] quasi-bidir (buttons) */
    P1M1 &= ~0x3C;
    P1M0 |=  0x3C;

    /* P3 [1:0] push-pull (LED6 + PWM), [7:6] quasi-bidir (buttons) */
    P3M1 &= ~0x03;
    P3M0 |=  0x03;

    /* P5.5 push-pull (LED5); P5.4 unused, keep at reset default (input). */
    P5M1 &= ~0x20;
    P5M0 |=  0x20;

    /* activate internal pull-ups on button pins.  P3.0 stays push-pull
     * for PWM (sample_btn5() flips it to quasi-bidir on demand); P5.5
     * stays push-pull for LED5 (sample_btn6() flips it similarly). */
    BTN1 = 1;  BTN2 = 1;  BTN3 = 1;  BTN4 = 1;

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
    unsigned char  btn;
    unsigned char  prev_btn   = NO_BUTTON;
    unsigned char  deb_cnt    = 0;
    unsigned char  cur_mode     = 0;
    unsigned int   sample_ctr   = 0;
    unsigned char  sample_phase = 0;

    hw_init();

    /* power-on default: Mode 1.  PWM duty set to 0 (= 0 V) so the
     * STM32 consistently reads Mode 1 during and after boot animation. */
    g_duty = duty_table[0];

    EA = 1;  /* global interrupt enable — must be on before delay_ms() */

    /* Boot scan: 1 -> 2 -> 3 -> 4 -> 5 -> 6, then settle on Mode 1. */
    boot_animation();
    set_leds(0);

    for (;;) {
        /* Sample shared pins: BTN5 (P3.0) and BTN6 (P5.5) alternately,
         * 125 ms apart, so each is seen every 250 ms but the PWM glitch
         * and the LED5 blink never happen in the same loop iteration.  */
        if (++sample_ctr >= 125) {
            sample_ctr = 0;
            if (sample_phase) {
                sample_btn5();
            } else {
                sample_btn6();
            }
            sample_phase ^= 1;
        }

        btn = scan_buttons();

        if (btn != NO_BUTTON && btn != cur_mode) {
            if (btn == prev_btn) {
                if (++deb_cnt >= DEBOUNCE_TH) {
                    cur_mode = btn;
                    g_duty   = duty_table[btn];
                    set_leds(btn);
                    deb_cnt  = 0;

                    /* Wait for release.  Keep the shared-pin samplers
                     * running, otherwise g_btn5_pressed / g_btn6_pressed
                     * freeze at 1 and we'd never see pos 5 / pos 6 let
                     * go.  Also bail after ~500 ms so a stuck sensor
                     * can't hang the chip. */
                    {
                        unsigned int rel_ctr = 0;
                        unsigned int guard   = 0;
                        while (scan_buttons() != NO_BUTTON && guard < 500) {
                            if (++rel_ctr >= 125) {
                                rel_ctr = 0;
                                if (sample_phase) sample_btn5();
                                else              sample_btn6();
                                sample_phase ^= 1;
                            }
                            delay_ms(1);
                            guard++;
                        }
                    }
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
