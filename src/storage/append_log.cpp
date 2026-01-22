#include "storage/append_log.h"
#include <cstring>
#include <chrono>

namespace simpledb {
namespace storage {

AppendLog::AppendLog(const std::string& filename) 
    : filename_(filename), current_offset_(0) {
    file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    
    if (!file_.is_open()) {
        // Create new file
        file_.clear();
        file_.open(filename_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(filename_, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    }
    
    // Get current file size
    file_.seekg(0, std::ios::end);
    current_offset_ = file_.tellg();
}

AppendLog::~AppendLog() {
    if (file_.is_open()) {
        file_.close();
    }
}

size_t AppendLog::append(const LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t offset = current_offset_;
    write_record(record);
    
    return offset;
}

void AppendLog::write_record(const LogRecord& record) {
    std::vector<char> buffer;
    size_t size = serialize_record(record, buffer);
    
    file_.write(buffer.data(), size);
    file_.flush();
    
    current_offset_ += size;
}

size_t AppendLog::serialize_record(const LogRecord& record, std::vector<char>& buffer) {
    // Format: [type:1][txn_id:8][timestamp:8][is_vector:1][key_len:4][key][data_len:4][data]
    
    size_t key_len = record.key.size();
    size_t data_len = record.is_vector ? (record.vector_data.size() * sizeof(float)) : record.value.size();
    size_t total_size = 1 + 8 + 8 + 1 + 4 + key_len + 4 + data_len;
    
    buffer.resize(total_size);
    size_t offset = 0;
    
    // Type
    buffer[offset++] = static_cast<char>(record.type);
    
    // Transaction ID
    std::memcpy(&buffer[offset], &record.transaction_id, 8);
    offset += 8;
    
    // Timestamp
    std::memcpy(&buffer[offset], &record.timestamp, 8);
    offset += 8;
    
    // Is vector flag
    buffer[offset++] = record.is_vector ? 1 : 0;
    
    // Key length and key
    std::memcpy(&buffer[offset], &key_len, 4);
    offset += 4;
    std::memcpy(&buffer[offset], record.key.data(), key_len);
    offset += key_len;
    
    // Data length and data
    std::memcpy(&buffer[offset], &data_len, 4);
    offset += 4;
    if (record.is_vector) {
        std::memcpy(&buffer[offset], record.vector_data.data(), data_len);
    } else {
        std::memcpy(&buffer[offset], record.value.data(), data_len);
    }
    offset += data_len;
    
    return total_size;
}

bool AppendLog::read(size_t offset, LogRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    file_.seekg(offset);
    return read_record(record);
}

bool AppendLog::read_record(LogRecord& record) {
    // Read type
    char type_byte;
    file_.read(&type_byte, 1);
    if (file_.gcount() != 1) return false;
    record.type = static_cast<RecordType>(type_byte);
    
    // Read transaction ID
    file_.read(reinterpret_cast<char*>(&record.transaction_id), 8);
    if (file_.gcount() != 8) return false;
    
    // Read timestamp
    file_.read(reinterpret_cast<char*>(&record.timestamp), 8);
    if (file_.gcount() != 8) return false;
    
    // Read is_vector flag
    char is_vector_byte;
    file_.read(&is_vector_byte, 1);
    if (file_.gcount() != 1) return false;
    record.is_vector = (is_vector_byte != 0);
    
    // Read key
    uint32_t key_len;
    file_.read(reinterpret_cast<char*>(&key_len), 4);
    if (file_.gcount() != 4) return false;
    
    record.key.resize(key_len);
    file_.read(&record.key[0], key_len);
    if (file_.gcount() != static_cast<std::streamsize>(key_len)) return false;
    
    // Read data
    uint32_t data_len;
    file_.read(reinterpret_cast<char*>(&data_len), 4);
    if (file_.gcount() != 4) return false;
    
    if (record.is_vector) {
        size_t vec_size = data_len / sizeof(float);
        record.vector_data.resize(vec_size);
        file_.read(reinterpret_cast<char*>(record.vector_data.data()), data_len);
    } else {
        record.value.resize(data_len);
        file_.read(&record.value[0], data_len);
    }
    if (file_.gcount() != static_cast<std::streamsize>(data_len)) return false;
    
    return true;
}

std::vector<LogRecord> AppendLog::read_all() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<LogRecord> records;
    file_.seekg(0, std::ios::beg);
    
    LogRecord record;
    while (read_record(record)) {
        records.push_back(record);
    }
    
    return records;
}

void AppendLog::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.flush();
}

void AppendLog::checkpoint() {
    sync();
    // In a real implementation, we would truncate old log entries
    // and create a snapshot of the current state
}

} // namespace storage
} // namespace simpledb
