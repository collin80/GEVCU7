#include "Watchdog_t4.h"
WDT_T4<WDT3> wdt;

void myCallback() {
  Serial.println("YOU DIDNT FEED THE DOG, 255 CYCLES TILL RESET...");
}

void setup() {
  Serial.begin(1);
  delay(600);
  Serial.println("Begin......");

  WDT_timings_t config;
  config.window = 3000; /* in seconds, 32ms to 522.232s, must be smaller than timeout */
  config.timeout = 10000; /* in seconds, 32ms to 522.232s */
  config.callback = myCallback;
  wdt.begin(config);
}

void loop() {
  static uint32_t feed = millis();

  /* set to below 3000 to demonstrate windowMode effect for feeding the dog too fast */
  /* set to 3100 to demonstrate proper processing */
  /* set to 12000 to demonstrate an actual non-feeding reset */

  if ( millis() - feed > 12000 ) {
    feed = millis();
    wdt.feed();
  }
}
