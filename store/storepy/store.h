//
// Created by 稻草人 on 2024-07-22.
//

#pragma once
#include "page.h"
#include "spdlog/spdlog.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

/// store header
struct store_header {
    int32_t item_a_used;
    int32_t item_b_used;
    int32_t key_a_used;
    int32_t key_b_used;
    int32_t value_a_used;
    int32_t value_b_used;

    uint8_t current_item_id;
    uint8_t current_key_id;
    uint8_t current_value_id;
    uint8_t current_reader_id;

    int32_t item_max_used;
    int32_t key_max_used;
    int32_t value_max_used;
};

/// item store key address and value address
struct store_item {
    int32_t key_offset;
    int32_t key_len;
    int32_t value_offset;
    int32_t value_len;
};

/// key or value data
struct store_data {
    int32_t length;
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


class Store {
private:
    const int32_t KeyMaxSize = 128;
    // shm
    store_header *store_header_ = nullptr;
    store_data_address store_data_address_{};

    store_item *writer_item_ = nullptr;
    store_item *reader_item_ = nullptr;
    store_item *reader_item_buffer_ = nullptr;
    int32_t *writer_item_used_ = nullptr;
    int32_t *reader_item_used_ = nullptr;

    char *writer_key_ = nullptr;
    char *reader_key_ = nullptr;
    int32_t *writer_key_used_ = nullptr;
    int32_t *reader_key_used_ = nullptr;

    char *writer_value_ = nullptr;
    char *reader_value_ = nullptr;
    int32_t *writer_value_used_ = nullptr;
    int32_t *reader_value_used_ = nullptr;

    int32_t *item_max_used_ = nullptr;
    int32_t *key_max_used_ = nullptr;
    int32_t *value_max_used_ = nullptr;

    int32_t cache_used_{};
    Page *page_ = nullptr;
    Buffer *reader_buffer_ = nullptr;

private:
    static int key_compare(const void *a, const void *b) {
        // left key, right item-element
        auto cmp = (store_cmp *) a;
        auto item = (store_item *) b;
        auto key = cmp->key;
        const int32_t key_len = key->length + 1;

        if (key_len > item->key_len) {
            // move right
            SPDLOG_TRACE("left:{}, right :{}, offset:{}, length:{},  ret:{}", key->data,
                         cmp->key_address + item->key_offset, item->key_offset, item->key_len, 1);
            return 1;
        } else if (key_len < item->key_len) {
            //move left
            SPDLOG_TRACE("left:{}, right :{}, offset:{}, length:{},  ret:{}", key->data,
                         cmp->key_address + item->key_offset, item->key_offset, item->key_len, -1);
            return -1;
        } else {
            //注意这里，比较大小的位置设置
            auto ret = memcmp(key->data, cmp->key_address + item->key_offset, key->length);
            SPDLOG_TRACE("left:{}, right :{}, offset:{}, length:{},  ret:{}", key->data,
                         cmp->key_address + item->key_offset, item->key_offset, item->key_len, ret);
            return ret;
        }
    }

