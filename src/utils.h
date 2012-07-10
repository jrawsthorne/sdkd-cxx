#ifndef UTILS_H_
#define UTILS_H_

namespace CBSdkd {

template <typename T, typename U>
// stolen from http://stackoverflow.com/a/1730798/479941
class create_map
{
private:
    std::map<T, U> m_map;
public:
    create_map(const T& key, const U& val) { m_map[key] = val; }
    create_map<T, U>& operator()(const T& key, const U& val)
    { m_map[key] = val;  return *this; }
    operator std::map<T, U>()
    { return m_map; }
};

}

#endif /* UTILS_H_ */
