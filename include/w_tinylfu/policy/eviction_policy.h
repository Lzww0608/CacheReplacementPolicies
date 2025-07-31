#ifndef INTERNAL_EVICTION_POLICY_H
#define INTERNAL_EVICTION_POLICY_H

#include "../../utils/intrusive_list.h"
#include "../../utils/node.h"

#include <cstdint>
#include <unordered_map>
#include <functional>
#include <string>

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
    void OnAccess(Node* n);
    /* 从窗口缓存升至 probation */
    void OnAdd(Node* n);

    /* 移除节点 */
    uint32_t EraseNode(Node* node);

    /* 公平竞争：候选数据 与 受害者*/
    uint32_t Compete(Node* candidate, Node* victim);
    /* 驱逐节点 */
    Node* Evict();

    /* 获取probation和protection大小 */
    uint64_t GetProbationSize() const;
    uint64_t GetProtectionSize() const;
    uint64_t GetSize() const;
    uint64_t GetCapacity() const;

    /* 判断节点是否存在于主缓存 */
    bool Contains(const K& key) const;
private:
    List probation_;
    List protection_;

    uint64_t probation_capacity_;
    uint64_t protection_capacity_;

    std::unordered_map<K, Node*> key_to_node_;
};

} // namespace w_tinylfu
} // namespace CRP
#endif
