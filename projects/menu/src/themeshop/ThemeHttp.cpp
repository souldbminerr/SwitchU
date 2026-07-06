#include "ThemeHttp.hpp"

#include "core/DebugLog.hpp"

#include <stdexcept>

// Networking (curlpp/libcurl) has been removed from this build. The theme-shop
// HTTP API is kept as a stub so the rest of the theme-shop UI still compiles and
// runs; any download/catalog request simply fails gracefully via the callers'
// existing error handling.

namespace themeshop::http {

bool initialize() {
    return false;
}

void shutdown() {
}

bool isInitialized() {
    return false;
}

std::vector<std::uint8_t> getBytes(const std::string& url,
                                   const std::list<std::string>& headers) {
    (void)headers;
    DebugLog::log("[themeshop] networking disabled, ignoring request: %s", url.c_str());
    throw std::runtime_error("Theme Shop networking is disabled in this build");
}

std::string getText(const std::string& url,
                    const std::list<std::string>& headers) {
    (void)headers;
    DebugLog::log("[themeshop] networking disabled, ignoring request: %s", url.c_str());
    throw std::runtime_error("Theme Shop networking is disabled in this build");
}

} // namespace themeshop::http
