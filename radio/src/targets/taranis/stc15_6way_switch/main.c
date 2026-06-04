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
 * Pin Map (SOP16) — per novaX-X7 schematic (U17):
 *
 *   Pin  GPIO       Dir   Function
 *   ---  ---------  ----  ----------------------------------------
 *    1   P1.2       OUT   LED1 driver (Mode 1, leftmost; active low, 220R to VCC)
 *    2   P1.3       OUT   LED2 driver (Mode 2)
 *    3   P1.4       OUT   LED3 driver (Mode 3)
 *    4   P1.5       OUT   LED4 driver (Mode 4)
 *    5   P5.4       OUT   LED5 driver (Mode 5) — see RST note below
 *    6   VCC        PWR   3.3V
 *    7   P5.5       OUT   LED6 driver (Mode 6, rightmost)
 *    8   GND        PWR   ground
 *    9   P3.0       IN    RXD / ISP (NC3 pin1) — NOT used at runtime
 *   10   P3.1       IN    SW (Mode 6 button, active low)
 *   11   P3.2       IN    SW (Mode 5 button, active low)
 *   12   P3.3       IN    SW (Mode 4 button)
 *   13   P3.6       IN    SW (Mode 3 button)
 *   14   P3.7       IN    SW (Mode 2 button)
 *   15   P1.0       IN    SW (Mode 1 button)
 *   16   P1.1       OUT   PWM — analog out -> R115+C111 -> R114+C110 -> PA5
 *                          (P1.1 = CCP0-capable; this is the pin the schematic
 *                           routes to PA5, NOT P3.0)
 *
 *   NOTE — LED5 on P5.4: P5.4 is the dedicated RST pin by default.
 *   To drive LED5, the ISP option "reset pin as GPIO" must be set
 *   (stcgal: `--option reset_pin_enabled=0`, or tick the box in
 *   STC-ISP).  Otherwise the pin stays in RST mode and LED5 will
 *   never light; worse, any glitch on the pad would reset the chip.
 *
 * ---------------------------------------------------------------
 * Analog output:
 *
 *   P1.1 generates software PWM at ~1 kHz.
 *   Two-stage RC LPF (R=10K, C=100nF, fc=159 Hz per stage)
 *   smooths it to a DC voltage read by STM32 PA5 (ADC1_IN5).
 *
 *   Positions are spread evenly so each sits centre-bucket with wide margin.
 *   edgeTX compares raw-ADC/32 against DEFAULT_6POS_CALIB (novaX-X7) =
 *   {11, 34, 57, 80, 103} -- the midpoints of the per-position raw/32 values
 *   0/23/46/69/92/115.
 *
 *     Pos  Bucket (V)        Target V   Duty/50    %
 *      0   0.000 - 0.297      0.000       0        0%
 *      1   0.297 - 0.891      0.594       9       18%
 *      2   0.891 - 1.485      1.188      18       36%
 *      3   1.485 - 2.079      1.782      27       54%
 *      4   2.079 - 2.673      2.376      36       72%
 *      5   2.673 - 3.300      2.970      45       90%
 *
 * ---------------------------------------------------------------
 * ISP programming (via NC3 connector):
 *
 *   NC3 pin2=GND, pin4=VCC, pin1/3 = P3.0(RXD)/P3.1(TXD)
 *   Use stcgal:
 *     stcgal -p /dev/ttyUSB0 -t 11059 \
 *            --option reset_pin_enabled=0 main.ihx
 *   Power-cycle target to enter bootloader.
 *
 *   The `reset_pin_enabled=0` option is REQUIRED: it turns P5.4 into
 *   a GPIO so LED5 can be driven.  Without it Mode 5's LED will never
 *   light and any glitch on the pad resets the chip — easily mistaken
 *   for a software bug.  Equivalent in the STC-ISP GUI: tick
 *   "P5.4 used as I/O port" (or untick "reset pin enabled").
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
__sbit __at(0xC8 + 4) P5_4;
__sbit __at(0xC8 + 5) P5_5;

/* ================================================================
 * Pin definitions
 * ================================================================ */

/* Buttons — quasi-bidirectional input, internal weak pull-up, press = GND. */
#define BTN1  P1_0   /* pin 15 — Mode 1 (leftmost) */
#define BTN2  P3_7   /* pin 14 — Mode 2           */
#define BTN3  P3_6   /* pin 13 — Mode 3           */
#define BTN4  P3_3   /* pin 12 — Mode 4           */
#define BTN5  P3_2   /* pin 11 — Mode 5           */
#define BTN6  P3_1   /* pin 10 — Mode 6 (rightmost) */

/* LEDs — active low: cathode -> GPIO (push-pull), anode -> 220R -> VCC.
 * LED5 sits on P5.4 (the default RST pin) — the ISP option
 * `reset_pin_enabled=0` must be set when flashing this firmware,
 * otherwise LED5 never lights and noise on the pad resets the chip. */
#define LED1  P1_2   /* pin 1 — Mode 1 */
#define LED2  P1_3   /* pin 2 — Mode 2 */
#define LED3  P1_4   /* pin 3 — Mode 3 */
#define LED4  P1_5   /* pin 4 — Mode 4 */
#define LED5  P5_4   /* pin 5 — Mode 5 (requires RST pin disabled in ISP) */
#define LED6  P5_5   /* pin 7 — Mode 6 */

