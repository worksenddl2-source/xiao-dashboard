// State shared between the main loop (touch, Supabase sync) and the LAN web server task
#pragma once

volatile bool ledOn = false;
volatile bool touched = false;
volatile uint32_t touchValue = 0;
volatile uint32_t baseline = 0;

// LED set from the LAN page, waiting to be written to Supabase (-1 = nothing pending)
volatile int pendingLed = -1;

inline void applyLed() {
  digitalWrite(LED_BUILTIN, ledOn ? LOW : HIGH);  // active-low
}
