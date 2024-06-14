//
// Created by Gavin on 2024/6/12.
//
#pragma once
#include "page.h"
#include <vector>

struct Bookmark {
    size_t item_num;
    std::atomic<size_t> cursor;
};

template<typename T>
class Record {
private:
    size_t capacity_ = 0;
    std::vector<T *> content_;
    Bookmark *bookmark_ptr_ = nullptr;
    std::unique_ptr<Page> page_ptr_;

    bool SetCapacity(size_t capacity) {
        try {
            content_.reserve(capacity);
            content_.resize(capacity);// resize to match capacity
        } catch (const std::length_error &e) {
            SPDLOG_ERROR("SetCapacity error: {}.", e.what());
            return false;
        }
        capacity_ = capacity;
        return true;
    }

    bool Init(const std::string &file_path, size_t item_num, bool writer, bool init) {
        if (!SetCapacity(item_num)) {
            return false;
        }

        if (init) {
            Page::RemoveFile(file_path);
        }

        size_t page_size = sizeof(Bookmark) + item_num * sizeof(T);
        page_ptr_ = std::make_unique<Page>(file_path, writer, static_cast<int>(page_size));
        if (!page_ptr_->GetShm()) {
            return false;
        }

        bookmark_ptr_ = static_cast<Bookmark *>(page_ptr_->GetShmDataAddress());
        if (init) {
            bookmark_ptr_->item_num = item_num;
            bookmark_ptr_->cursor = 0;
        }

        char *base_ptr = static_cast<char *>(page_ptr_->GetShmDataAddress()) + sizeof(Bookmark);
        for (size_t i = 0; i < item_num; ++i) {
            content_[i] = reinterpret_cast<T *>(base_ptr + i * sizeof(T));
        }

        return true;
    }

public:
    Record(const std::string &file_path, size_t item_num, bool writer, bool init)
        : page_ptr_(nullptr) {
        Init(file_path, item_num, writer, init);
    }

    void Clear() {
        bookmark_ptr_->cursor = 0;
    }

    int AddData(const T &data) {
        if (bookmark_ptr_->cursor >= bookmark_ptr_->item_num) {
            SPDLOG_ERROR("Memory overflow error!");
            return -1;
        }

        *content_[bookmark_ptr_->cursor] = data;
        ++bookmark_ptr_->cursor;
        return 0;
    }

    //    T *GetData(size_t idx) const {
    //        if (idx >= bookmark_ptr_->item_num) {
    //            return nullptr;
    //        }
    //        return content_[idx];
    //    }

    /// 返回包含指向所有数据的指针的向量
    std::vector<T *> GetAllData() const {
        std::vector<T *> cache;
        cache.reserve(bookmark_ptr_->cursor);
        for (size_t i = 0; i < bookmark_ptr_->cursor; ++i) {
            cache.push_back(content_[i]);
        }
        return cache;
    }
};
