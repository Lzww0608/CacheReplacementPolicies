


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