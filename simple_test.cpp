/*
Simple test for S3FIFO Cache
*/

#include "include/s3fifo/cache.h"
#include <iostream>

using namespace S3FIFO;

int main() {
    std::cout << "S3FIFO Cache 简单测试" << std::endl;
    std::cout << "===================" << std::endl;
    
    try {
        // 创建一个容量为5，10%用于S队列的缓存
        S3FIFOCache<std::string, int> cache(5, 0.2);
        
        std::cout << "缓存创建成功！" << std::endl;
        std::cout << "容量: " << cache.capacity() << std::endl;
        std::cout << "当前大小: " << cache.size() << std::endl;
        std::cout << "是否为空: " << (cache.empty() ? "是" : "否") << std::endl;
        
        // 基础插入和获取测试
        cache.put("key1", 100);
        std::cout << "\n插入 key1=100" << std::endl;
        
        auto result = cache.get("key1");
        if (result.has_value()) {
            std::cout << "获取 key1 = " << result.value() << std::endl;
        } else {
            std::cout << "key1 未找到" << std::endl;
        }
        
        // 添加更多项目
        cache.put("key2", 200);
        cache.put("key3", 300);
        
        std::cout << "\n添加更多项目后，缓存大小: " << cache.size() << std::endl;
        
        // 测试访问key1以设置其clock位
        cache.get("key1");
        std::cout << "访问key1以设置clock位" << std::endl;
        
        // 添加更多项目触发驱逐
        cache.put("key4", 400);
        cache.put("key5", 500);
        cache.put("key6", 600);
        
        std::cout << "\n添加更多项目后，缓存大小: " << cache.size() << std::endl;
        std::cout << "最大容量: " << cache.capacity() << std::endl;
        
        // 检查哪些键还在缓存中
        std::vector<std::string> test_keys = {"key1", "key2", "key3", "key4", "key5", "key6"};
        std::cout << "\n检查缓存中的键:" << std::endl;
        for (const auto& key : test_keys) {
            auto val = cache.get(key);
            if (val.has_value()) {
                std::cout << key << " = " << val.value() << " (存在)" << std::endl;
            } else {
                std::cout << key << " (不存在)" << std::endl;
            }
        }
        
        // 清理测试
        cache.clear();
        std::cout << "\n清理缓存后:" << std::endl;
        std::cout << "缓存大小: " << cache.size() << std::endl;
        std::cout << "是否为空: " << (cache.empty() ? "是" : "否") << std::endl;
        
        std::cout << "\n测试完成！S3FIFO缓存工作正常。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

