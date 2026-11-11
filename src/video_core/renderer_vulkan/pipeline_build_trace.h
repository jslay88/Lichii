// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <span>
#include <thread>

#include "common/cityhash.h"
#include "common/common_types.h"

namespace Vulkan {

/// EDEN_NX2_PIPELINE_BUILD_TRACE=<file>: appends a line before and after every driver pipeline
/// build, flushed at once, so the last BEGIN without an END names the shaders a driver crashed on.
/// END lines carry the build's wall time (ms=), which includes time spent waiting on other builds.
inline std::FILE* PipelineBuildTraceFile() {
    static std::FILE* const file = [] {
        const char* const path = std::getenv("EDEN_NX2_PIPELINE_BUILD_TRACE");
        return path != nullptr && *path != '\0' ? std::fopen(path, "a") : nullptr;
    }();
    return file;
}

inline std::mutex& PipelineBuildTraceMutex() {
    static std::mutex mutex;
    return mutex;
}

inline void TracePipelineBuild(const char* phase, const char* kind, u64 key_hash,
                               std::span<const u64> stage_hashes, bool unoptimized,
                               double elapsed_ms = -1.0) {
    std::FILE* const file = PipelineBuildTraceFile();
    if (file == nullptr) {
        return;
    }
    std::scoped_lock lock{PipelineBuildTraceMutex()};
    std::fprintf(file, "%s %s pipeline=%016llx thread=%zu%s", phase, kind,
                 static_cast<unsigned long long>(key_hash),
                 std::hash<std::thread::id>{}(std::this_thread::get_id()),
                 unoptimized ? " unoptimized" : "");
    for (size_t stage = 0; stage < stage_hashes.size(); ++stage) {
        if (stage_hashes[stage] != 0) {
            std::fprintf(file, " s%zu:%016llx", stage,
                         static_cast<unsigned long long>(stage_hashes[stage]));
        }
    }
    if (elapsed_ms >= 0.0) {
        std::fprintf(file, " ms=%.1f", elapsed_ms);
    }
    std::fputc('\n', file);
    std::fflush(file);
}

/// A SPIRV line per emitted graphics stage, with a hash of its code: a pipeline built again with
/// the same hashes gave the driver the same shaders.
inline void TracePipelineSpirv(u64 key_hash, size_t stage, std::span<const u32> code) {
    std::FILE* const file = PipelineBuildTraceFile();
    if (file == nullptr) {
        return;
    }
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(code.data()),
                                        code.size() * sizeof(u32));
    std::scoped_lock lock{PipelineBuildTraceMutex()};
    std::fprintf(file, "SPIRV graphics pipeline=%016llx stage=%zu words=%zu hash=%016llx\n",
                 static_cast<unsigned long long>(key_hash), stage, code.size(),
                 static_cast<unsigned long long>(hash));
    std::fflush(file);
}

} // namespace Vulkan
