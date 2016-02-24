/*
 * Dataset.h
 *
 *  Created on: May 11, 2012
 *      Author: mnunberg
 */

#ifndef DATASET_H_
#define DATASET_H_
#ifndef SDKD_INTERNAL_H_
#error "include sdkd_internal.h first"
#endif

#define CBSDKD_DATASET_XTYPE(X) \
    X(DSTYPE_FILE) \
    X(DSTYPE_INLINE) \
    X(DSTYPE_REFERENCE) \
    X(DSTYPE_SEEDED) \
    X(DSTYPE_N1QL)

namespace CBSdkd {

class DatasetIterator {

public:
    DatasetIterator() {};

    virtual ~DatasetIterator() {
        // TODO Auto-generated destructor stub
    }

    const std::string key() const;
    const std::string value() const;
    void start();
    virtual void advance();
    virtual bool done() = 0;

protected:
    std::string curk;
    std::string curv;

    virtual void init_data(int idx) = 0;
    unsigned int curidx;

};

class Dataset {
public:
    enum Type {
#define X(c) c,
        CBSDKD_DATASET_XTYPE(X)
#undef X
        DSTYPE_INVALID
    };

    Dataset(Type);

    virtual ~Dataset() {
        // TODO Auto-generated destructor stub
    }

    virtual DatasetIterator* getIter() const = 0;
    virtual unsigned int getCount() const = 0;

    bool isValid();

    static Type
    determineType(const Request& req, std::string *refid);

    static Dataset*
    fromType(Type t, const Request& req);

    Error err;
    Type type;
};

class DatasetInlineIterator : public DatasetIterator
{
public:
    DatasetInlineIterator(const Json::Value *items);
    bool done();
    unsigned int getCount() const;

private:
    void init_data(int idx);
    const Json::Value *items;
};

// Inline dataset class.
class DatasetInline : public Dataset
{
public:
    DatasetInline(const Json::Value& json);
    DatasetIterator* getIter() const;
    unsigned int getCount() const;

private:
    Json::Value items;
};


struct DatasetSeedSpecification {
    unsigned int count;
    unsigned int ksize;
    unsigned int vsize;
    bool continuous;
    std::string repeat;
    std::string kseed;
    std::string vseed;
};

class DatasetSeededIterator : public DatasetIterator
{
public:
    DatasetSeededIterator(const struct DatasetSeedSpecification *spec);
    bool done();
    virtual void advance();

private:
    void init_data(int idx);
    const DatasetSeedSpecification *spec;
};

class DatasetSeeded : public Dataset
{

public:
    DatasetSeeded(const Json::Value& spec);
    DatasetSeeded(const struct DatasetSeedSpecification& spec);
    DatasetIterator* getIter() const;
    unsigned int getCount() const;

    const DatasetSeedSpecification& getSpec() const {
        return spec;
    }

private:
    struct DatasetSeedSpecification spec;
    bool verify_spec();

};

struct N1QLDatasetSpecification {
    std::vector<std::string> params;
    std::vector<std::string> paramValues;
    bool continuous;
    unsigned int count;
};

class N1QLDatasetIterator : public DatasetIterator
{
public:
    N1QLDatasetIterator(const struct N1QLDatasetSpecification *spec);
    bool done();
    virtual void advance();

private:
    void init_data(int idx);
    const N1QLDatasetSpecification *spec;
};

class N1QLDataset : public Dataset {
public:
    N1QLDataset(const Json::Value& spec);
    N1QLDataset(const struct N1QLDatasetSpecification& spec);
    N1QLDatasetIterator* getIter() const;
    unsigned int getCount() const;
    void split(const std::string &s, char delim, std::vector<std::string> &elems);

    const N1QLDatasetSpecification& getSpec() const {
        return spec;
    }

private:
    struct N1QLDatasetSpecification spec;
    bool verify_spec();
};

} /* namespace CBSdkd */
#endif /* DATASET_H_ */
