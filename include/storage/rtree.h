#ifndef SIMPLE_DB_RTREE_H
#define SIMPLE_DB_RTREE_H

#include <vector>
#include <memory>
#include <string>
#include <algorithm>

namespace simpledb {
namespace storage {

// Bounding box for R-tree
struct BoundingBox {
    double min_x, min_y, max_x, max_y;
    
    BoundingBox() : min_x(0), min_y(0), max_x(0), max_y(0) {}
    BoundingBox(double x1, double y1, double x2, double y2) 
        : min_x(x1), min_y(y1), max_x(x2), max_y(y2) {}
    
    double area() const {
        return (max_x - min_x) * (max_y - min_y);
    }
    
    bool intersects(const BoundingBox& other) const {
        return !(other.min_x > max_x || other.max_x < min_x ||
                 other.min_y > max_y || other.max_y < min_y);
    }
    
    BoundingBox merge(const BoundingBox& other) const {
        return BoundingBox(
            std::min(min_x, other.min_x),
            std::min(min_y, other.min_y),
            std::max(max_x, other.max_x),
            std::max(max_y, other.max_y)
        );
    }
};

// Entry in R-tree (key and its bounding box)
struct RTreeEntry {
    std::string key;
    BoundingBox bbox;
    size_t file_offset;  // Offset in append-only log
    
    RTreeEntry(const std::string& k, const BoundingBox& bb, size_t offset)
        : key(k), bbox(bb), file_offset(offset) {}
};

// R-tree node
struct RTreeNode {
    bool is_leaf;
    BoundingBox bbox;
    std::vector<std::unique_ptr<RTreeNode>> children;
    std::vector<RTreeEntry> entries;
    
    RTreeNode(bool leaf) : is_leaf(leaf) {}
};

// R-tree for spatial indexing
class RTree {
public:
    RTree(size_t max_entries = 4);
    ~RTree() = default;
    
    void insert(const std::string& key, const BoundingBox& bbox, size_t offset);
    bool search(const std::string& key, size_t& offset);
    std::vector<RTreeEntry> range_search(const BoundingBox& query_box);
    bool remove(const std::string& key);
    
private:
    std::unique_ptr<RTreeNode> root_;
    size_t max_entries_;
    
    void insert_internal(RTreeNode* node, const RTreeEntry& entry);
    RTreeNode* choose_leaf(RTreeNode* node, const BoundingBox& bbox);
    void split_node(RTreeNode* node);
    bool search_internal(RTreeNode* node, const std::string& key, size_t& offset);
    void range_search_internal(RTreeNode* node, const BoundingBox& query_box, 
                               std::vector<RTreeEntry>& results);
};

} // namespace storage
} // namespace simpledb

#endif // SIMPLE_DB_RTREE_H
