#ifndef CBSDKD_H_
#define CBSDKD_H_

#define CBSDKD_MSGFLD_REQID                     "ReqID"
#define CBSDKD_MSGFLD_CMD                       "Command"
#define CBSDKD_MSGFLD_HID                       "Handle"
#define CBSDKD_MSGFLD_REQDATA                   "CommandData"
#define CBSDKD_MSGFLD_RESDATA                   "ResponseData"
#define CBSDKD_MSGFLD_DSREQ_DSTYPE              "DSType"
#define CBSDKD_MSGFLD_DSREQ_DS                  "DS"
#define CBSDKD_MSGFLD_STATUS                    "Status"
#define CBSDKD_MSGFLD_ERRSTR                    "ErrorString"

#define CBSDKD_MSGFLD_DS_PRELOAD                "Preload"
/**
 * Seed-specific fields
 */
#define CBSDKD_MSGFLD_DSSEED_KSIZE              "KSize"
#define CBSDKD_MSGFLD_DSSEED_VSIZE              "VSize"
#define CBSDKD_MSGFLD_DSSEED_COUNT              "Count"
#define CBSDKD_MSGFLD_DSSEED_KSEED              "KSeed"
#define CBSDKD_MSGFLD_DSSEED_VSEED              "VSeed"
#define CBSDKD_MSGFLD_DSSEED_REPEAT             "Repeat"

/**
 * Inline-specific fields
 */
#define CBSDKD_MSGFLD_DSINLINE_ITEMS            "Items"
#define CBSDKD_MSGFLD_DS_ID                     "ID"

/**
 * DS Request fields
 */

#define CBSDKD_MSGFLD_DSREQ_OPTS                "Options"
#define CBSDKD_MSGFLD_DSREQ_DELAY               "DelayMsec"
#define CBSDKD_MSGFLD_DSREQ_DELAY_MIN           "DelayMin"
#define CBSDKD_MSGLFD_DSREQ_DELAY_MAX           "DelayMax"
#define CBSDKD_MSGFLD_DSREQ_FULL                "Detailed"
#define CBSDKD_MSGFLD_DSREQ_MULTI               "Multi"
#define CBSDKD_MSGFLD_DSREQ_EXPIRY              "Expiry"
#define CBSDKD_MSGFLD_DSREQ_ITERWAIT            "IterWait"
#define CBSDKD_MSGFLD_DSREQ_CONTINUOUS          "Continuous"
#define CBSDKD_MSGFLD_DSREQ_TIMERES             "TimeRes"
#define CBSDKD_MSGFLD_DSREQ_PERSIST             "PersistTo"
#define CBSDKD_MSGFLD_DSREQ_REPLICATE           "ReplicateTo"

/**
 * DS Response fields
 */

#define CBSDKD_MSGFLD_DSRES_STATS               "Summary"
#define CBSDKD_MSGFLD_DSRES_FULL                "Details"
#define CBSDKD_MSGFLD_DRES_TIMINGS              "Timings"

/**
 * Fields for timing information
 */
#define CBSDKD_MSGFLD_TMS_BASE                  "Base"
#define CBSDKD_MSGFLD_TMS_COUNT                 "Count"
#define CBSDKD_MSGFLD_TMS_MIN                   "Min"
#define CBSDKD_MSGFLD_TMS_MAX                   "Max"
#define CBSDKD_MSGFLD_TMS_PERCENTILE            "Percentile"
#define CBSDKD_MSGFLD_TMS_AVG                   "Avg"
#define CBSDKD_MSGFLD_TMS_ECS                   "Errors"
#define CBSDKD_MSGFLD_TMS_WINS                  "Windows"
#define CBSDKD_MSGFLD_TMS_STEP                  "Step"

/**
 * Handle request fields..
 */
#define CBSDKD_MSGFLD_HANDLE_HOSTNAME           "Hostname"
#define CBSDKD_MSGFLD_HANDLE_PORT               "Port"
#define CBSDKD_MSGFLD_HANDLE_BUCKET             "Bucket"
#define CBSDKD_MSGFLD_HANDLE_USERNAME           "Username"
#define CBSDKD_MSGFLD_HANDLE_PASSWORD           "Password"
#define CBSDKD_MSGFLD_HANDLE_OPTIONS            "Options"
#define CBSDKD_MSGFLD_HANDLE_OPT_TMO            "Timeout"
#define CBSDKD_MSGFLD_HANDLE_OPT_BACKUPS        "OtherNodes"
#define CBSDKD_MSGFLD_HANDLE_OPT_CLUSTERCERT    "ClusterCertificate"
#define CBSDKD_MSGFLD_HANDLE_OPT_SSL            "SSL"


