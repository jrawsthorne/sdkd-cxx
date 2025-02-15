#include "sdkd_internal.h"
#include <thread>

namespace CBSdkd
{
bool
CBASQueryExecutor::execute(ResultSet& out, const ResultOptions& options, const Request& req)
{
    out.clear();

    handle->externalEnter();
    unsigned int kvCount = req.payload[CBSDKD_MSGFLD_CBAS_COUNT].asInt64();

    // Hardcoded for now until sdkdclient supports more than 1 analytics collection
    std::string scope = "0";
    std::string collection = "0";
    std::string bucket = handle->options.bucket;

    while (!handle->isCancelled()) {
        out.cbas_query_resp_count = 0;

        std::string q = "SELECT * FROM defaultDataSet where `value` = 'SampleValue" + std::to_string(generator) + "'";

        if (handle->options.useCollections) {
            q = "SELECT * FROM `" + collection + "` where `" + collection + "`.`value` = 'SampleValue" + std::to_string(generator) + "'";
        }

        std::vector<std::string> rows{};

        couchbase::operations::analytics_request req{};
        req.statement = q;
        req.bucket_name = bucket;
        req.row_callback = [&rows](std::string&& row) {
            rows.emplace_back(std::move(row));
            return couchbase::utils::json::stream_control::next_row;
        };

        out.markBegin();

        if (handle->options.useCollections) {
            req.scope_name = scope;
        }

        auto resp = handle->execute(req);

        out.cbas_query_resp_count = rows.size();
        out.setRescode(resp.ctx.ec, true);

        generator = (generator + 1) % kvCount;
    }
    return true;
}
} // namespace CBSdkd
