#include "Message.h"
#include "Dataset.h"
#include "Request.h"
#include "Response.h"
#include "Handle.h"

#include <libcouchbase/couchbase.h>

#include <sstream>
#include <cstdio>
#include <cassert>
#include <cstdlib>


#define KVCOUNT 5

using namespace CBSdkd;


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

    if (!Handle::verifyRequest(*msg, &errmsg)) {
        cerr << "Couldn't verify request: " << errmsg << endl;
        abort();
    }

    Handle h(*msg);
    if (!h.connect(&errp)) {
        cerr << "Couldn't connect: " << errp.prettyPrint() << endl;
        abort();
    }

    struct DatasetSeedSpecification spec;
    spec.kseed = "The_Key";
    spec.vseed = "The_Value";
    spec.ksize = spec.vsize = 12;
    spec.repeat = "*";
    spec.count = 10;
    DatasetSeeded ds(spec);

    ResultSet rs = h.dsMutate(Command::MC_DS_MUTATE_SET,
                              ds,
                              Json::Value());

    printf("Dumping SET summaries\n");
    for (map<int,int>::iterator iter = rs.stats.begin();
            iter != rs.stats.end(); iter++) {
        printf("%d: %d\n", iter->first, iter->second);
    }
    printf("\n");

    Json::Value opts;
    opts["Full"] = true;

    rs = h.dsGet(Command::MC_DS_GET,
                 ds,
                 opts);

    printf("Dumping GET Summaries\n");
    for (map<int,int>::iterator iter = rs.stats.begin();
            iter != rs.stats.end(); iter++) {
        printf("%d: %d\n", iter->first, iter->second);
    }
    printf("\n");


    printf("Dumping GET Details\n");
    for (map<std::string,std::string>::iterator iter = rs.fullstats.begin();
            iter != rs.fullstats.end(); iter++) {
        printf("%s: %s\n", iter->first.c_str(), iter->second.c_str());
    }
    printf("\n");

}

int main(void)
{

    printf("Using libcouchbase version %s\n\n",
           libcouchbase_get_version(NULL));
    testMessage();
    testSeededIterator();
    testInlineIterator();
    testNewHandle();
    return 0;
}
