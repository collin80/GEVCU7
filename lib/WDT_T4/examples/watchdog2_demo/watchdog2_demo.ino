#include "Watchdog_t4.h"
WDT_T4<WDT2> wdt;

void myCallback() {
  Serial.println("FEED THE DOG SOON, OR RESET!");
}

void setup() {
  Serial.begin(1);
  delay(600);
  Serial.println("Begin......");

  WDT_timings_t config;
  config.trigger = 5; /* in seconds, 0->128 */
  config.timeout = 10; /* in seconds, 0->128 */
  config.pin = 13; /* lucky for us, the led shows us how the GPIO works, a simple pulse after timeout */
  config.callback = myCallback;
  wdt.begin(config);
}

void loop() {
  static uint32_t feed = millis();
  if ( millis() - feed > 11000 ) {
    feed = millis();
    wdt.feed(); /* feed the dog every 11 seconds, to exceed 10second timeout period to refresh callback and gpio state for repeat */
  }
}
