#ifndef INTERNAL_EVICTION_POLICY_H
#define INTERNAL_EVICTION_POLICY_H

#include "../../utils/intrusive_list.h"
#include "../../utils/node.h"

#include <cstdint>
#include <unordered_map>
#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>

namespace CRP {
namespace w_tinylfu {

template <typename K, typename V, typename Hash = std::hash<std::string>>
class SLRU {
public:
    using Node = ::Node<K, V>;
    using List = CRP::IntrusiveList<K, V>;

    SLRU(uint64_t probation_capacity, uint64_t protection_capacity);
    virtual ~SLRU() = default;
    /* 从probation 升至 protected */
    void onAccess(Node* n);
    /* 从窗口缓存升至 probation */
    void onAdd(Node* n);

    /* 移除节点 */
    uint32_t eraseNode(Node* node);

    /* 公平竞争：候选数据 与 受害者*/
    uint32_t compete(Node* candidate, Node* victim);
    /* 驱逐节点 */
    Node* evict();

    bool get(const K& key, V& value);

    void put(const K& key, const V& value);

    /* 获取probation和protection大小 */
    uint64_t probationSize() const;
    uint64_t protectionSize() const;
    uint64_t size() const;
    uint64_t capacity() const;

    /* 判断节点是否存在于主缓存 */
    bool contains(const K& key) const;

    /* protected_ decay*/
    void decay_all_frequencies(double factor);
private:
    List probation_;
    List protection_;

    uint64_t probation_capacity_;
    uint64_t protection_capacity_;

    std::unordered_map<K, Node*> key_to_node_;

    std::shared_mutex probation_mutex_;
    std::shared_mutex protection_mutex_;
};

} // namespace w_tinylfu
} // namespace CRP
#endif
