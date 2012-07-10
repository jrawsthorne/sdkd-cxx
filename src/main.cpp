#include "Message.h"
#include "Dataset.h"
#include "Request.h"
#include "Response.h"
#include "Handle.h"
#include "IODispatch.h"

#include <libcouchbase/couchbase.h>

#include <sstream>
#include <cstdio>
#include <cassert>
#include <cstdlib>
#include "contrib/debug++.h"

#define KVCOUNT 5

using namespace CBSdkd;


DebugContext CBSdkd::CBsdkd_Global_Debug_Context;

static void
testMessage(void)
{
    printf("Attempting to create simple request..\n");
    std::string foo("{\"Command\":\"NEWHANDLE\", \"ReqID\":42 ");
    foo += ", \"CommandData\" : {} ";
    foo += "}";

    Error errp;
    Request *msg = Request::decode(foo, &errp);
    if (!msg) {
        cerr << "Couldn't decode: " << errp.prettyPrint() << endl;
        abort();
    }
    assert(msg->command == Command::NEWHANDLE);
    assert(msg->reqid == 42);
    assert(msg->isValid() == true);

    Response r(msg);

    assert(r.command == msg->command);
    assert(r.handle_id == msg->handle_id);

    cout << "Encoded Response: " << r.encode() << endl;
    delete msg;

}

static void
testSeededIterator(void)
{
    printf("Performing Seeded tests\n");

    struct DatasetSeedSpecification spec;
    spec.kseed = "The_Key";
    spec.vseed = "The_Value";
    spec.ksize = 12;
    spec.vsize = 24;
    spec.repeat = "!-";
    spec.count = KVCOUNT;

    DatasetSeeded seeded(spec);
    assert(seeded.isValid() == true);
    DatasetIterator *iter = seeded.getIter();
    assert(iter != NULL);

    for (iter->start(); iter->done() == false; iter->advance()) {
        printf("Key: %s => %s\n", iter->key().c_str(), iter->value().c_str());
    }
    delete iter;

    printf("\n");

}

static void
testInlineIterator(void)
{
    printf("Will perform Inline tests\n");
    Json::Value newv;
    Json::Value newcontainer;

    for (int i = 0; i < KVCOUNT; i++) {
        Json::Value jl;
        stringstream ss;

        ss << "Key_" << i;
        jl[0] = ss.str();

        ss.str("");
        ss << "Value_" << i;
        jl[1] = ss.str();

        newv[i] = jl;
    }

    newcontainer["Items"] = newv;
    DatasetInline inl = DatasetInline(newcontainer);
    assert(inl.isValid() == true);
    DatasetIterator *iter = inl.getIter();

    for (iter->start(); iter->done() == false; iter->advance()) {
        printf("%s => %s\n", iter->key().c_str(), iter->value().c_str());
    }

    delete iter;
    printf("\n");

}