    int ResetKey(store_data &key) {
        // memory space size allow
        int32_t key_need = *writer_key_used_ + key.length + 1;
        if (key_need <= *key_max_used_) {
            SPDLOG_DEBUG("No need reset_key.");
            return 0;
        }

        // if still bigger than key_max_used return false
        key_need = key.length + 1;
        for (int32_t i = 0; i < *writer_item_used_; i++) {
            key_need += (writer_item_ + i)->key_len;
        }
        if (key_need >= *key_max_used_) {
            SPDLOG_ERROR("Key buffer memory size error, key_max_used: {}, need: {}", *writer_key_used_, key_need);
            return -1;
        }

        SPDLOG_DEBUG("ResetKey");
        // reset_key
        char *key_from_ = nullptr;
        char *key_to_ = nullptr;
        store_item *item = nullptr;
        if (store_header_->current_key_id == 0) {
            key_from_ = store_data_address_.key_a_address;
            key_to_ = store_data_address_.key_b_address;
        } else {
            key_from_ = store_data_address_.key_b_address;
            key_to_ = store_data_address_.key_a_address;
        }

        int32_t key_used = 0;
        for (int32_t i = 0; i < *writer_item_used_; i++) {
            item = writer_item_ + i;
            memcpy(key_to_ + key_used, key_from_ + item->key_offset, item->key_len);
            item->key_offset = key_used;
            key_used += item->key_len;
        }

        // change key id
        store_header_->current_key_id = store_header_->current_key_id == 0 ? 1 : 0;
        if (store_header_->current_key_id == 0) {
            writer_key_ = store_data_address_.key_a_address;
            writer_key_used_ = &store_header_->key_a_used;
        } else {
            writer_key_ = store_data_address_.key_b_address;
            writer_key_used_ = &store_header_->key_b_used;
        }
        *writer_key_used_ = key_used;
        SPDLOG_DEBUG("Reset key buffer.");
        return 0;
    }

    int ResetValue(store_data &value) {
        // memory space size allow
        int32_t value_need = *writer_value_used_ + value.length + 1;
        if (value_need <= *value_max_used_) {
            SPDLOG_DEBUG("No need reset_value.");
            return 0;
        }

        // if still bigger than value_max_used return false
        value_need = value.length + 1;
        for (int32_t i = 0; i < *writer_item_used_; i++) {
            value_need += (writer_item_ + i)->value_len;
        }
        if (value_need >= *value_max_used_) {
            SPDLOG_ERROR("Value buffer memory size error, value_max_used: {}, need: {}", *writer_value_used_,
                         value_need);
            return -1;
        }

        SPDLOG_DEBUG("ResetValue");
        // reset_value
        char *value_from_ = nullptr;
        char *value_to_ = nullptr;
        store_item *item = nullptr;
        if (store_header_->current_value_id == 0) {
            value_from_ = store_data_address_.value_a_address;
            value_to_ = store_data_address_.value_b_address;
        } else {
            value_from_ = store_data_address_.value_b_address;
            value_to_ = store_data_address_.value_a_address;
        }

        int32_t value_used = 0;
        for (int32_t i = 0; i < *writer_item_used_; i++) {
            item = writer_item_ + i;
            memcpy(value_to_ + value_used, value_from_ + item->value_offset, item->value_len);
            item->value_offset = value_used;
            value_used += item->value_len;
        }

        // change value id
        store_header_->current_value_id = store_header_->current_value_id == 0 ? 1 : 0;
        if (store_header_->current_value_id == 0) {
            writer_value_ = store_data_address_.value_a_address;
            writer_value_used_ = &store_header_->value_a_used;
        } else {
            writer_value_ = store_data_address_.value_b_address;
            writer_value_used_ = &store_header_->value_b_used;
        }
        *writer_value_used_ = value_used;

        SPDLOG_DEBUG("Reset value buffer.");
        return 0;
    }

