//
// Created by Gavin on 2024/6/12.
//
#include "logger.h"
#include "record.h"
#include <iostream>

//todo:如何释放共享内存映射的问题
typedef struct {
    char data[128];
    size_t th;
} TestBufferData;

int main(){
    create_logger("clogs/test.log", "trace", false, false, false, 1, 1);

    // page
    {
        auto page = Page("test.ts", true, 1024 * 1204);
        page.GetShm();
        auto file_path = page.GetFilePath();
        auto address = page.GetShmDataAddress();
        SPDLOG_INFO("file_path:{}", file_path);
        SPDLOG_INFO("address:{}", address);
        page.DetachShm();
        page.RemoveShm();
    }

    // write test
    {
        auto client = Record<TestBufferData>("test.ts", 1024, true, true);
        SPDLOG_INFO("start.");
        for (auto i = 0; i < 1024 + 2; i++) {
            TestBufferData t{};
            t.th = i;
            client.AddData(t);
        }
        client.Clear();
        SPDLOG_INFO("end.");
    }

    // read test
    {
        auto client = Record<TestBufferData>("test.ts", 1024, false, false);
        auto data = client.GetAllData();
        for (auto v: data) {
            std::cout << v->th << std::endl;
        }
    }

    return 0;
}

