#include "sdkd_internal.h"

namespace CBSdkd {

bool
FTSDataset::verify_spec(void)
{
    if (!spec.count) {
        this->err.setCode(Error::SDKD_EINVAL | Error::SUBSYSf_SDKD);
        this->err.errstr = "Must have count";
        return false;
    }

    return true;
}

FTSDataset::FTSDataset(const Json::Value& jspec)
: Dataset(Dataset::DSTYPE_FTS)
{
    struct FTSDatasetSpecification *spec = &this->spec;
    const Json::Value &doc = jspec[CBSDKD_MSGFLD_FTS_SCHEMA].asString();
    spec->doc = doc;
    spec->count = jspec[CBSDKD_MSGFLD_FTS_COUNT].asUInt();
    spec->continuous = jspec[CBSDKD_MSGFLD_DSREQ_CONTINUOUS].asTruthVal();
    verify_spec();

}

FTSDataset::FTSDataset(const struct FTSDatasetSpecification& spec)
: Dataset(Dataset::DSTYPE_FTS)
{
    this->spec = spec;
}


FTSDatasetIterator*
FTSDataset::getIter() const
{
    return new FTSDatasetIterator(&this->spec);
}

unsigned int
FTSDataset::getCount() const {
    return spec.count;
}


FTSDatasetIterator::FTSDatasetIterator(
        const struct FTSDatasetSpecification *spec)
{
    this->spec = spec;
}


void
FTSDatasetIterator::advance()
{
    DatasetIterator::advance();
    if (spec->continuous && curidx > spec->count) {
        curidx = 0;
    }
}

void
FTSDatasetIterator::init_data(int idx)
{
    Json::Value doc = spec->doc;
    this->curv = Json::FastWriter().write(doc);
    std::string newcurv;
    for(std::string::iterator it = this->curv.begin(); it != this->curv.end(); ++it) {
        if (*it != '\\' && it != this->curv.begin() && it != this->curv.end()-2 && it != this->curv.end() -1 && it != this->curv.end()) {
            newcurv += *it;
        }
    }
    this->curv = newcurv;
    this->curk = std::to_string(idx);
}

bool
FTSDatasetIterator::done() {

    if (this->spec->continuous) {
        return false;
    }

    if (this->curidx >= this->spec->count) {
        return true;
    }
    return false;
}
} /* namespace CBSdkd */
