#pragma once

#include <cstdint>
#include <include/nlohmann/json.hpp>

class Message {

private:
    uint64_t id;
    uint64_t size;
    nlohmann::json data;
public:
    Message(uint64_t id, uint64_t size, const nlohmann::json& data)
        : id(id), size(size), data(data) {}

    uint64_t getId() const { return id; }
    uint64_t getSize() const { return size; }
    nlohmann::json getData() const { return data; }

    void setId(uint64_t newId) { id = newId; }
    void setSize(uint64_t newSize) { size = newSize; }
    void setData(const nlohmann::json& newData) { data = newData; }
};