/* PWM analog output.
 * The schematic routes the 6-way analog encoding from P1.1 (pin 16, a
 * CCP0-capable pin) through the R115/R114 + C111/C110 two-stage RC filter
 * to STM32 PA5.  P3.0 (pin 9) is the RXD/ISP line on NC3 and carries no
 * runtime signal — driving the PWM there (as earlier revisions did) left
 * P1.1 floating at its weak pull-up, so PA5 sat at ~Vcc and never moved. */
#define PWM_OUT P1_1 /* pin 16 — RC filtered -> PA5 */

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
 *   ripple < 30 mV pp, negligible against the ~594 mV spacing between
 *   evenly-spread positions (see duty_table).
 */
#define PWM_PERIOD   50
#define TH0_RELOAD   35
#define DEBOUNCE_TH  30   /* ~30 ms at 1 ms loop rate */
#define NO_BUTTON    0xFFu

/* PWM duty per position (out of PWM_PERIOD = 50).
 *
 * Positions are spread EVENLY across the range (duty step 9 -> ~0.59 V apart)
 * so each lands dead-centre of its edgeTX 6POS bucket with ~0.3 V of margin to
 * either boundary.  The earlier table crammed positions 0-4 into 0-0.46 V, so
 * adjacent steps were only ~60-130 mV apart — comparable to the PWM ripple and
 * the 1/50 duty resolution — and positions sitting on a bucket edge flickered
 * between two values (the 6P source jittered in the mixer).  Even spacing makes
 * the ripple negligible relative to the margin. */
static const __code unsigned char duty_table[6] = {
     0,   /* pos 0 -> 0.000 V */
     9,   /* pos 1 -> 0.594 V */
    18,   /* pos 2 -> 1.188 V */
    27,   /* pos 3 -> 1.782 V */
    36,   /* pos 4 -> 2.376 V */
    45    /* pos 5 -> 2.970 V */
};

/* ================================================================
 * Globals (kept minimal for 256-byte RAM)
 * ================================================================ */
static volatile unsigned char g_duty         = 0;  /* active PWM duty     */
static volatile unsigned char g_pwm_cnt      = 0;  /* ISR cycle counter   */
static volatile unsigned int  g_ms_tick      = 0;  /* 1 ms tick, wraps OK */

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

    PWM_OUT = (cnt < g_duty) ? 1 : 0;
}

/* ================================================================
 * LED control
 *
 * Physical panel layout (left -> right):
 *   position 1  = code LED1 (P1.2)
 *   position 2  = code LED2 (P1.3)
 *   position 3  = code LED3 (P1.4)
 *   position 4  = code LED4 (P1.5)
 *   position 5  = code LED5 (P5.4)
 *   position 6  = code LED6 (P5.5)
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
    if (!BTN1) return 0;
    if (!BTN2) return 1;
    if (!BTN3) return 2;
    if (!BTN4) return 3;
    if (!BTN5) return 4;
    if (!BTN6) return 5;
    return NO_BUTTON;
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
    /* P1.1 push-pull (PWM analog out), P1[5:2] push-pull (LED1..LED4),
     * P1.0 quasi-bidir (BTN1).  0x3E = P1.1..P1.5. */
    P1M1 &= ~0x3E;
    P1M0 |=  0x3E;

    /* P3.1/P3.2/P3.3/P3.6/P3.7 quasi-bidir (BTN6/5/4/3/2).
     * P3.0 is RXD/ISP (NC3) — leave as default quasi-bidir input, never driven. */
    P3M1 &= ~0xCE;
    P3M0 &= ~0xCE;

    /* P5.4 push-pull (LED5, requires reset_pin_enabled=0);
     * P5.5 push-pull (LED6). */
    P5M1 &= ~0x30;
    P5M0 |=  0x30;

    /* engage internal weak pull-ups on all button inputs */
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
    unsigned char  btn;
    unsigned char  prev_btn   = NO_BUTTON;
    unsigned char  deb_cnt    = 0;
    unsigned char  cur_mode   = 0;

    hw_init();

    /* power-on default: Mode 1.  PWM duty set to 0 (= 0 V) so the
     * STM32 consistently reads Mode 1 during and after boot animation. */
    g_duty = duty_table[0];

    EA = 1;  /* global interrupt enable — must be on before delay_ms() */

    /* Boot scan: 1 -> 2 -> 3 -> 4 -> 5 -> 6, then settle on Mode 1. */
    boot_animation();
    set_leds(0);

    for (;;) {
        btn = scan_buttons();

        if (btn != NO_BUTTON && btn != cur_mode) {
            if (btn == prev_btn) {
                if (++deb_cnt >= DEBOUNCE_TH) {
                    cur_mode = btn;
                    g_duty   = duty_table[btn];
                    set_leds(btn);
                    deb_cnt  = 0;

                    /* Wait for release; bail after ~500 ms so a
                     * stuck contact can't hang the chip. */
                    {
                        unsigned int guard = 0;
                        while (scan_buttons() != NO_BUTTON && guard < 500) {
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
