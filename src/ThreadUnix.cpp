#include "sdkd_internal.h"

namespace CBSdkd {

class PosixThread : public Thread
{

public:
    PosixThread(StartFunc fn) : fn(fn) {
        assert(fn);
    }

    void abort() {
        pthread_cancel(thr);
    }

    void start(void *arg) {
        assert(fn);
        pthread_create(&thr, NULL, fn, arg);
    }

    bool isAlive() {
        return pthread_kill(thr, 0) == 0;
    }

    void join() {
        pthread_join(thr, NULL);
    }


    virtual ~PosixThread() { }

private:
    pthread_t thr;
    StartFunc fn;
};


Thread *Thread::Create(StartFunc fn)
{
    return new PosixThread(fn);
}

class PosixMutex : public Mutex {
public:
    PosixMutex() {
        int rv;
        rv = pthread_mutex_init(&mutex, NULL);
        assert(rv == 0);
    }
    void lock() {
        pthread_mutex_lock(&mutex);
    }
    void unlock() {
        pthread_mutex_unlock(&mutex);
    }
    virtual ~PosixMutex() {
        pthread_mutex_destroy(&mutex);
    }
private:
    pthread_mutex_t mutex;
};

Mutex *Mutex::Create() {
    return new PosixMutex();
}


static Timer::TimerFunc alarm_handler = NULL;
static void *timer_arg = NULL;
static void alarm_handler_wrap(int signo) {
    (void)signo;
    alarm_handler(timer_arg);
}

class PosixTimer : public Timer {
public:
    PosixTimer(TimerFunc fn) {
        this->fn = fn;
        is_active = false;
        signal(SIGALRM, alarm_handler_wrap);
        alarm_handler = fn;
    }

    void schedule(unsigned int seconds) {
        if (!seconds) {
            is_active = false;
        }
        alarm(seconds);
    }

    bool isActive() {
        return is_active;
    }

    virtual ~PosixTimer() { alarm(0); }

private:
    TimerFunc fn;
    bool is_active;
};

Timer *Timer::Create(TimerFunc fn) {
    return new PosixTimer(fn);
}

}
