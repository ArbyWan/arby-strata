// [LOCAL-FIX friday / status-endpoint] see sys_vitals.hpp
#include "sys_vitals.hpp"

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>

namespace sys_vitals {

Snapshot sample() {
    Snapshot s;
    PERFORMANCE_INFORMATION pi{};
    pi.cb = sizeof(pi);
    if (GetPerformanceInfo(&pi, sizeof(pi))) {
        s.page_size    = (uint64_t) pi.PageSize;
        s.commit_total = (uint64_t) pi.CommitTotal * s.page_size;
        s.commit_limit = (uint64_t) pi.CommitLimit * s.page_size;
        s.ram_total    = (uint64_t) pi.PhysicalTotal * s.page_size;
        s.ram_avail    = (uint64_t) pi.PhysicalAvailable * s.page_size;
        s.valid        = s.commit_limit > 0;
    }

    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        s.commit_headroom = (uint64_t) ms.ullAvailPageFile;   // commit limit - committed
        if (!s.valid) {                                        // fall back if psapi was refused
            s.commit_limit = (uint64_t) ms.ullTotalPageFile;
            s.commit_total = (uint64_t) (ms.ullTotalPageFile - ms.ullAvailPageFile);
            s.ram_total    = (uint64_t) ms.ullTotalPhys;
            s.ram_avail    = (uint64_t) ms.ullAvailPhys;
            s.valid        = s.commit_limit > 0;
        }
    }
    return s;
}

}   // namespace sys_vitals

#else

namespace sys_vitals {

Snapshot sample() { return Snapshot{}; }

}   // namespace sys_vitals

#endif