    int AddItem(store_data &key, store_data &value) {
        if (*writer_item_used_ + 1 > *item_max_used_) {
            SPDLOG_ERROR("item_max_used_:{}, current_item_num:{}", *item_max_used_, *writer_item_used_);
            return -1;
        }

        if (ResetKey(key) != 0) {
            return -1;
        }
        if (ResetValue(value) != 0) {
            return -1;
        }

        // find the store_data_address_ to insert new item
        store_item *item = writer_item_;
        if (*writer_item_used_ > 0) {
            bool break_label = false;
            int32_t i;
            for (i = 0; i < *writer_item_used_; i++) {
                item = writer_item_ + i;
                if (item->key_len > key.length + 1) {
                    break_label = true;
                    break;
                } else if (item->key_len < key.length + 1) {
                } else {
                    if (memcmp(writer_key_ + item->key_offset, key.data, key.length) > 0) {
                        break_label = true;
                        break;
                    }
                }
            }

            //move bigger item to next item
            if (break_label) {
                i--;
                memcpy(writer_item_ + i + 1, writer_item_ + i, (*writer_item_used_ - i) * sizeof(store_item));
            }
            // end place append
            else {
                item += 1;
            }
        }
        item->key_len = key.length + 1;
        item->key_offset = *writer_key_used_;
        item->value_len = value.length + 1;
        item->value_offset = *writer_value_used_;

        memcpy(writer_key_ + item->key_offset, key.data, key.length);
        char *key_ = writer_key_ + item->key_offset + key.length;
        *key_ = '\0';

        memcpy(writer_value_ + item->value_offset, value.data, value.length);
        char *value_ = writer_value_ + item->value_offset + value.length;
        *value_ = '\0';

        *writer_item_used_ += 1;
        *writer_key_used_ += item->key_len;
        *writer_value_used_ += item->value_len;
        return 0;
    }

    int ReplaceItem(store_item *item, store_data &key, store_data &value) {
        if (ResetValue(value) != 0) {
            return -1;
        }
        item->value_len = value.length + 1;
        item->value_offset = *writer_value_used_;

        memcpy(writer_value_ + item->value_offset, value.data, value.length);
        char *value_ = writer_value_ + item->value_offset + value.length;
        *value_ = '\0';

        *writer_value_used_ += item->value_len;
        return 0;
    }

public:
    Store() = default;

    ~Store() {
        delete page_;
        delete reader_buffer_;
    }

    int Init(const std::string &file_path,
             const int32_t &item_num,
             const int32_t &value_size,
             const bool &write_mode,
             const bool &init_header,
             const bool &init_disk) {

        if (init_disk) {
            Page::RemoveFile(file_path);
        }

        // 约束最大内存空间，在不同系统泛用
        {
            int32_t header_size = sizeof(store_header);
            int32_t item_size = sizeof(store_item);
            int32_t k_size = KeyMaxSize;
            int32_t v_size = value_size;

            // value 4x min_memory_size for buffer
            int32_t shm_size = header_size + item_size * item_num * 2 + k_size * item_num * 2 + v_size * item_num * 2 * 2;

            SPDLOG_INFO("store size: {}.", shm_size);
            if (shm_size > Page::GB) {
                SPDLOG_ERROR("shm-kv size no more than {}, params is {}.", Page::GB, shm_size);
                return -1;
            }

            // 64*1024
            if (item_num > 65536) {
                SPDLOG_ERROR("shm-kv count-num no more than {}, params is {}.", 65536, item_num);
                return -2;
            }

            if (value_size > 16 * Page::MB) {
                SPDLOG_ERROR("shm-kv value-size no more than {}, params is {}.", 16 * Page::MB, value_size);
                return -3;
            }

            page_ = new Page(file_path, write_mode, shm_size);
        }

        store_header_ = (store_header *) page_->GetShmDataAddress();
        if (init_header) {
            store_header_->current_item_id = 0;
            store_header_->item_a_used = 0;
            store_header_->item_b_used = 0;
            store_header_->current_key_id = 0;
            store_header_->key_a_used = 0;
            store_header_->key_b_used = 0;
            store_header_->current_value_id = 0;
            store_header_->value_a_used = 0;
            store_header_->value_b_used = 0;
            store_header_->current_reader_id = 0;
            store_header_->item_max_used = item_num;
            store_header_->key_max_used = KeyMaxSize * item_num;
            store_header_->value_max_used = value_size * item_num * 2;

            // clean item memory
            memset((char *) store_header_ + sizeof(store_header), 0, sizeof(store_item) * item_num * 2);
        }
        store_data_address_.item_a_address = (store_item *) ((char *) store_header_ + sizeof(store_header));
        store_data_address_.item_b_address = (store_item *) store_data_address_.item_a_address + store_header_->item_max_used;
        store_data_address_.key_a_address = (char *) ((store_item *) store_data_address_.item_b_address + store_header_->item_max_used);
        store_data_address_.key_b_address = (char *) store_data_address_.key_a_address + store_header_->key_max_used;
        store_data_address_.value_a_address = (char *) store_data_address_.key_b_address + store_header_->key_max_used;
        store_data_address_.value_b_address = (char *) store_data_address_.value_a_address + store_header_->value_max_used;

        if (store_header_->item_max_used != item_num) {
            if (write_mode) {
                SPDLOG_ERROR("Writer Params Error. {}-{}", item_num, store_header_->item_max_used);
                return -5;
            } else {
                SPDLOG_WARN("Reader Params Different. {}-{}", item_num, store_header_->item_max_used);
            }
        }

        // address ptr
        item_max_used_ = &store_header_->item_max_used;
        key_max_used_ = &store_header_->key_max_used;
        value_max_used_ = &store_header_->value_max_used;

        // reader item buffer
        reader_buffer_ = new Buffer(static_cast<int32_t>(sizeof(store_item)) * (*item_max_used_));
        if (!reader_buffer_->GetShm()) {
            SPDLOG_ERROR("Buffer error!");
            return -5;
        }
        reader_item_buffer_ = (store_item *) reader_buffer_->GetShmDataAddress();

        return 0;
    }

