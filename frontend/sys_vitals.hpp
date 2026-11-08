// [LOCAL-FIX friday / status-endpoint] Windows commit / RAM snapshot for GET /status.
//
// The engine prints `strata serve: Windows commit: ...` exactly once, at load, so it can never be a
// live field. This samples the same numbers Windows gives Task Manager, at request time, with no
// dependency beyond the OS: GetPerformanceInfo reports in PAGES, GlobalMemoryStatusEx in bytes.
#pragma once

#include <cstdint>

namespace sys_vitals {

struct Snapshot {
    bool     valid           = false;
    uint64_t page_size       = 0;
    uint64_t commit_total    = 0;   // bytes currently committed
    uint64_t commit_limit    = 0;   // bytes addressable = RAM + page file
    uint64_t ram_total       = 0;
    uint64_t ram_avail       = 0;
    uint64_t commit_headroom = 0;   // commit limit - committed (= ullAvailPageFile)
};

Snapshot sample();

}   // namespace sys_vitals
