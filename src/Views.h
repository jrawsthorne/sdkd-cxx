#ifndef SDKD_VIEWS_H_
#define SDKD_VIEWS_H_

#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#include "sdkd_internal.h"

namespace CBSdkd
{
using namespace std;

class ViewLoader : protected DebugContext
{

  public:
    ViewLoader(Handle* handle);

    bool populateViewData(Command cmd, const Dataset& ds, ResultSet& out, const ResultOptions& options, const Request& req);

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

class ViewExecutor : protected DebugContext
{
  public:
    ViewExecutor(Handle* handle);

    bool executeView(Command cmd, ResultSet& out, const ResultOptions& options, const Request& req);

  private:
    Handle* handle;
    ResultSet* rs;
};

} // namespace CBSdkd

#endif /* SDKD_VIEWS_H_ */
