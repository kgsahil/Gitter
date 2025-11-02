#pragma once

#include <filesystem>
#include <string>

namespace gitter {

/**
 * @brief Path normalization utilities
 */
class PathUtils {
public:
    /**
     * @brief Normalize path for consistent storage in index
     * 
     * Normalizes path to use forward slashes and removes unnecessary components
     * like "./" prefix. Ensures same file always has same path representation.
     * 
     * @param path Path to normalize
     * @return Normalized path string
     */
    static std::string normalize(const std::string& path);
};

}

