//
// Created by Gavin on 2024/6/14.
//

#pragma once

#include "spdlog/spdlog.h"
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
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
    static constexpr int KB = 1024;
    static constexpr int MB = KB * KB;
    static constexpr int GB = KB * MB;
    static constexpr int PageMaxSize = 1 * GB;

    static bool RemoveFile(const std::string &path) {
        if (remove(path.c_str()) == -1) {
            SPDLOG_WARN("Failed to remove: {}, errno: {}", path, strerror(errno));
            return false;
        }
        SPDLOG_DEBUG("Remove: {}.", path);
        return true;
    }

private:
    std::string file_path_;
    bool write_mode_;
    void *data_ = nullptr;
    int32_t page_size_;

public:
    Page(const std::string &file_path, const bool &write_mode, const int32_t &page_size) {
        if (page_size > PageMaxSize) {
            SPDLOG_ERROR("Page max size:{}, page_size:{}.", PageMaxSize, page_size);
            return;
        }
        file_path_ = file_path;
        write_mode_ = write_mode;
        page_size_ = page_size;
        SPDLOG_DEBUG("Page created, path: {}, mode: {}, page_size: {}.", file_path_, write_mode_, page_size_);
    }

    ~Page() {
        DetachShm();
        SPDLOG_DEBUG("Page destroyed, path: {}, mode: {}, page_size: {}.", file_path_, write_mode_, page_size_);
    }

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
            if (_chsize(fd, (int64_t) page_size_) == 0) {
#else
            if (ftruncate(fd, (int64_t) page_size_) == 0) {
#endif

                SPDLOG_DEBUG("Ftruncate, file size:{}", page_size_);
            } else {
                SPDLOG_ERROR("Failed to ftruncate {}, size:{}, error:{}", file_path_, page_size_, strerror(errno));
                return false;
            }
        } else {
            SPDLOG_DEBUG("File exit,  path:{}, size:{}.", file_path_, st.st_size);
            if (st.st_size != page_size_) {
                SPDLOG_WARN("File exit,  path:{}, size:{}.", file_path_, st.st_size);
                page_size_ = st.st_size;
                //                    SPDLOG_ERROR("File exit,  path:{}, size:{}.", file_path_, st.st_size);
                //                    return false;
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

    bool RemoveShm() {
        return RemoveFile(file_path_);
    }

    void *GetShmDataAddress() const { return data_; }

    const std::string &GetFilePath() const { return file_path_; }

    int GetSize() const { return page_size_; }
};
