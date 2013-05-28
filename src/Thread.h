#ifndef SDKD_THREAD_H
#define SDKD_THREAD_H

#ifndef SDKD_INTERNAL_H_
#error "Include sdkd_internal.h first"
#endif

namespace CBSdkd {

class Thread
{
public:
#ifdef _WIN32
    typedef unsigned (__stdcall *StartFunc)(void*);
#else
    typedef void *(*StartFunc)(void*);
#endif
    virtual void start(void *arg) = 0;
    virtual void abort() = 0;
    virtual bool isAlive() = 0;
    virtual void join() = 0;
    virtual ~Thread() { }

    static Thread* Create(StartFunc fn);

};

class Mutex
{
public:
    virtual ~Mutex() { }
    virtual void lock() = 0;
    virtual void unlock() = 0;

    static Mutex* Create();

};

class Timer {
public:

    typedef void (*TimerFunc)(void*);
    virtual void schedule(unsigned int seconds) = 0;

    void disable() {
        schedule(0);
    }

    virtual bool isActive() = 0;

    static Timer* Create(TimerFunc fn);
    virtual ~Timer() {}
};

}
#endif
