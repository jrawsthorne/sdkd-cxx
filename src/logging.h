#ifndef LOGGER_H
#define LOGGER_H
#endif

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#ifdef _WIN32
#define flockfile(x) (void) 0
#define funlockfile(x) (void) 0
#endif

namespace CBSdkd {

extern "C" {
    static void logcb(const lcb_LOGGER *procs, uint64_t iid, const char *subsys, lcb_LOG_SEVERITY severity, const char *srcfile, int srcline, const char *fmt, va_list ap);

}

class Logger {
    public:
        Logger(const char *filename, lcb_LOGGER *lcblogger) :start_time(0), file(NULL)
        {
            fp = fopen(filename, "a");
            if (!fp) {
                fp = stderr;
            } else {
                file = filename;
            }
            lcb_logger_create(&lcblogger, this);
            lcb_logger_callback(lcblogger, &logcb);
        }
        ~Logger()
        {
            if (file) {
                fclose(fp);
            }
        }

        uint64_t gethrtime() {
            uint64_t ret = 0;
            struct timeval tv;
            if (gettimeofday(&tv, NULL) == -1) {
                return -1;
            }
            ret = (uint64_t)tv.tv_sec * 1000000000;
            ret += (uint64_t)tv.tv_usec * 1000;
            return ret;
        }

        void log(unsigned int iid,
                const char *subsys,
                int severity,
                const char *srcfile,
                int srcline,
                const char *fmt,
                va_list ap)
        {
            uint64_t now = gethrtime();
            if (start_time == 0) {
                start_time = now;
            }
            flockfile(fp);
            fprintf(fp, "%lums %s [I%d] (%s - L:%d) ",
                    (unsigned long)(now - start_time) /1000000,
                    severity_str(severity),
                    iid,
                    subsys,
                    srcline);
            vfprintf(fp, fmt, ap);
            fprintf(fp, "\n");
            fflush(fp);
            funlockfile(fp);
        }
    private:
        static const char * severity_str(int severity)
        {
            switch (severity) {
            case LCB_LOG_TRACE:
                return "TRACE";
            case LCB_LOG_DEBUG:
                return "DEBUG";
            case LCB_LOG_INFO:
                return "INFO";
            case LCB_LOG_WARN:
                return "WARN";
            case LCB_LOG_ERROR:
                return "ERROR";
            case LCB_LOG_FATAL:
                return "FATAL";
            default:
                return "";
            }
        }

        uint64_t start_time;
        const char *file;
        FILE *fp;
};

extern "C" {
    static void logcb(const lcb_LOGGER *procs, uint64_t iid, const char *subsys, lcb_LOG_SEVERITY severity, const char *srcfile, int srcline, const char *fmt, va_list ap) {
        Logger *logger;
        lcb_logger_cookie(procs, reinterpret_cast<void **>(&logger));
        logger->log(iid, subsys, severity, srcfile, srcline, fmt, ap);
    }
}
}

