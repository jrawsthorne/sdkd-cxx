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

/**
 * DS Response fields
 */

#define CBSDKD_MSGFLD_DSRES_STATS               "Summary"
#define CBSDKD_MSGFLD_DSRES_FULL                "Details"

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

#endif /* CBSDKD_H_ */
