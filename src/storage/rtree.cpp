#include "storage/rtree.h"
#include <limits>
#include <cmath>

namespace simpledb {
namespace storage {

RTree::RTree(size_t max_entries) : max_entries_(max_entries) {
    root_ = std::make_unique<RTreeNode>(true);
}

void RTree::insert(const std::string& key, const BoundingBox& bbox, size_t offset) {
    RTreeEntry entry(key, bbox, offset);
    insert_internal(root_.get(), entry);
}

void RTree::insert_internal(RTreeNode* node, const RTreeEntry& entry) {
    if (node->is_leaf) {
        node->entries.push_back(entry);
        node->bbox = node->bbox.merge(entry.bbox);
        
        if (node->entries.size() > max_entries_) {
            split_node(node);
        }
    } else {
        RTreeNode* best_child = choose_leaf(node, entry.bbox);
        insert_internal(best_child, entry);
        node->bbox = node->bbox.merge(entry.bbox);
    }
}

RTreeNode* RTree::choose_leaf(RTreeNode* node, const BoundingBox& bbox) {
    if (node->children.empty()) {
        return node;
    }
    
    RTreeNode* best = node->children[0].get();
    double min_enlargement = std::numeric_limits<double>::max();
    
    for (auto& child : node->children) {
        BoundingBox merged = child->bbox.merge(bbox);
        double enlargement = merged.area() - child->bbox.area();
        
        if (enlargement < min_enlargement) {
            min_enlargement = enlargement;
            best = child.get();
        }
    }
    
    return choose_leaf(best, bbox);
}

void RTree::split_node(RTreeNode* node) {
    // Simple split: divide entries in half
    if (node->entries.size() <= max_entries_) {
        return;
    }
    
    size_t mid = node->entries.size() / 2;
    
    // Create new sibling node
    auto sibling = std::make_unique<RTreeNode>(node->is_leaf);
    
    // Move half the entries to sibling
    sibling->entries.assign(
        node->entries.begin() + mid,
        node->entries.end()
    );
    node->entries.erase(
        node->entries.begin() + mid,
        node->entries.end()
    );
    
    // Recalculate bounding boxes
    node->bbox = BoundingBox();
    for (const auto& entry : node->entries) {
        node->bbox = node->bbox.merge(entry.bbox);
    }
    
    sibling->bbox = BoundingBox();
    for (const auto& entry : sibling->entries) {
        sibling->bbox = sibling->bbox.merge(entry.bbox);
    }
}

bool RTree::search(const std::string& key, size_t& offset) {
    return search_internal(root_.get(), key, offset);
}

bool RTree::search_internal(RTreeNode* node, const std::string& key, size_t& offset) {
    if (node->is_leaf) {
        for (const auto& entry : node->entries) {
            if (entry.key == key) {
                offset = entry.file_offset;
                return true;
            }
        }
        return false;
    }
    
    for (auto& child : node->children) {
        if (search_internal(child.get(), key, offset)) {
            return true;
        }
    }
    
    return false;
}

std::vector<RTreeEntry> RTree::range_search(const BoundingBox& query_box) {
    std::vector<RTreeEntry> results;
    range_search_internal(root_.get(), query_box, results);
    return results;
}

void RTree::range_search_internal(RTreeNode* node, const BoundingBox& query_box,
                                  std::vector<RTreeEntry>& results) {
    if (!node->bbox.intersects(query_box)) {
        return;
    }
    
    if (node->is_leaf) {
        for (const auto& entry : node->entries) {
            if (entry.bbox.intersects(query_box)) {
                results.push_back(entry);
            }
        }
    } else {
        for (auto& child : node->children) {
            range_search_internal(child.get(), query_box, results);
        }
    }
}

bool RTree::remove(const std::string& key) {
    // Simple implementation: mark as deleted in entries
    // In production, you'd need to rebalance the tree
    return false;  // Not implemented for simplicity
}

} // namespace storage
} // namespace simpledb
