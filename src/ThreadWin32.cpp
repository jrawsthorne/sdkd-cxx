#include "sdkd_internal.h"
#include <process.h>

namespace CBSdkd {

class WindowsThread : public Thread {
public:
    WindowsThread(StartFunc fn) {
        this->fn = fn;
    }

    void start(void *arg) {
        hThread = (HANDLE)_beginthreadex(NULL, 0, fn, arg, 0, NULL);
    }

    void abort() {
        printf("Terminating Thread %p..\n", this);
        TerminateThread(hThread, NULL);
    }
    
    bool isAlive() {
        DWORD result = WaitForSingleObject(hThread, 0);
        if (result == WAIT_OBJECT_0) {
            return false;
        }
        return true;
    }

    void join() {
        DWORD result = WaitForSingleObject(hThread, INFINITE);
        if (result != WAIT_OBJECT_0) {
            printf("WaitForSingleObject returned something strange..\n");
        }
    }

    virtual ~WindowsThread() {
        CloseHandle(hThread);
        hThread = NULL;
    }

private:
    HANDLE hThread;
    StartFunc fn;
};


Thread * Thread::Create(StartFunc fn)
{
    return new WindowsThread(fn);
}

class WindowsMutex : public Mutex
{
public:
    WindowsMutex() {
        InitializeCriticalSection(&csMutex);
    }

    void lock() {
        EnterCriticalSection(&csMutex);
    }

    void unlock() {
        LeaveCriticalSection(&csMutex);
    }

    virtual ~WindowsMutex() {
        DeleteCriticalSection(&csMutex);
    }

private:
    CRITICAL_SECTION csMutex;
};

Mutex * Mutex::Create() {
    return new WindowsMutex();
}


extern "C" {
static void CALLBACK timer_callback(void *param, BOOLEAN unused);
}

class WindowsTimer : public Timer
{
public:
    WindowsTimer(TimerFunc fn) : fn(fn), hTimer(NULL) {}

    void schedule(unsigned int seconds) {
        reset();
        if (!seconds) {
            return;
        }

        BOOL ret = CreateTimerQueueTimer(&hTimer,
            NULL,
            timer_callback,
            this,
            seconds * 1000,
            0,
            0);
        if (!ret) {
            printf("GRRRR. Could not create timer!\n");
        }
    }

    bool isActive() {
        return hTimer != NULL;
    }

    void fire() {
        this->fn(NULL);
    }

private:
    TimerFunc fn;
    HANDLE hTimer;

    void reset() {
        if (!hTimer) {
            return;
        }

        DeleteTimerQueueTimer(NULL, hTimer, NULL);
        hTimer = NULL;
    }
};

extern "C" {
static void CALLBACK timer_callback(void *param, BOOLEAN unused)
{
    reinterpret_cast<WindowsTimer*>(param)->fire();
    (void)unused;
}
}

Timer * Timer::Create(Timer::TimerFunc fn)
{
    return new WindowsTimer(fn);
}

} // namespace