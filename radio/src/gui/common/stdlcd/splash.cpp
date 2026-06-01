/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "edgetx.h"
#include "inactivity_timer.h"

static bool splashStarted = false;

void startSplash()
{
  splashStarted = true;
}

void cancelSplash()
{
  splashStarted = false;
}

void waitSplash()
{
  // Handle B&W splash screen

#if defined(PWR_BUTTON_PRESS)
  bool refresh = false;
#endif

  if (SPLASH_NEEDED() && splashStarted) {
    resetBacklightTimeout();
    drawSplash();

    getADC(); // init ADC array

    inactivityCheckInputs();

    tmr10ms_t tgtime = get_tmr10ms() + SPLASH_TIMEOUT;

#if defined(RADIO_NOVAX_X7)
    // Fast boot leaves the ADC inputs and key queue still settling when the
    // splash starts. The input-activity check below would then read that
    // transient as "user input" and skip the logo on the very first loop
    // iteration (the symptom: animation plays, then straight to the main view
    // with no visible logo). Hold the logo for a minimum time before honouring
    // a skip request, so it is always shown; the user can still skip after.
    tmr10ms_t skipAllowedTime = get_tmr10ms() + 50; // 500 ms
#endif

    while (tgtime > get_tmr10ms()) {
      RTOS_WAIT_TICKS(1);

      getADC();

      bool allowSkip = true;
#if defined(RADIO_NOVAX_X7)
      allowSkip = (get_tmr10ms() >= skipAllowedTime);
#endif
      if (allowSkip && (getEvent() || inactivityCheckInputs()))
        return;

#if defined(PWR_BUTTON_PRESS)
      uint32_t pwr_check = pwrCheck();
      if (pwr_check == e_power_off) {
        break;
      }
      else if (pwr_check == e_power_press) {
        refresh = true;
      }
      else if (pwr_check == e_power_on && refresh) {
        drawSplash();
        refresh = false;
      }
#else
      if (pwrCheck() == e_power_off) {
        return;
      }
#endif

      checkBacklight();
    }
  }
}
