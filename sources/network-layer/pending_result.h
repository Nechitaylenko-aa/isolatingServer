//
// Created by artem on 14.08.26.
//

#ifndef APP_SERVERD_PENDING_RESULT_H
#define APP_SERVERD_PENDING_RESULT_H

#include <cstdint>
#include <vector>

struct SPendingResult
{
    uint32_t              worker_id{0};
    uint32_t              request_id{0};
    std::vector<uint8_t>  responseBytes;
};

#endif //APP_SERVERD_PENDING_RESULT_H
