#include <cstdint>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class Message {

private:
    uint64_t id;
    uint64_t size;
    json data;
public:

};