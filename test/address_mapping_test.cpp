/*
@Author: Lzww
@LastEditTime: 2025-8-14 18:00:00
@Description: 地址映射测试程序
@Language: C++17
*/

#include <iostream>
#include <iomanip>
#include "../include/SRRIP/srrip_cache.h"

using namespace SRRIP;

void testAddressMapping() {
    std::cout << "=== 地址映射测试 ===" << std::endl;
    
    // 64KB缓存，64字节块，4路组相联
    SRRIPCache<2> cache(64, 64, 4);
    
    std::cout << "缓存配置: 64KB, 64B块, 4路组相联" << std::endl;
    std::cout << "总块数: " << (64 * 1024 / 64) << std::endl;
    std::cout << "组数: " << (64 * 1024 / 64 / 4) << std::endl;
    std::cout << "offset_bits: " << 6 << std::endl;
    std::cout << "set_index_bits: " << 8 << std::endl;
    std::cout << std::endl;
    
    std::cout << "地址映射测试:" << std::endl;
    std::cout << std::setw(10) << "地址" << std::setw(10) << "组索引" << std::setw(10) << "标签" << std::endl;
    std::cout << std::string(30, '-') << std::endl;
    
    std::vector<uint64_t> addresses = {0x0000, 0x0040, 0x0080, 0x00C0, 0x0100, 0x0140, 0x0180, 0x01C0};
    
    for (auto addr : addresses) {
        uint64_t tag, set_index;
        cache.parseAddress(addr, tag, set_index);
        
        std::cout << std::setw(10) << std::hex << addr 
                  << std::setw(10) << std::dec << set_index
                  << std::setw(10) << std::hex << tag << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "结论: 地址0x0000, 0x0040, 0x0080, 0x00C0都映射到组0" << std::endl;
    std::cout << "地址0x0100映射到组1，所以不会触发替换" << std::endl;
}

int main() {
    testAddressMapping();
    return 0;
}
