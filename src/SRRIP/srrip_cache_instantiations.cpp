/*
@Author: Lzww
@LastEditTime: 2025-8-14 17:30:00
@Description: SRRIP缓存模板实例化
@Language: C++17
*/

#include "../../include/SRRIP/srrip_cache.h"
#include "../../include/SRRIP/cache_set.h"
#include "srrip_cache.cpp"
#include "cache_set.cpp"

// 显式实例化常用配置
template class SRRIP::SRRIPCache<2>;  // 4个RRPV值 (0,1,2,3)
template class SRRIP::SRRIPCache<3>;  // 8个RRPV值 (0,1,2,3,4,5,6,7)
template class SRRIP::SRRIPCache<4>;  // 16个RRPV值

template class SRRIP::CacheSet<2>;
template class SRRIP::CacheSet<3>;
template class SRRIP::CacheSet<4>;
