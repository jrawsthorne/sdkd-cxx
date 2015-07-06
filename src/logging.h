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
    static void logcb(lcb_logprocs_st *procs, unsigned int iid, const char *subsys, int severity, const char *srcfile, int srcline, const char *fmt, va_list ap);

}

class Logger : public lcb_logprocs_st {
    public:
        Logger(int level, const char *filename) :start_time(0), file(NULL)
        {
            minlevel = level;
            fp = fopen(filename, "a");
            if (!fp) {
                fp = stderr;
            } else {
                file = filename;
            }
            v.v0.callback = logcb;
            version = 0;
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
            if (!start_time) {
                start_time = gethrtime();
            }
            if (now == start_time) {
                now++;
            }
            flockfile(fp);
            fprintf(fp, "%lums  [I%d] (%s - L:%d) ",
                    (unsigned long)(now - start_time) /1000000,
                    iid,
                    subsys,
                    srcline);
            vfprintf(fp, fmt, ap);
            fprintf(fp, "\n");
            funlockfile(fp);
        }
    private:
        uint64_t start_time;
        const char *file;
        FILE *fp;
        int minlevel;
};

extern "C" {
    static void logcb(lcb_logprocs_st *procs, unsigned int iid, const char *subsys, int severity, const char *srcfile, int srcline, const char *fmt, va_list ap) {
        Logger *logger = static_cast<Logger*>(procs);
        logger->log(iid, subsys, severity, srcfile, srcline, fmt, ap);
    }
}
}

