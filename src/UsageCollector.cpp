/**
 * Author: subhashni
 *
 */
#include "sdkd_internal.h"

namespace CBSdkd {
       
  extern "C" {
    #ifdef _WIN32
        static unsigned __stdcall new_stats_collector(void *coll) {
            reinterpret_cast<UsageCollector*>(coll)->Loop();
            return 0;
        }
    #else
        static void *new_stats_collector(void *coll) {
            ((UsageCollector*)coll)->Loop();
            return NULL;
        }
    #endif
    }

    void UsageCollector::Start(void) {
        thr = Thread::Create(new_stats_collector);
        thr->start(this);
    }

    void UsageCollector::Loop(void) {
            struct rusage usage;

            samplingtime = Json::Value(Json::arrayValue);
            memusages = Json::Value(Json::arrayValue);
            cputimeusages = Json::Value(Json::arrayValue);

            while(1) {
                memset(&usage, '0', sizeof usage);
                if((getrusage(RUSAGE_SELF, &usage) == -1)) {
                    fprintf(stderr, "Unable to get usage info ! aborting");
                    exit(1);
                }
                double oncputime_s = usage.ru_utime.tv_sec + usage.ru_stime.tv_sec;
                double oncputime_us = usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
                double time, mem;

                struct timeval tv;
                gettimeofday(&tv, NULL);
                time = tv.tv_sec + (tv.tv_usec/1000000);
                mem = usage.ru_maxrss;

                samplingtime.append(time);
                memusages.append(mem);

                cputimeusages.append(oncputime_s + (oncputime_us/1000000));

                //memusages.push_back(usage.ru_maxrss);
                //cputimeusages.push_back(oncputime_s + (oncputime_us/1000000));

                sleep(interval);
            }
        }

    void UsageCollector::GetResponseJson(Json::Value &res) {
        res["time"] =  samplingtime;
        res["memory"] = memusages;
        res["cpu"] = cputimeusages;
    }
}



