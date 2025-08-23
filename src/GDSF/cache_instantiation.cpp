/*
@Author: Lzww  
@LastEditTime: 2025-8-23 18:30:00
@Description: Template instantiation for GDSF Cache
@Language: C++17
*/

#include "cache.cpp"

namespace GDSF {
    // 显式实例化常用的模板类型
    template class GDSFCache<std::string, std::string, std::hash<std::string>>;
    template class GDSFCache<int, std::string, std::hash<int>>;
    template class GDSFCache<std::string, int, std::hash<std::string>>;
}
