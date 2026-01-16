#ifndef SIMPLE_DB_APPEND_LOG_H
#define SIMPLE_DB_APPEND_LOG_H

#include <string>
#include <fstream>
#include <mutex>
#include <vector>

namespace simpledb {
namespace storage {

// Log record types
enum class RecordType : uint8_t {
    INSERT = 1,
    DELETE = 2,
    COMMIT = 3,
    CHECKPOINT = 4
};

// Log record structure
struct LogRecord {
    RecordType type;
    uint64_t transaction_id;
    std::string key;
    std::string value;
    uint64_t timestamp;
    
    LogRecord() : type(RecordType::INSERT), transaction_id(0), timestamp(0) {}
    LogRecord(RecordType t, uint64_t tid, const std::string& k, 
              const std::string& v, uint64_t ts)
        : type(t), transaction_id(tid), key(k), value(v), timestamp(ts) {}
};

// Append-only log for durability
class AppendLog {
public:
    AppendLog(const std::string& filename);
    ~AppendLog();
    
    size_t append(const LogRecord& record);
    bool read(size_t offset, LogRecord& record);
    std::vector<LogRecord> read_all();
    void sync();  // Force sync to disk
    void checkpoint();
    
    size_t size() const { return current_offset_; }
    
private:
    std::string filename_;
    std::fstream file_;
    mutable std::mutex mutex_;
    size_t current_offset_;
    
    void write_record(const LogRecord& record);
    bool read_record(LogRecord& record);
    size_t serialize_record(const LogRecord& record, std::vector<char>& buffer);
    bool deserialize_record(const std::vector<char>& buffer, LogRecord& record);
};

} // namespace storage
} // namespace simpledb

#endif // SIMPLE_DB_APPEND_LOG_H
