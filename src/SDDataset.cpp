#include "sdkd_internal.h"

namespace CBSdkd {

bool
SDDataset::verify_spec(void)
{
    if (!spec.count) {
        this->err.setCode(Error::SDKD_EINVAL | Error::SUBSYSf_SDKD);
        this->err.errstr = "Must have count";
        return false;
    }

    return true;
}

SDDataset::SDDataset(const Json::Value& jspec, bool load)
: Dataset(Dataset::DSTYPE_SD)
{
    struct SDDatasetSpecification *spec = &this->spec;
    spec->load = load;
    if (load) {
        const Json::Value &doc = jspec[CBSDKD_MSGFLD_SD_SCHEMA].asString();
        spec->doc = doc;
    } else {
        spec->path = jspec[CBSDKD_MSGFLD_SD_PATH].asString();
        spec->value = jspec[CBSDKD_MSGFLD_SD_VALUE].asString();
    }
    spec->count = jspec[CBSDKD_MSGFLD_NQ_COUNT].asUInt();
    spec->continuous = jspec[CBSDKD_MSGFLD_DSREQ_CONTINUOUS].asTruthVal();
    verify_spec();

}

SDDataset::SDDataset(const struct SDDatasetSpecification& spec)
: Dataset(Dataset::DSTYPE_SD)
{
    this->spec = spec;
}


SDDatasetIterator*
SDDataset::getIter() const
{
    return new SDDatasetIterator(&this->spec);
}

unsigned int
SDDataset::getCount() const {
    return spec.count;
}


SDDatasetIterator::SDDatasetIterator(
        const struct SDDatasetSpecification *spec)
{
    this->spec = spec;
}


void
SDDatasetIterator::advance()
{
    DatasetIterator::advance();
    if (spec->continuous && curidx > spec->count) {
        curidx = 0;
    }
}

void
SDDatasetIterator::init_data(int idx)
{
    if (spec->load) {
        Json::Value doc = spec->doc;
        doc["id"] = std::to_string(idx);
        this->curv = Json::FastWriter().write(doc);
    } else {
        this->curp = spec->path;
        this->curv = spec->value;
    }
    this->curk = std::to_string(idx);
}

bool
SDDatasetIterator::done() {

    if (this->spec->continuous) {
        // Continuous always returns True
        return false;
    }

    if (this->curidx >= this->spec->count) {
        return true;
    }
    return false;
}
} /* namespace CBSdkd */
