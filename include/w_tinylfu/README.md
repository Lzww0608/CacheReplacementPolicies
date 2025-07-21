# W-TinyLFU 缓存淘汰策略

## 项目架构目录
w_tinylfu_cache/
├── CMakeLists.txt
├── include/
│   └── tinylfu/
│       ├── api/
│       │   ├── cache.h
│       │   └── loading_cache.h
│       ├── policy/
│       │   ├── eviction_policy.h
│       │   └── expiration_policy.h
│       └── sketch/
│           └── frequency_sketch.h
├── src/
│   ├── core/
│   │   ├── cache_builder.cpp
│   │   └── w_tiny_lfu_cache.cpp
│   ├── policy/
│   │   ├── segmented_lru_policy.cpp
│   │   └── timer_wheel.cpp
│   ├── sketch/
│   │   └── frequency_sketch.cpp
│   └── concurrent/
│       ├── read_buffer.cpp
│       └── maintenance_task.cpp
├── internal/
│   ├── node.h
│   ├── hashing.h
│   └── intrusive_list.h
└── tests/
    ├── policy/
    │   └── test_slru_policy.cpp
    └── sketch/
        └── test_frequency_sketch.cpp

## Loading Cache窗口缓存策略
窗口缓存作为“准入门槛”，在新数据初次仅此TinyLFU时存入窗口缓存，避免一次性数据直接进入主缓存。窗口缓存采用LRU策略，当窗口缓存满时淘汰最久未被访问的数据。一般而言，我们默认将窗口缓存的大小设置为主缓存的“1%”（该项数值将被设置为在conf中作为输入参数设定）。

### 缓存逻辑
#### 1. 通过LRU管理存活时间
+ 数据被插入或访问时按照LRU逻辑更新
+ 窗口缓存满时，将淘汰**最久未被访问**的数据，该数据作为**候选数据**
    + 当主缓存的`probation`部分未满，直接插入**候选数据**
    + 当主缓存的`probation`部分已满，**候选数据**与**受害者数据**在TinyLFU过滤器竞争。（见eviction_policy部分）
 
#### 2. 关键逻辑
+ **访问次数** ≠ **淘汰条件**：窗口缓存的淘汰机制仅与LRU有关，而非LFU的访问次数。
+ **频率统计的延迟性**：数据的访问频率由`Count-Min Sketch`统计，但频率更新仅在数据进入主缓存的`probation`之后才会触发。窗口缓存中的数据即使被多次访问，其频率也不会计入到主缓存的数据频率中，因此无法通过频率竞争进入`probation`。
+ **`probation`的准入门槛**：`probation`的设计目的是筛选“存活超过窗口的候选数据”，而非直接接纳窗口中的高频数据。数据必须被窗口缓存淘汰，才能获得进入观察区的资格。
+ **候选数据的频率来源**：候选数据的频率是其在窗口缓存中被淘汰前的**累计访问次数**，而非仅窗口内的访问次数。例如，若数据在窗口中被访问3次后被淘汰，其频率为3。进入观察区之后若再次被访问，频率会继续累加。


## CMS —— Count-Min Sketch
Count-Min Sketch（CMS）是 W-TinyLFU 中用于高效估计键访问频率的核心组件，所谓`sketch`就是用很少的一点数据来描述全体数据的特性，牺牲了准确性但是代价变得很低。。

### 设计核心逻辑
+ 用多个**哈希函数**将键映射到一个二维数组（计数器矩阵），通过多个位置的计数近似键的真实访问频率。
