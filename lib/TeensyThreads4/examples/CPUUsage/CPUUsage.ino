/*
 * Show how much CPU time each thread uses.
 *
 * Thread 1 spins continuously, thread 2 does a little work and then
 * yields its remaining time slice, and thread 0 (loop) prints the usage
 * once per second.
 */
#include <TeensyThreads.h>

volatile uint32_t busy_count = 0;
volatile uint32_t light_count = 0;

void busy_thread() {
  while (1) busy_count++;
}

void light_thread() {
  while (1) {
    for (int i = 0; i < 1000; i++) light_count++;
    threads.yield();
  }
}

int busy_id, light_id;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  busy_id = threads.addThread(busy_thread);
  light_id = threads.addThread(light_thread);
  threads.setCPUUsageWindow(1000); // measure over 1 second (the default)
}

void loop() {
  Serial.printf("main: %5.1f%%  busy: %5.1f%%  light: %5.1f%%\n",
                threads.getCPUUsage(0),
                threads.getCPUUsage(busy_id),
                threads.getCPUUsage(light_id));
  threads.delay(1000);
}
