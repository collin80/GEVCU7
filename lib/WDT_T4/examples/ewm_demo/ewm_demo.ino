#include "Watchdog_t4.h"
WDT_T4<EWM> ewm;

void myCallback() {
  digitalWrite(13, !digitalRead(13));
  ewm.reset();
}

void setup() {
  Serial.begin(1);
  delay(600);
  Serial.println("Begin......");
  pinMode(13, OUTPUT);

  WDT_timings_t configewm;
  configewm.callback = myCallback;
  configewm.window = 100; /* window mode is disabled when ommitted */
  configewm.timeout = 2000;
  configewm.pin = 21;
  ewm.begin(configewm);

  if ( 1 ) { /* feed the dog early */
    delay(100);
    ewm.feed();
    /* window mode test */
    delay(100); /* <-- not keeping this here would cause resets by myCallback */
    /* this is because right after you fed it, the loop would immediately have fed it within the window time */
  }

}

void loop() {
  Serial.println(millis());
  ewm.feed();
  delay(1900); /* <-- change this value higher to trigger late feeding */
}
