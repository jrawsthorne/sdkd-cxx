/*
 * Dataset.cpp
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#include "/home/mnunberg/src/cbsdkd/cplusplus/Dataset.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <auto_ptr.h>

namespace CBSdkd {



Dataset::Dataset(Type t) {
    // TODO Auto-generated constructor stub
    this->type = t;
    this->err.code = (Error::Code)0;
}

bool
Dataset::isValid() {
    if (this->err.code == Error::SUCCESS) {
        return true;
    }
    return false;
}



const std::string
DatasetIterator::key() const
{
    return curk;
}

const std::string
DatasetIterator::value() const
{
    return curv;
}

void
DatasetIterator::start()
{
    curidx = 0;
    init_data(curidx);
}

void DatasetIterator::advance()
{
    curk.clear();
    curv.clear();
    curidx++;
    init_data(curidx);
}

DatasetInline::DatasetInline(const Json::Value& json)
: Dataset::Dataset(Dataset::DSTYPE_INLINE)
{
    if (!json["Items"]) {
        this->err = Error(Error::SDKD_EINVAL,
                          "Expected 'Items' but couldn't find any");
        return;
    }
    this->items = json["Items"];
}

DatasetIterator *
DatasetInline::getIter() const
{
    return new DatasetInlineIterator(&this->items);
}

unsigned int
DatasetInline::getCount() const
{
    return this->items.size();
}

DatasetInlineIterator::DatasetInlineIterator(const Json::Value *items) {

    this->items = items;
    this->curidx = 0;
}

void
DatasetInlineIterator::init_data(int idx)
{
    Json::Value pair = (*this->items)[idx];
    if (pair.isArray()) {
        this->curk = pair[0].asString();
        this->curv = pair[1].asString();
    } else {
        this->curk = pair.asString();
    }
}

bool
DatasetInlineIterator::done()
{
    if (this->curidx >= this->items->size()) {
        return true;
    }
    return false;
}


bool
DatasetSeeded::verify_spec(void)
{
    if (!spec.count) {
        this->err.setCode(Error::SDKD_EINVAL | Error::SUBSYSf_SDKD);
        this->err.errstr = "Must have count";
        return false;
    }

    if (spec.kseed.size() == 0 || spec.vseed.size() == 0) {
        this->err.setCode(Error::SDKD_EINVAL | Error::SUBSYSf_SDKD);
        this->err.errstr = "KSeed and VSeed must not be empty";
        return false;
    }
    return true;
}

DatasetSeeded::DatasetSeeded(const Json::Value& jspec)
: Dataset::Dataset(Dataset::DSTYPE_SEEDED)
{
    struct DatasetSeedSpecification *spec = &this->spec;
    memset(spec, 0, sizeof(*spec));

    spec->kseed = jspec["KSeed"].asString();
    spec->vseed = jspec["VSeed"].asString();
    spec->ksize = jspec["KSize"].asUInt();
    spec->vsize = jspec["VSize"].asUInt();

    spec->repeat = jspec["Repeat"].asString();
    spec->count = jspec["Count"].asUInt();
    verify_spec();

}

DatasetSeeded::DatasetSeeded(const struct DatasetSeedSpecification& spec)
: Dataset::Dataset(Dataset::DSTYPE_SEEDED)
{
//    memcpy(&this->spec, spec, sizeof(this->spec));
    this->spec = spec;
}

DatasetIterator*
DatasetSeeded::getIter() const
{
    return new DatasetSeededIterator(&this->spec);
}

unsigned int
DatasetSeeded::getCount() const {
    return spec.count;
}


DatasetSeededIterator::DatasetSeededIterator(
        const struct DatasetSeedSpecification *spec)
{
    this->spec = spec;
}

static const std::string
_fill_repeat(const std::string base,
             unsigned int limit,
             const std::string repeat,
             unsigned int idx)
{
    std::string filler, ret;
    char dummy;
    const char *fmt = "%s%lu";

    unsigned long multiplier;

    unsigned long nw;
    unsigned long wanted = snprintf(&dummy, 0, fmt, repeat.c_str(), idx);


    filler.resize(wanted+1);
    nw = snprintf((char*)filler.c_str(), filler.size(), fmt, repeat.c_str(), idx);

    // Because sprintf places an extra NUL at the end, we chop it off again.
    filler.resize(wanted);

    assert(nw == wanted);
    multiplier = 1;
    while ( (nw * multiplier) + base.length() < limit) {
        multiplier++;
    }

    ret = base;
    ret.reserve(ret.size() + (nw * multiplier));
    for (int i = 0; i <= multiplier; i++) {
        ret += filler;
    }

    return ret;
}

void
DatasetSeededIterator::init_data(int idx)
{
    this->curk = _fill_repeat(this->spec->kseed,
                              this->spec->ksize,
                              this->spec->repeat,
                              idx);
    this->curv = _fill_repeat(this->spec->vseed,
                              this->spec->vsize,
                              this->spec->repeat,
                              idx);
}


bool
DatasetSeededIterator::done() {
    if (this->curidx >= this->spec->count) {
        return true;
    }
    return false;
}

} /* namespace CBSdkd */
