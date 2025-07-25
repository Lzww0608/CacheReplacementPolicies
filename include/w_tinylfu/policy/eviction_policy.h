#ifndef INTERNAL_EVICTION_POLICY_H
#define INTERNAL_EVICTION_POLICY_H

#include "../../utils/intrusive_list.h"
#include "../../utils/node.h"

#include <cstdint>

namespace CRP {
namespace w_tinylfu {


template <typename K, typename V>
class SLRU {
public:
    using Node = ::Node<K, V>;

    virtual ~SLRU() = default;
    /* 从probation 升至 protected */
    void onAccess(Node* n);
    /* 从窗口缓存升至 probation */
    void onAdd(Node* n);

    /* 移除节点 */
    uint32_t erase(Node* node);

    /* 获取victime节点 */
    Node* getVictim();

    /* 公平竞争：候选数据 与 受害者*/
    uint32_t compete(Node* candidate, Node* victim);
    /* 驱逐节点 */
    Node* evict();

    /* 获取probation和protection大小 */
    uint64_t getProbationSize() const;
    uint64_t getProtectionSize() const;

private:
    IntrusiveList<K, V> probation_;
    IntrusiveList<K, V> protection_;
    
    uint64_t probation_size_;
    uint64_t protection_size_;
};

} // namespace w_tinylfu
} // namespace CRP
#endif
