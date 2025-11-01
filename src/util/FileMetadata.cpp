#include "util/FileMetadata.hpp"

#include <chrono>
#include <filesystem>

namespace gitter {

FileMetadata getFileMetadata(const std::filesystem::path& filePath) {
    FileMetadata metadata;
    std::error_code ec;

    // Get file size
    metadata.sizeBytes = static_cast<uint64_t>(std::filesystem::file_size(filePath, ec));
    if (ec) {
        metadata.sizeBytes = 0;
    }

    // Get modification time (convert to nanoseconds)
    auto ft = std::filesystem::last_write_time(filePath, ec);
    if (!ec) {
        // Convert filesystem time to system clock time (for consistent timestamps)
        auto now_sys = std::chrono::system_clock::now();
        auto now_file = std::filesystem::file_time_type::clock::now();
        auto adj = ft - now_file + now_sys;
        metadata.mtimeNs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(adj.time_since_epoch()).count()
        );
        // Use mtime as ctime fallback (simplified)
        metadata.ctimeNs = metadata.mtimeNs;
    }

    // Get file permissions and convert to Git mode
    auto status = std::filesystem::status(filePath, ec);
    if (!ec) {
        auto perms = status.permissions();
        // Git uses octal mode: 0100644 (regular file) or 0100755 (executable)
        if ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ||
            (perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none ||
            (perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none) {
            metadata.mode = 0100755;  // Executable
        } else {
            metadata.mode = 0100644;  // Regular file
        }
    }

    return metadata;
}

}

