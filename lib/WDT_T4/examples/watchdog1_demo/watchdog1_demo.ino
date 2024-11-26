#include "Watchdog_t4.h"
WDT_T4<WDT1> wdt;

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
  config.callback = myCallback;
  wdt.begin(config);
  pinMode(13, OUTPUT);
}

void loop() {
  static uint32_t blinkled = millis();
  if ( millis() - blinkled > 50 ) {
    blinkled = millis();
    digitalWriteFast(13, !digitalReadFast(13));
  }

  static uint32_t callback_test = millis();
  if ( millis() - callback_test > 5500 ) {
    callback_test = millis();
    // wdt.feed(); /* uncomment to feed the watchdog */
  }
}
