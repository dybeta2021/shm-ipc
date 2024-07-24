//
// Created by Gavin on 2024/6/14.
//
// 统一数据类型为int32_t，控制单个page的大小

#pragma once

#include "spdlog/spdlog.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32

#include "io.h"
#include "mman.h"

#else
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <unistd.h>
#endif


class Page {
public:
    static constexpr int32_t KB = 1024;
    static constexpr int32_t MB = KB * KB;
    static constexpr int32_t GB = KB * MB;
    static constexpr int32_t PageMaxSize = 1 * GB;

    static bool RemoveFile(const std::string &path) {
        if (remove(path.c_str()) == -1) {
            SPDLOG_WARN("Failed to remove: {}, errno: {}", path, strerror(errno));
            return false;
        }
        SPDLOG_DEBUG("Remove: {}.", path);
        return true;
    }

    static bool CheckFileExists(const std::string &path) {
        bool exists = std::filesystem::exists(path);
        SPDLOG_INFO("File {} exists: {}.", path, exists ? "yes" : "no");
        return exists;
    }

private:
    std::string file_path_;
    bool write_mode_;
    void *data_ = nullptr;
    int32_t page_size_;


    bool GetShm() {
        int fd = write_mode_ ? open(file_path_.c_str(), O_RDWR | O_CREAT, 0666) : open(file_path_.c_str(), O_RDONLY);
        if (fd == -1) {
            SPDLOG_ERROR("Open error: {}", strerror(errno));
            return false;
        } else {
            SPDLOG_DEBUG("Open, fd: {}", fd);
        }

        // 获取文件大小
        struct stat st {};
        if (fstat(fd, &st) == -1) {
            SPDLOG_ERROR("Fstat error: {}", strerror(errno));
            close(fd);
            return false;
        }

        // 改变文件大小
        if (st.st_size == 0) {
#ifdef _WIN32
            if (_chsize(fd, static_cast<int32_t>(page_size_)) == 0) {
#else
            if (ftruncate(fd, (int32_t) page_size_) == 0) {
#endif

                SPDLOG_DEBUG("Ftruncate, file size:{}", page_size_);
            } else {
                SPDLOG_ERROR("Failed to ftruncate {}, size:{}, error:{}", file_path_, page_size_, strerror(errno));
                return false;
            }
        } else {
            SPDLOG_DEBUG("File exit,  path:{}, size:{}.", file_path_, st.st_size);
            if (st.st_size != page_size_) {
                SPDLOG_INFO("File exit,  path:{}, size:{}.", file_path_, st.st_size);
                page_size_ = st.st_size;
            }
        }

        if (write_mode_) {
            data_ = mmap(nullptr, page_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        } else {
            data_ = mmap(nullptr, page_size_, PROT_READ, MAP_PRIVATE, fd, 0);
        }

        if (data_ == MAP_FAILED) {
            SPDLOG_ERROR("Failed to mmap: {}, size: {}, errno: {}", file_path_, page_size_, strerror(errno));
            close(fd);
            return false;
        } else {
            close(fd);
            return true;
        }
    }

    bool DetachShm() {
        if (msync(data_, page_size_, MS_SYNC) != 0) {
            SPDLOG_ERROR("Failed to msync: {}, size: {}, errno: {}", file_path_, page_size_, strerror(errno));
            return false;
        }

        if (munmap(data_, page_size_) == -1) {
            SPDLOG_ERROR("Failed to munmap: {}, size: {}, errno: {}", file_path_, page_size_, strerror(errno));
            return false;
        }
        data_ = nullptr;
        return true;
    }

public:
    Page(const std::string &file_path, const bool &write_mode, const int32_t &page_size) {
        if (page_size > PageMaxSize) {
            SPDLOG_ERROR("Page max size:{}, page_size:{}.", PageMaxSize, page_size);
            return;
        }
        file_path_ = file_path;
        write_mode_ = write_mode;
        page_size_ = page_size;

        GetShm();
        SPDLOG_INFO("Page created, path: {}, write_mode: {}, page_size: {}.", file_path_, write_mode_, page_size_);
    }

    ~Page() {
        DetachShm();
        SPDLOG_INFO("Page destroyed, path: {}, write_mode: {}, page_size: {}.", file_path_, write_mode_, page_size_);
    }


    [[nodiscard]] void *GetShmDataAddress() const { return data_; }

    [[nodiscard]] const std::string &GetFilePath() const { return file_path_; }

    [[nodiscard]] int32_t GetSize() const { return page_size_; }
};


class Buffer {
private:
    void *data_ = nullptr;
    int32_t page_size_{};

public:
    explicit Buffer(const int32_t &page_size) {
        page_size_ = page_size;
    }

    ~Buffer() = default;

    bool GetShm() {
        data_ = mmap(nullptr, page_size_, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (data_ == MAP_FAILED) {
            SPDLOG_ERROR("ReaderBuffer Failed to size: {}, errno: {}", page_size_, strerror(errno));
            return false;
        } else {
            return true;
        }
    }

    void *GetShmDataAddress() {
        SPDLOG_DEBUG("GetShmDataAddress:{}", (void *) data_);
        return data_;
    }
};