//
// Created by 稻草人 on 2022/8/5.
//

#ifndef SPMC_SHMKV_DATA_H
#define SPMC_SHMKV_DATA_H

#include <cstddef>
#include <cstdint>

namespace ots::store {
    constexpr int KB = 1024;
    constexpr int MB = KB * KB;
    constexpr int GB = KB * MB;
    constexpr int KeyMaxSize = 128;
    constexpr int PageMaxSize = 1024 * MB;

    /// store header
    struct store_header {
        size_t item_a_used;
        size_t item_b_used;
        size_t key_a_used;
        size_t key_b_used;
        size_t value_a_used;
        size_t value_b_used;

        uint8_t item_id;
        uint8_t key_id;
        uint8_t value_id;
        uint8_t reader_id;

        size_t item_max_used;
        size_t key_max_used;
        size_t value_max_used;
    };

    /// item store key address and value address
    struct store_item {
        size_t key_offset;
        size_t key_len;
        size_t value_offset;
        size_t value_len;
    };

    /// key or value data
    struct store_data {
        size_t length;
        char *data;
    };

    /// different mode memory address
    struct store_data_address {
        store_item *item_a_address;
        store_item *item_b_address;
        char *key_a_address;
        char *key_b_address;
        char *value_a_address;
        char *value_b_address;
    };

    struct store_cmp {
        char *key_address;
        store_data *key;
    };
}// namespace ots::store

#endif//SPMC_SHMKV_DATA_H
