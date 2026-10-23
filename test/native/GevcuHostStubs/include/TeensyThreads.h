/*
 * Host stand-in for TeensyThreads. There is no real scheduler on the host:
 * threads.delay() advances virtual time and Mutex records lock nesting so a
 * test can detect a non-recursive mutex being re-entered (which would
 * deadlock on the real hardware).
 */
#pragma once

#include <Arduino.h>

typedef void (*ThreadFunction)(void *);

class Threads
{
public:
    class Mutex
    {
    public:
        int lock(unsigned int timeout_ms = 0);
        int try_lock();
        int unlock();
        int getState() const { return depth; }
        int depth = 0;
    };

    class Scope
    {
    public:
        explicit Scope(Mutex &m) : r(m) { r.lock(); }
        ~Scope() { r.unlock(); }
    private:
        Mutex &r;
    };

    void delay(int ms);
    void yield();
    int addThread(ThreadFunction, void *arg = 0, int stack_size = -1, void *stack = 0) { (void)arg; (void)stack_size; (void)stack; return -1; }
    int id() { return 0; }
};

extern Threads threads;