    int Get(store_data &key, store_data &value) {
        const uint8_t reader_id = store_header_->current_reader_id;
        if (reader_id == 0) {
            reader_item_ = store_data_address_.item_a_address;
            reader_item_used_ = &store_header_->item_a_used;
            reader_key_ = store_data_address_.key_a_address;
            reader_key_used_ = &store_header_->key_a_used;
            reader_value_ = store_data_address_.value_a_address;
            reader_value_used_ = &store_header_->value_a_used;
        } else if (reader_id == 1) {
            reader_item_ = store_data_address_.item_a_address;
            reader_item_used_ = &store_header_->item_a_used;
            reader_key_ = store_data_address_.key_a_address;
            reader_key_used_ = &store_header_->key_a_used;
            reader_value_ = store_data_address_.value_b_address;
            reader_value_used_ = &store_header_->value_b_used;
        } else if (reader_id == 2) {
            reader_item_ = store_data_address_.item_a_address;
            reader_item_used_ = &store_header_->item_a_used;
            reader_key_ = store_data_address_.key_b_address;
            reader_key_used_ = &store_header_->key_b_used;
            reader_value_ = store_data_address_.value_a_address;
            reader_value_used_ = &store_header_->value_a_used;
        } else if (reader_id == 3) {
            reader_item_ = store_data_address_.item_a_address;
            reader_item_used_ = &store_header_->item_a_used;
            reader_key_ = store_data_address_.key_b_address;
            reader_key_used_ = &store_header_->key_b_used;
            reader_value_ = store_data_address_.value_b_address;
            reader_value_used_ = &store_header_->value_b_used;
        } else if (reader_id == 4) {
            reader_item_ = store_data_address_.item_b_address;
            reader_item_used_ = &store_header_->item_b_used;
            reader_key_ = store_data_address_.key_a_address;
            reader_key_used_ = &store_header_->key_a_used;
            reader_value_ = store_data_address_.value_a_address;
            reader_value_used_ = &store_header_->value_a_used;
        } else if (reader_id == 5) {
            reader_item_ = store_data_address_.item_b_address;
            reader_item_used_ = &store_header_->item_b_used;
            reader_key_ = store_data_address_.key_a_address;
            reader_key_used_ = &store_header_->key_a_used;
            reader_value_ = store_data_address_.value_b_address;
            reader_value_used_ = &store_header_->value_b_used;
        } else if (reader_id == 6) {
            reader_item_ = store_data_address_.item_b_address;
            reader_item_used_ = &store_header_->item_b_used;
            reader_key_ = store_data_address_.key_b_address;
            reader_key_used_ = &store_header_->key_b_used;
            reader_value_ = store_data_address_.value_a_address;
            reader_value_used_ = &store_header_->value_a_used;
        } else if (reader_id == 7) {
            reader_item_ = store_data_address_.item_b_address;
            reader_item_used_ = &store_header_->item_b_used;
            reader_key_ = store_data_address_.key_b_address;
            reader_key_used_ = &store_header_->key_b_used;
            reader_value_ = store_data_address_.value_b_address;
            reader_value_used_ = &store_header_->value_b_used;
        } else {
            SPDLOG_ERROR("current_reader_id:{}", reader_id);
            return -1;
        }
        SPDLOG_DEBUG("current_reader_id:{}, reader_item_used:{}", reader_id, *reader_item_used_);

        // fix-bug, create snapshot
        cache_used_ = *reader_item_used_;
        memcpy(reader_item_buffer_, reader_item_, cache_used_ * sizeof(store_item));

        store_cmp cmp_{};// wrapper
        cmp_.key_address = reader_key_;
        cmp_.key = &key;
        auto b_ptr = bsearch(&cmp_, reader_item_buffer_, cache_used_, sizeof(store_item), key_compare);
        SPDLOG_DEBUG("b-search b_ptr:{}", b_ptr);

        if (b_ptr == nullptr) {
            return -1;
        }

        auto *item = (store_item *) b_ptr;
        SPDLOG_DEBUG("get, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                     item->key_offset, item->key_len, item->value_offset, item->value_len);
        value.length = item->value_len;
        value.data = reader_value_ + item->value_offset;
        return 0;
    }

    int Set(store_data &key, store_data &value) {
        // create item snapshot
        // current_id:0, writer_item B, reader_item a
        // current_id:1, writer_item A, reader_item b
        uint8_t writer_item_id;
        if (store_header_->current_item_id == 0) {
            writer_item_ = store_data_address_.item_b_address;
            reader_item_ = store_data_address_.item_a_address;
            writer_item_used_ = &store_header_->item_b_used;
            reader_item_used_ = &store_header_->item_a_used;
            writer_item_id = 1;
        } else {
            writer_item_ = store_data_address_.item_a_address;
            reader_item_ = store_data_address_.item_b_address;
            writer_item_used_ = &store_header_->item_a_used;
            reader_item_used_ = &store_header_->item_b_used;
            writer_item_id = 0;
        }

        *writer_item_used_ = *reader_item_used_;
        memcpy(writer_item_, reader_item_, sizeof(store_item) * (*writer_item_used_));
        SPDLOG_DEBUG("current_item_id:{}, writer_item_id:{}, writer_item_used:{}", store_header_->current_item_id, writer_item_id,
                     *writer_item_used_);

        // key value pointer
        if (store_header_->current_key_id == 0) {
            writer_key_ = store_data_address_.key_a_address;
            writer_key_used_ = &store_header_->key_a_used;
        } else {
            writer_key_ = store_data_address_.key_b_address;
            writer_key_used_ = &store_header_->key_b_used;
        }
        if (store_header_->current_value_id == 0) {
            writer_value_ = store_data_address_.value_a_address;
            writer_value_used_ = &store_header_->value_a_used;
        } else {
            writer_value_ = store_data_address_.value_b_address;
            writer_value_used_ = &store_header_->value_b_used;
        }

        // b-search if exit replace else append
        store_cmp cmp_{};// wrapper
        cmp_.key_address = writer_key_;
        cmp_.key = &key;
        auto b_ptr = bsearch(&cmp_, writer_item_, *writer_item_used_, sizeof(store_item), key_compare);
        SPDLOG_DEBUG("b-search b_ptr:{}", b_ptr);

        int ret;
        if (b_ptr == nullptr) {
            SPDLOG_DEBUG("Append, key:{}, value:{}", key.data, value.data);
            ret = AddItem(key, value);
        } else {
            SPDLOG_DEBUG("Replace, key:{}, value:{}", key.data, value.data);
            ret = ReplaceItem((store_item *) b_ptr, key, value);
        }

        if (ret == 0) {
            // change reader-item
            store_header_->current_item_id = writer_item_id;
            if (store_header_->current_item_id == 0 && store_header_->current_key_id == 0 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 0;
            } else if (store_header_->current_item_id == 0 && store_header_->current_key_id == 0 && store_header_->current_value_id == 1) {
                store_header_->current_reader_id = 1;
            } else if (store_header_->current_item_id == 0 && store_header_->current_key_id == 1 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 2;
            } else if (store_header_->current_item_id == 0 && store_header_->current_key_id == 1 && store_header_->current_value_id == 1) {
                store_header_->current_reader_id = 3;
            } else if (store_header_->current_item_id == 1 && store_header_->current_key_id == 0 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 4;
            } else if (store_header_->current_item_id == 1 && store_header_->current_key_id == 0 && store_header_->current_value_id == 1) {
                store_header_->current_reader_id = 5;
            } else if (store_header_->current_item_id == 1 && store_header_->current_key_id == 1 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 6;
            } else {
                store_header_->current_reader_id = 7;
            }
        }
        return ret;
    }

    int Del(store_data &key) {
        // create item snapshot
        // current_id:0, writer_item B, reader_item a
        // current_id:1, writer_item A, reader_item b
        uint8_t writer_item_id;
        if (store_header_->current_item_id == 0) {
            writer_item_ = store_data_address_.item_b_address;
            reader_item_ = store_data_address_.item_a_address;
            writer_item_used_ = &store_header_->item_b_used;
            reader_item_used_ = &store_header_->item_a_used;
            writer_item_id = 1;
        } else {
            writer_item_ = store_data_address_.item_a_address;
            reader_item_ = store_data_address_.item_b_address;
            writer_item_used_ = &store_header_->item_a_used;
            reader_item_used_ = &store_header_->item_b_used;
            writer_item_id = 0;
        }
        *writer_item_used_ = *reader_item_used_;
        memcpy(writer_item_, reader_item_, sizeof(store_item) * (*writer_item_used_));
        SPDLOG_DEBUG("current_item_id:{}, writer_item_id:{}, writer_item_used:{}", store_header_->current_item_id, writer_item_id,
                     *writer_item_used_);

        // key value pointer
        if (store_header_->current_key_id == 0) {
            writer_key_ = store_data_address_.key_a_address;
            writer_key_used_ = &store_header_->key_a_used;
        } else {
            writer_key_ = store_data_address_.key_b_address;
            writer_key_used_ = &store_header_->key_b_used;
        }
        if (store_header_->current_value_id == 0) {
            writer_value_ = store_data_address_.value_a_address;
            writer_value_used_ = &store_header_->value_a_used;
        } else {
            writer_value_ = store_data_address_.value_b_address;
            writer_value_used_ = &store_header_->value_b_used;
        }

        // b-search if exit replace else append
        store_cmp cmp_{};// wrapper
        cmp_.key_address = writer_key_;
        cmp_.key = &key;
        auto b_ptr = bsearch(&cmp_, writer_item_, *writer_item_used_, sizeof(store_item), key_compare);
        SPDLOG_DEBUG("b-search b_ptr:{}", b_ptr);

        if (b_ptr == nullptr) {
            SPDLOG_DEBUG("Not found key: {}-{}", key.length, key.data);
            return -1;
        } else {
            SPDLOG_DEBUG("Delete, key:{}, value:{}", key.length, key.data);
            auto *item = (store_item *) b_ptr;
            // not the last item
            if (item < writer_item_ + *writer_item_used_ - 1) {
                memcpy(item, item + 1, sizeof(store_item) * (*writer_item_used_ + 1 - (item - writer_item_)));
            }
            // last item,just sub used_num
            *writer_item_used_ -= 1;

            store_header_->current_item_id = writer_item_id;
            if (store_header_->current_item_id == 0 && store_header_->current_key_id == 0 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 0;
            } else if (store_header_->current_item_id == 0 && store_header_->current_key_id == 0 && store_header_->current_value_id == 1) {
                store_header_->current_reader_id = 1;
            } else if (store_header_->current_item_id == 0 && store_header_->current_key_id == 1 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 2;
            } else if (store_header_->current_item_id == 0 && store_header_->current_key_id == 1 && store_header_->current_value_id == 1) {
                store_header_->current_reader_id = 3;
            } else if (store_header_->current_item_id == 1 && store_header_->current_key_id == 0 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 4;
            } else if (store_header_->current_item_id == 1 && store_header_->current_key_id == 0 && store_header_->current_value_id == 1) {
                store_header_->current_reader_id = 5;
            } else if (store_header_->current_item_id == 1 && store_header_->current_key_id == 1 && store_header_->current_value_id == 0) {
                store_header_->current_reader_id = 6;
            } else {
                store_header_->current_reader_id = 7;
            }

            return 0;
        }
    }

    std::vector<std::string> GetCurrentAllKeys() {
        std::vector<std::string> out;

        if (store_header_->current_item_id == 0) {
            SPDLOG_INFO("item-a");
            for (int i = 0; i < store_header_->item_a_used; i++) {
                auto *item = (store_item *) store_data_address_.item_a_address + i;
                auto *key_buffer =
                        store_header_->current_key_id == 0 ? store_data_address_.key_a_address : store_data_address_.key_b_address;
                char tmp[128] = {0};
                memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                std::string key_str(tmp);
                out.push_back(key_str);
            }
        } else {
            SPDLOG_INFO("item-b");
            for (int i = 0; i < store_header_->item_b_used; i++) {
                auto *item = (store_item *) store_data_address_.item_b_address + i;
                auto *key_buffer =
                        store_header_->current_key_id == 0 ? store_data_address_.key_a_address : store_data_address_.key_b_address;
                char tmp[128] = {0};
                memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                std::string key_str(tmp);
                out.push_back(key_str);
            }
        }
        return out;
    }

public:
    void ResetValue() {
        SPDLOG_INFO("ResetValue");
        ShowHeader();
        ShowAllKey();
        // reset_value
        char *value_from_ = nullptr;
        char *value_to_ = nullptr;
        store_item *item = nullptr;
        if (store_header_->current_value_id == 0) {
            value_from_ = store_data_address_.value_a_address;
            value_to_ = store_data_address_.value_b_address;
        } else {
            value_from_ = store_data_address_.value_b_address;
            value_to_ = store_data_address_.value_a_address;
        }

        *writer_value_used_ = 0;
        for (int32_t i = 0; i < *writer_item_used_; i++) {
            item = writer_item_ + i;
            memcpy(value_to_ + *writer_value_used_, value_from_ + item->value_offset, item->value_len);
            item->value_offset = *writer_value_used_;
            *writer_value_used_ += item->value_len;
        }

        // change id
        store_header_->current_value_id = store_header_->current_value_id == 0 ? 1 : 0;
        // change value id
        store_header_->current_value_id = store_header_->current_value_id == 0 ? 1 : 0;
        if (store_header_->current_value_id == 0) {
            writer_value_ = store_data_address_.value_a_address;
            writer_value_used_ = &store_header_->value_a_used;
        } else {
            writer_value_ = store_data_address_.value_b_address;
            writer_value_used_ = &store_header_->value_b_used;
        }
        SPDLOG_INFO("Reset value buffer.");
        ShowHeader();
        ShowAllKey();
    }

    void ShowHeader() {
        SPDLOG_INFO("store_header-current_item_id:{}", store_header_->current_item_id);
        SPDLOG_INFO("store_header-current_key_id:{}", store_header_->current_key_id);
        SPDLOG_INFO("store_header-current_value_id:{}", store_header_->current_value_id);
        SPDLOG_INFO("store_header-item_a_used:{}", store_header_->item_a_used);
        SPDLOG_INFO("store_header-item_b_used:{}", store_header_->item_b_used);
        SPDLOG_INFO("store_header-key_a_used:{}", store_header_->key_a_used);
        SPDLOG_INFO("store_header-key_b_used:{}", store_header_->key_b_used);
        SPDLOG_INFO("store_header-value_a_used:{}", store_header_->value_a_used);
        SPDLOG_INFO("store_header-value_b_used:{}", store_header_->value_b_used);
        SPDLOG_INFO("store_header-item_max_used_:{}", store_header_->item_max_used);
        SPDLOG_INFO("store_header: {}", (void *) store_header_);
        SPDLOG_INFO("store_header-item_a_address: {}", (void *) store_data_address_.item_a_address);
        SPDLOG_INFO("store_header-item_b_address: {}", (void *) store_data_address_.item_b_address);
        SPDLOG_INFO("store_header-key_a_address: {}", (void *) store_data_address_.key_a_address);
        SPDLOG_INFO("store_header-key_b_address: {}", (void *) store_data_address_.key_b_address);
        SPDLOG_INFO("store_header-value_a_address: {}", (void *) store_data_address_.value_a_address);
        SPDLOG_INFO("store_header-value_b_address: {}", (void *) store_data_address_.value_b_address);
    }

    void ShowCurrentKey() {
        if (store_header_->current_item_id == 0) {
            SPDLOG_INFO("item-a");
            for (int i = 0; i < store_header_->item_a_used; i++) {
                auto *item = (store_item *) store_data_address_.item_a_address + i;
                auto *key_buffer =
                        store_header_->current_key_id == 0 ? store_data_address_.key_a_address : store_data_address_.key_b_address;
                char tmp[128] = {0};
                memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                            i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
            }
        } else {
            SPDLOG_INFO("item-b");
            for (int i = 0; i < store_header_->item_b_used; i++) {
                auto *item = (store_item *) store_data_address_.item_b_address + i;
                auto *key_buffer =
                        store_header_->current_key_id == 0 ? store_data_address_.key_a_address : store_data_address_.key_b_address;
                char tmp[128] = {0};
                memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
                SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                            i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
            }
        }
    }

    void ShowAllKey() {
        SPDLOG_INFO("current_item_id:{}, current_key_id:{}, current_value_id:{}", store_header_->current_item_id, store_header_->current_key_id, store_header_->current_value_id);
        SPDLOG_INFO("item-a");
        for (int i = 0; i < store_header_->item_a_used; i++) {
            auto *item = (store_item *) store_data_address_.item_a_address + i;
            auto *key_buffer = store_header_->current_key_id == 0 ? store_data_address_.key_a_address : store_data_address_.key_b_address;
            char tmp[128] = {0};
            memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
            SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                        i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
        }
        SPDLOG_INFO("item-b");
        for (int i = 0; i < store_header_->item_b_used; i++) {
            auto *item = (store_item *) store_data_address_.item_b_address + i;
            auto *key_buffer = store_header_->current_key_id == 0 ? store_data_address_.key_a_address : store_data_address_.key_b_address;
            char tmp[128] = {0};
            memcpy(&tmp, key_buffer + item->key_offset, item->key_len);
            SPDLOG_INFO("item:{}, key:{}, key_offset:{}, key_len:{}, value_offset:{}, value_len:{}",
                        i, tmp, item->key_offset, item->key_len, item->value_offset, item->value_len);
        }
    }
};
