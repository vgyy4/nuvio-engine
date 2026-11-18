#ifndef NUVIO_ENGINE_UTIL_FS_HPP
#define NUVIO_ENGINE_UTIL_FS_HPP

// File system access for the engine.
//
// The system C++ library only ships std::filesystem from iOS 13, so builds for
// iOS 12 (NUVIO_ENGINE_USE_BOOST_FILESYSTEM) use Boost.Filesystem, compiled
// into the engine, behind the same names. Every other build uses std::filesystem.

#include <cstdint>
#include <ctime>

#if defined(NUVIO_ENGINE_USE_BOOST_FILESYSTEM)

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

namespace nuvio::fs {

using namespace boost::filesystem;
using error_code = boost::system::error_code;
using file_time_type = std::time_t;

enum class file_kind {
    regular,
    directory,
    not_found,
    other,
};

inline file_kind kind_of(const file_status& status) {
    switch (status.type()) {
    case boost::filesystem::regular_file:
        return file_kind::regular;
    case boost::filesystem::directory_file:
        return file_kind::directory;
    case boost::filesystem::file_not_found:
        return file_kind::not_found;
    default:
        return file_kind::other;
    }
}

inline bool is_missing(const error_code& error) {
    return error == boost::system::errc::no_such_file_or_directory;
}

inline file_time_type now_file_time() {
    return std::time(nullptr);
}

inline recursive_directory_iterator make_recursive_iterator(const path& root, error_code& error) {
    return recursive_directory_iterator(root, directory_options::none, error);
}

}

#else

#include <filesystem>
#include <system_error>

namespace nuvio::fs {

using namespace std::filesystem;
using error_code = std::error_code;

enum class file_kind {
    regular,
    directory,
    not_found,
    other,
};

inline file_kind kind_of(const file_status& status) {
    switch (status.type()) {
    case std::filesystem::file_type::regular:
        return file_kind::regular;
    case std::filesystem::file_type::directory:
        return file_kind::directory;
    case std::filesystem::file_type::not_found:
        return file_kind::not_found;
    default:
        return file_kind::other;
    }
}

inline bool is_missing(const error_code& error) {
    return error == std::errc::no_such_file_or_directory;
}

inline file_time_type now_file_time() {
    return file_time_type::clock::now();
}

inline recursive_directory_iterator make_recursive_iterator(const path& root, error_code& error) {
    return recursive_directory_iterator(root, directory_options::none, error);
}

}

#endif

#endif
