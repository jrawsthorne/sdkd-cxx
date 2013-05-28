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

struct sigev_wrapper {
    void *data;
    Timer::TimerFunc callback;
};

extern "C" {
static void sigev_wrapper_callback(union sigval siv)
{
    struct sigev_wrapper *sevw = (struct sigev_wrapper*)siv.sival_ptr;
    sevw->callback(sevw->data);
}

}

class PosixTimer : public Timer {
public:
    PosixTimer(TimerFunc fn) {
        this->fn = fn;

        struct sigevent sev;
        int rv;

        memset(&sev, 0, sizeof(sev));

        sevw.callback = fn;
        sevw.data = NULL;

        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = sigev_wrapper_callback;
        sev.sigev_value.sival_ptr = &sevw;

        rv = timer_create(CLOCK_REALTIME, &sev, &tmid);
        assert(rv == 0);
        is_active = false;

    }

    void schedule(unsigned int seconds) {

        /**
         * Initialize the actual time..
         */
        log_noctx_info("Setting timer to %u seconds", seconds);

        struct itimerspec its;
        memset(&its, 0, sizeof(its));
        int rv;

        its.it_value.tv_sec = seconds;
        rv = timer_settime(tmid, 0, &its, NULL);
        assert(rv == 0);
        if (!seconds) {
            is_active = false;
        }

    }

    bool isActive() {
        return is_active;
    }

    virtual ~PosixTimer() {
        timer_delete(tmid);
    }

private:
    TimerFunc fn;
    timer_t tmid;
    bool is_active;
    struct sigev_wrapper sevw;

};

Timer *Timer::Create(TimerFunc fn) {
    return new PosixTimer(fn);
}

}
