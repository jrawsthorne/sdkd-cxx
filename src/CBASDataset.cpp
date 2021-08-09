#include "sdkd_internal.h"

namespace CBSdkd {

bool
CBASDataset::verify_spec(void)
{
    if (!spec.count) {
        this->err.setCode(Error::SDKD_EINVAL | Error::SUBSYSf_SDKD);
        this->err.errstr = "Must have count";
        return false;
    }

    return true;
}

CBASDataset::CBASDataset(const Json::Value& jspec)
: Dataset(Dataset::DSTYPE_CBAS)
{
    struct CBASDatasetSpecification *spec = &this->spec;
    spec->count = jspec[CBSDKD_MSGFLD_CBAS_COUNT].asUInt();
    verify_spec();

}

CBASDataset::CBASDataset(const struct CBASDatasetSpecification& spec)
: Dataset(Dataset::DSTYPE_CBAS)
{
    this->spec = spec;
}


CBASDatasetIterator*
CBASDataset::getIter() const
{
    return new CBASDatasetIterator(&this->spec);
}

unsigned int
CBASDataset::getCount() const {
    return spec.count;
}


CBASDatasetIterator::CBASDatasetIterator(
        const struct CBASDatasetSpecification *spec)
{
    this->spec = spec;
}


void
CBASDatasetIterator::advance()
{
    DatasetIterator::advance();
}

void
CBASDatasetIterator::init_data(int idx)
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
CBASDatasetIterator::done() {
    if (this->curidx >= this->spec->count) {
        return true;
    }
    return false;
}

} /* namespace CBSdkd */
