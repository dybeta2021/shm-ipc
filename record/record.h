//
// Created by Gavin on 2024/6/12.
//
#pragma once

#include <cerrno>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <atomic>
#include <stdexcept>

#ifdef _WIN32

#include <windows.h>
#include <io.h>

#else
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>
#endif

#include "spdlog/spdlog.h"

class Page {
public:
    static constexpr int KB = 1024;
    static constexpr int MB = KB * KB;
    static constexpr int GB = KB * MB;

private:
    std::string file_path_;
    bool write_mode_;
    void *data_ = nullptr;
    int page_size_;

public:
    Page(std::string file_path, bool write_mode, int page_size)
            : file_path_(std::move(file_path)), write_mode_(write_mode), page_size_(page_size) {
        SPDLOG_DEBUG("Page created, path: {}, mode: {}, page_size: {}.", file_path_, write_mode_, page_size_);
    }

    ~Page() {
        DetachShm();
        SPDLOG_DEBUG("Page destroyed, path: {}, mode: {}, page_size: {}.", file_path_, write_mode_, page_size_);
    }

    static bool RemoveFile(const std::string &path) {
#ifdef _WIN32
        if (_unlink(path.c_str()) == -1) {
#else
            if (remove(path.c_str()) == -1) {
#endif
            SPDLOG_WARN("Failed to remove: {}, errno: {}", path, strerror(errno));
            return false;
        }
        SPDLOG_DEBUG("File removed: {}.", path);
        return true;
    }

    bool GetShm() {
#ifdef _WIN32
        HANDLE file_handle = write_mode_ ? CreateFileA(file_path_.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                                       CREATE_ALWAYS,
                                                       FILE_ATTRIBUTE_NORMAL, nullptr)
                                         : CreateFileA(file_path_.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                                       FILE_ATTRIBUTE_NORMAL, nullptr);

        if (file_handle == INVALID_HANDLE_VALUE) {
            SPDLOG_ERROR("Open error: {}", GetLastError());
            return false;
        }

        HANDLE mapping_handle = CreateFileMappingA(file_handle, nullptr, write_mode_ ? PAGE_READWRITE : PAGE_READONLY,
                                                   0,
                                                   page_size_, nullptr);

        if (mapping_handle == nullptr) {
            SPDLOG_ERROR("CreateFileMapping error: {}", GetLastError());
            CloseHandle(file_handle);
            return false;
        }

        data_ = MapViewOfFile(mapping_handle, write_mode_ ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ, 0, 0, page_size_);

        if (data_ == nullptr) {
            SPDLOG_ERROR("MapViewOfFile error: {}", GetLastError());
            CloseHandle(mapping_handle);
            CloseHandle(file_handle);
            return false;
        }

        CloseHandle(mapping_handle);
        CloseHandle(file_handle);
        return true;
#else
        int fd = open(file_path_.c_str(), write_mode_ ? (O_RDWR | O_CREAT) : O_RDONLY, 0666);

        if (fd == -1) {
            SPDLOG_ERROR("Open error: {}", strerror(errno));
            return false;
        }

        struct stat st {};
        if (fstat(fd, &st) == -1) {
            SPDLOG_ERROR("Fstat error: {}", strerror(errno));
            close(fd);
            return false;
        }

        if (st.st_size == 0) {
            if (ftruncate(fd, page_size_) == -1) {
                SPDLOG_ERROR("Ftruncate error for {}: size: {}, errno: {}", file_path_, page_size_, strerror(errno));
                close(fd);
                return false;
            }
            SPDLOG_DEBUG("Ftruncate success, file size: {}.", page_size_);
        } else if (st.st_size != page_size_) {
            SPDLOG_ERROR("File size mismatch: path: {}, expected size: {}, actual size: {}.", file_path_, page_size_, st.st_size);
            close(fd);
            return false;
        }

        data_ = mmap(nullptr, page_size_, write_mode_ ? PROT_READ | PROT_WRITE : PROT_READ, write_mode_ ? MAP_SHARED : MAP_PRIVATE, fd, 0);
        if (data_ == MAP_FAILED) {
            SPDLOG_ERROR("Mmap error for {}: size: {}, errno: {}", file_path_, page_size_, strerror(errno));
            close(fd);
            return false;
        }

        close(fd);
        return true;
#endif
    }

    bool DetachShm() {
#ifdef _WIN32
        if (data_ != nullptr && !UnmapViewOfFile(data_)) {
            SPDLOG_ERROR("UnmapViewOfFile error: {}", GetLastError());
            return false;
        }
#else
        if (data_ != nullptr && munmap(data_, page_size_) == -1) {
            SPDLOG_ERROR("Munmap error for {}: size: {}, errno: {}", file_path_, page_size_, strerror(errno));
            return false;
        }
#endif
        data_ = nullptr;
        return true;
    }

    bool RemoveShm() {
        return RemoveFile(file_path_);
    }

    void *GetShmDataAddress() const { return data_; }

    const std::string &GetFilePath() const { return file_path_; }

    int GetSize() const { return page_size_; }
};

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
            content_.resize(capacity); // resize to match capacity
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

    int AddData(const T &data) {
        if (bookmark_ptr_->cursor >= bookmark_ptr_->item_num) {
            SPDLOG_ERROR("Memory overflow error!");
            return -1;
        }

        *content_[bookmark_ptr_->cursor] = data;
        ++bookmark_ptr_->cursor;
        return 0;
    }

    T *GetData(size_t idx) const {
        if (idx >= bookmark_ptr_->item_num) {
            return nullptr;
        }
        return content_[idx];
    }

    std::vector<T> GetAllData() const {
        std::vector<T> cache;
        cache.reserve(bookmark_ptr_->cursor);
        for (size_t i = 0; i < bookmark_ptr_->cursor; ++i) {
            cache.push_back(*content_[i]);
        }
        return cache;
    }
};