/**
 * TTL Parameters
 */
#define CBSDKD_MSGFLD_TTL_SECONDS                "Seconds"

/** View Query Parameters */
#define CBSDKD_MSGFLD_QVOPT_STALE "stale"
#define CBSDKD_MSGFLD_QVOPT_LIMIT "limit"
#define CBSDKD_MSGFLD_QVOPT_ONERR "on_error"
#define CBSDKD_MSGFLD_QVOPT_DESC "descending"
#define CBSDKD_MSGFLD_QVOPT_SKIP "skip"
#define CBSDKD_MSGFLD_QVOPT_REDUCE "reduce"
#define CBSDKD_MSGFLD_QVOPT_INCDOCS "include_docs"
#define CBSDKD_MSGFLD_QV_ONERR_CONTINUE "continue"
#define CBSDKD_MSGFLD_QV_ONERR_STOP "stop"
#define CBSDKD_MSGFLD_QV_STALE_UPDATEAFTER "update_after"


/** View Load Options */
#define CBSDKD_MSGFLD_V_SCHEMA "Schema"
#define CBSDKD_MSGFLD_V_INFLATEBASE "InflateContent"
#define CBSDKD_MSGFLD_V_INFLATECOUNT "InflateLevel"
#define CBSDKD_MSGFLD_V_KIDENT "KIdent"
#define CBSDKD_MSGFLD_V_KSEQ "KVSequence"
#define CBSDKD_MSGFLD_V_DESNAME "DesignName"
#define CBSDKD_MSGFLD_V_MRNAME "ViewName"


/** View Query Control Options */
#define CBSDKD_MSGFLD_V_QOPTS "ViewParameters"
#define CBSDKD_MSGFLD_V_QDELAY "ViewQueryDelay"
#define CBSDKD_MSGFLD_V_QITERCOUNT "ViewQueryCount"

/** get usage interval */
#define CBSDKD_MSDGFLD_SAMPLING_INTERVAL "Interval"
#define FLAGS 0x2

/** N1QL query options */
#define CBSDKD_MSGFLD_NQ_PARAM "NQParam"
#define CBSDKD_MSGFLD_NQ_PARAMVALUES "NQParamValues"
#define CBSDKD_MSGFLD_NQ_INDEX_ENGINE "NQIndexEngine"
#define CBSDKD_MSGFLD_NQ_INDEX_TYPE "NQIndexType"
#define CBSDKD_MSGFLD_NQ_PREPARED "NQPrepared"
#define CBSDKD_MSGFLD_NQ_PARAMETERIZED "NQParameterized"
#define CBSDKD_MSGFLD_NQ_DEFAULT_INDEX_NAME "NQDefaultIndexName"
#define CBSDKD_MSGFLD_NQ_SCANCONSISTENCY "NQScanConsistency"
#define CBSDKD_MSGFLD_NQ_BATCHCOUNT "NQBatchCount"
#define CBSDKD_MSGFLD_NQ_COUNT "NQCount"

/** Subdoc field options */
#define CBSDKD_MSGFLD_SD_SCHEMA "SDSchema"
#define CBSDKD_MSGFLD_SD_PATH "SDPath"
#define CBSDKD_MSGFLD_SD_VALUE "SDValue"
#define CBSDKD_MSGFLD_SD_COMMAND "SDCommand"
#define CBSDKD_MSGFLD_SD_COUNT "Count"

/** FTS field options */
#define CBSDKD_MSGFLD_FTS_INDEXNAME "FTSIndexName"
#define CBSDKD_MSGFLD_FTS_LIMIT "FTSLimit"
#define CBSDKD_MSGFLD_FTS_COUNT "Count"
#define CBSDKD_MSGFLD_FTS_CONSISTENCY "FTSConsistency"

#endif /* CBSDKD_H_ */
