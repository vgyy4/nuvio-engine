#ifndef NUVIO_ENGINE_ATOMIC_FILE_HPP
#define NUVIO_ENGINE_ATOMIC_FILE_HPP

#include <cstddef>
#include "util/fs.hpp"
#include <optional>
#include <span>
#include <vector>

namespace nuvio::storage {

[[nodiscard]] std::optional<std::vector<char>> read_bounded_file(
    const fs::path& path,
    std::size_t maximum_size
);

void write_file_atomically(
    const fs::path& path,
    std::span<const char> contents
);

void remove_file_if_present(const fs::path& path);

}

#endif
