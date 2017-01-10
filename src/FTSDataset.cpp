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
    spec->count = jspec[CBSDKD_MSGFLD_FTS_COUNT].asUInt();
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
}

void
FTSDatasetIterator::init_data(int idx)
{
    Json::Value doc;
    doc["value"] = "SampleValue" + std::to_string(idx);
    Json::Value subValue;
    subValue["subvalue"] = "SampleSubvalue" + std::to_string(idx);
    subValue["recurringField"] = "RecurringSubvalue";
    doc["SubFields"] = subValue;

    this->curv = Json::FastWriter().write(doc);
    this->curk = std::to_string(idx);
}

bool
FTSDatasetIterator::done() {
    if (this->curidx >= this->spec->count) {
        return true;
    }
    return false;
}

} /* namespace CBSdkd */
