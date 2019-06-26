#ifndef SDKD_VIEWS_H_
#define SDKD_VIEWS_H_

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd {
using namespace std;

class ViewLoader : protected DebugContext {

public:
    ViewLoader(Handle* handle);
    virtual ~ViewLoader() {}

    bool populateViewData(Command cmd,
                          const Dataset& ds,
                          ResultSet& out,
                          const ResultOptions& options,
                          const Request& req);

private:
    void flushValues(ResultSet& rs);
    struct _kvp {
        string key;
        string value;
    };
    // Make view loading quicker by employing multi operations
    typedef vector<_kvp> kvp_list;
    kvp_list values;
    Handle* handle;
};

class ViewExecutor : protected DebugContext {
public:
    ViewExecutor(Handle *handle);
    virtual ~ViewExecutor();

    bool executeView(Command cmd,
                     ResultSet& out,
                     const ResultOptions& options,
                     const Request& req);

    // Should really be private, but hey, can't have everything
    void handleRowResult(const lcb_vrow_datum_t *dt);
    bool handleHttpChunk(lcb_STATUS err, const lcb_RESPHTTP *resp);

    static set<string> ViewOptions;
    static void InitializeViewOptions();


private:
    bool genOptionsString(const Request& req, string& out, Error& err);
    void runSingleView(lcb_CMDVIEW *cmd, ResultSet& out);

    Handle *handle;
    ResultSet *rs;
    lcb_vrow_ctx_t *rctx;
    Json::Reader jreader;

    bool responseTick;

    // Apparently re-creating these objects each time is expensive
    Json::Value persistRow;
    Json::Value persistKey;
};

#define SDKD_INIT_VIEWS() ViewExecutor::InitializeViewOptions()


} // namespace CBSdkd

#endif /* SDKD_VIEWS_H_ */