static void
testNewHandle(void)
{
    log_noctx_info("Testing handle operations..");
    Json::Value newv;
    std::string reqstr = "{ \"Command\" : \"NEWHANDLE\" ";
    reqstr += ", \"ReqID\" : 0";
    reqstr += ", \"Handle\" : 1 ";
    reqstr += ", \"CommandData\" : {";
    reqstr += " \"Username\" : \"Administrator\" ";
    reqstr += ", \"Password\" : \"123456\" ";
    reqstr += ", \"Bucket\" : \"membase0\" ";
    reqstr += ", \"Hostname\" : \"localhost\" ";
    reqstr += "}}";

    Error errp;
    Request *msg = Request::decode(reqstr, &errp);
    if (!msg) {
        cerr << "Problem decoding message..\n";
        cerr << errp.prettyPrint();
        abort();
    }

    assert(msg->isValid());
    std::string errmsg;
    HandleOptions hopts = HandleOptions(msg->payload);
    assert(hopts.isValid());
    Handle h(hopts);
    delete msg;

    if (!h.connect(&errp)) {
        cerr << "Couldn't connect: " << errp.prettyPrint() << endl;
        abort();
    }

    struct DatasetSeedSpecification spec = { 0 };
    spec.kseed = "The_Key";
    spec.vseed = "The_Value";
    spec.ksize = spec.vsize = 12;
    spec.repeat = "*";
    spec.count = 10;

    DatasetSeeded ds(spec);
    ResultSet rs;
    ResultOptions opts;

    h.dsMutate(Command::MC_DS_MUTATE_SET, ds, rs);

    printf("Dumping SET summaries\n");
    for (map<int,int>::iterator iter = rs.stats.begin();
            iter != rs.stats.end(); iter++) {
        printf("%d => %d\n", iter->first, iter->second);
    }
    printf("\n");


    opts.full = true;
    h.dsGet(Command::MC_DS_GET, ds, rs, opts);

    printf("Dumping GET Summaries\n");
    for (map<int,int>::iterator iter = rs.stats.begin();
            iter != rs.stats.end(); iter++) {
        printf("%d => %d\n", iter->first, iter->second);
    }
    printf("\n");


    printf("Dumping GET Details\n");
    for (map<std::string,FullResult>::iterator iter = rs.fullstats.begin();
            iter != rs.fullstats.end(); iter++) {
        printf("%s => ", iter->first.c_str());
        if (iter->second) {
            printf("OK: %s", iter->second.getString().c_str());
        } else {
            printf("ERR: %d", iter->second.getStatus());
        }
        printf("\n");
    }
    printf("\n");
}

// Separate thread we spawn as the 'client', for testing..
static void *
_dispatchfn(void *ctx)
{
    struct sockaddr_in *saddr = (struct sockaddr_in*)ctx;

    int ctlfd = socket(AF_INET, SOCK_STREAM, 0);

    assert(ctlfd != -1);
    assert(-1 != connect(ctlfd, (struct sockaddr*)saddr, sizeof(*saddr)));

    // So we have a control socket.. good...

    int newfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(newfd != -1);
    assert(-1 != connect(newfd, (struct sockaddr*)saddr, sizeof(*saddr)));
    IOProtoHandler iop = IOProtoHandler(newfd);
    Json::Value req;
    req["Command"] = "NEWHANDLE";
    req["ReqID"] = 32;
    req["Handle"] = 1;
    Json::Value hjopts;
    hjopts["Bucket"] = "membase0";
    hjopts["Hostname"] = "localhost";
    req["CommandData"] = hjopts;
    std::string obuf = Json::FastWriter().write(req);

    assert(iop.putRawMessage(obuf, true) == iop.OK);
    std::string resbuf = "";
    assert( iop.getRawMessage(resbuf, true) == iop.OK );
    log_noctx_info("Got response %s", resbuf.c_str());
    Json::Value res;
    Json::Reader().parse(resbuf, res);
    assert(res["Status"] == 0 && res["Handle"].asInt() == 1);



    close(newfd);
    close(ctlfd);
    return NULL;
}

static void testDispatcher(void)
{
    MainDispatch dispatch;
    struct sockaddr_in saddr;
    assert( dispatch.establishSocket(&saddr) == true );
    printf("Listening on %d\n", ntohs(saddr.sin_port));
    pthread_t thr;
    pthread_create(&thr, NULL, _dispatchfn, &saddr);
    dispatch.run();
}

int main(void)
{
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.color = 1;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.initialized = 1;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.out = stderr;
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.prefix = "cbskd-test";
    CBsdkd_Global_Debug_Context.cbsdkd__debugctx.level = CBSDKD_LOGLVL_DEBUG;

    printf("Using libcouchbase version %s\n\n",
           libcouchbase_get_version(NULL));
    testMessage();
    testSeededIterator();
    testInlineIterator();
    testNewHandle();
    testDispatcher();
    return 0;
}
