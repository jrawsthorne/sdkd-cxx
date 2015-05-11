/**
 * Author: subhashni
 *
 */
#ifndef USAGECOLLECTOR_H_
#define USAGECOLLECTOR_H_
#endif

#ifndef SDKD_INTERNAL_H_
#error "Include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {


    class UsageCollector {
        public:
            UsageCollector():interval(5) {
            } 

            virtual ~UsageCollector() {
            }

            void StopCollector() {
                if (thr && thr->isAlive()){
                    thr->abort();
                }
            }
            
            void Start();
            void Loop();
            void GetResponseJson(Json::Value &res);

            Thread *thr;
            unsigned int interval;

        private:
            Json::Value memusages;
            Json::Value cputimeusages;
            Json::Value samplingtime; 
    };
}
