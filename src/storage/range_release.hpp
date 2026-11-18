#ifndef NUVIO_ENGINE_RANGE_RELEASE_HPP
#define NUVIO_ENGINE_RANGE_RELEASE_HPP

#include <cstdint>
#include "util/fs.hpp"

namespace nuvio::storage {

enum class RangeRelease {
    released,
    missing,
    unsupported,
    failed,
};

[[nodiscard]] RangeRelease release_file_range(
    const fs::path& path,
    std::uint64_t offset,
    std::uint64_t length
);

[[nodiscard]] std::uint64_t allocated_file_bytes(const fs::path& path);

[[nodiscard]] bool supports_range_release(const fs::path& directory);

}

#endif
