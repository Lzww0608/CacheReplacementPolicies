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
    void on_access(Node* n);
    /* 从窗口缓存升至 probation */
    void on_add(Node* n);

    Node* get_victim();

    /* 公平竞争：候选数据 与 受害者*/
    uint32_t compete(Node* candidate, Node* victim);
    /* 驱逐节点 */
    Node* evict();

private:
    IntrusiveList<K, V> probation_;
    IntrusiveList<K, V> protected_;
    
    uint64_t probation_size_;
    uint64_t protected_size_;
};

} // namespace w_tinylfu
} // namespace CRP
#endif