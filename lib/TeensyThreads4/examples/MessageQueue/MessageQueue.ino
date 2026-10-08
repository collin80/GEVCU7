/*
 * Pass messages between threads (and from an interrupt) with Threads::Queue.
 *
 * Two producer threads and a timer interrupt send messages to one inbox.
 * The consumer thread blocks in receive(), using no CPU while the inbox is
 * empty, and prints each message as it arrives. Each producer also has its
 * own reply queue, which the consumer uses to acknowledge its messages.
 *
 * Note: Serial.printf() needs well over 1 KB of stack, more than the default
 * thread stack size, so the threads here are given larger stacks.
 */
#include <TeensyThreads.h>

const int STACK_SIZE = 4096;

typedef Threads::Queue<uint32_t, 2> ReplyQueue;

struct Message {
  int sender;           // thread id, or -1 for the interrupt
  uint32_t seq;
  uint32_t time_ms;
  ReplyQueue *reply;    // where to send the acknowledgement, or NULL
};

struct Producer {
  int period_ms;
  ReplyQueue acks;
  Producer(int period_ms) : period_ms(period_ms) {}
};

Threads::Queue<Message, 8> inbox;
Producer producers[2] = { {250}, {400} };
IntervalTimer timer;

void producer(void *arg) {
  Producer *p = (Producer *)arg;
  uint32_t seq = 0;
  while (1) {
    Message m = { threads.id(), seq, millis(), &p->acks };
    inbox.send(m);                    // waits while the inbox is full
    uint32_t ack;
    if (!p->acks.receive(ack, 500))   // wait up to 500 ms for an acknowledgement
      Serial.printf("thread %d: no ack for seq %lu\n", threads.id(), seq);
    else if (ack != seq)
      Serial.printf("thread %d: wrong ack %lu for seq %lu\n", threads.id(), ack, seq);
    seq++;
    threads.delay(p->period_ms);
  }
}

void consumer() {
  Message m;
  while (1) {
    inbox.receive(m);                 // BLOCKED until a message arrives
    Serial.printf("from %2d  seq %4lu  at %6lu ms  (%u queued)\n",
                  m.sender, m.seq, m.time_ms, inbox.count());
    if (m.reply) m.reply->trySend(m.seq);
  }
}

void timer_isr() {
  static uint32_t seq = 0;
  Message m = { -1, seq++, millis(), NULL };
  inbox.trySend(m);                   // never waits; drops the message if full
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  threads.addThread(consumer, 0, STACK_SIZE);
  for (Producer &p : producers) threads.addThread(producer, &p, STACK_SIZE);
  timer.begin(timer_isr, 1000000);    // once per second
}

void loop() {
  threads.delay(5000);
  Serial.print(threads.threadsInfo());
  int id;
  if (threads.testStackMarkers(&id)) Serial.printf("stack overflow in thread %d\n", id);
}
