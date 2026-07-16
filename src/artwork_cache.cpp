#include "artwork_cache.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <system_error>
#include <vector>

#include <foobar2000/SDK/foobar2000.h>

namespace refrain {
namespace {

constexpr std::uint32_t kPixelMagic = 0x31585052U; // RPX1
constexpr std::uint32_t kThemeMagic = 0x314D5452U; // RTM1
constexpr std::uint32_t kCacheVersion = 1U;
constexpr std::uint32_t kMaximumCachedDimension = 320U;
constexpr std::uint32_t kMaximumPixelBytes = kMaximumCachedDimension * kMaximumCachedDimension * 4U;

struct PixelHeader {
    std::uint32_t magic{};
    std::uint32_t version{};
    std::uint32_t maximumDimension{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::uint32_t payloadBytes{};
    std::uint32_t checksum{};
};

struct ThemeHeader {
    std::uint32_t magic{};
    std::uint32_t version{};
    std::uint32_t payloadBytes{};
    std::uint32_t checksum{};
};

[[nodiscard]] std::uint32_t checksum(const void* bytes, std::size_t size) noexcept {
    auto value = std::uint32_t{2166136261U};
    const auto* input = static_cast<const std::uint8_t*>(bytes);
    for (std::size_t index = 0; index < size; ++index) {
        value ^= input[index];
        value *= 16777619U;
    }
    return value;
}

[[nodiscard]] std::filesystem::path pixelPath(
    const std::filesystem::path& root, const std::string& identity, std::uint32_t dimension) {
    return root / "artwork" / (identity + "-" + std::to_string(dimension) + ".rpx");
}

[[nodiscard]] std::filesystem::path themePath(
    const std::filesystem::path& root, const std::string& identity) {
    return root / "theme-colors" / (identity + ".rtm");
}

[[nodiscard]] std::filesystem::path temporaryPath(const std::filesystem::path& target) {
    return target.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId())
        + L"-" + std::to_wstring(GetCurrentThreadId());
}

template<typename Header>
[[nodiscard]] bool writeAtomically(
    const std::filesystem::path& target, const Header& header, const void* payload, std::size_t payloadBytes) {
    const auto temporary = temporaryPath(target);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) return false;
        output.write(reinterpret_cast<const char*>(&header), sizeof(header));
        output.write(static_cast<const char*>(payload), static_cast<std::streamsize>(payloadBytes));
        output.flush();
        if (!output) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

[[nodiscard]] std::array<std::uint32_t, 19> encodeTheme(const ThemePalette& palette) noexcept {
    return {palette.dark ? 1U : 0U,
        rgbValue(palette.backgroundBase), rgbValue(palette.surfacePanel),
        rgbValue(palette.surfaceMuted), rgbValue(palette.surfaceElevated),
        rgbValue(palette.textPrimary), rgbValue(palette.textSecondary),
        rgbValue(palette.textDisabled), rgbValue(palette.textOnAccent),
        rgbValue(palette.borderSubtle), rgbValue(palette.accent),
        rgbValue(palette.accentHover), rgbValue(palette.stateHover),
        rgbValue(palette.stateSelected), rgbValue(palette.focus),
        rgbValue(palette.defaultPlaylist), rgbValue(palette.error),
        rgbValue(palette.lyricsHighlight), rgbValue(palette.lyricsNormal)};
}

[[nodiscard]] ThemePalette decodeTheme(const std::array<std::uint32_t, 19>& values) noexcept {
    return {values[0] != 0, rgb(values[1]), rgb(values[2]), rgb(values[3]), rgb(values[4]),
        rgb(values[5]), rgb(values[6]), rgb(values[7]), rgb(values[8]), rgb(values[9]),
        rgb(values[10]), rgb(values[11]), rgb(values[12]), rgb(values[13]), rgb(values[14]),
        rgb(values[15]), rgb(values[16]), rgb(values[17]), rgb(values[18])};
}

void touch(const std::filesystem::path& path) noexcept {
    std::error_code ignored;
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), ignored);
}

[[nodiscard]] std::filesystem::path pathFromUtf8(const char* value) {
    if (!value || *value == '\0') return {};
    const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, nullptr, 0);
    if (length <= 1) return {};
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1,
            wide.data(), length) != length) return {};
    wide.resize(static_cast<std::size_t>(length - 1));
    return std::filesystem::path(std::move(wide));
}

} // namespace

ArtworkDiskCache::ArtworkDiskCache(std::filesystem::path root, std::uintmax_t maximumBytes)
    : m_root(std::move(root)), m_maximumBytes(maximumBytes) {}

void ArtworkDiskCache::prepare() noexcept {
    if (m_prepared) return;
    if (m_root.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(m_root / "artwork", error);
    if (!error) std::filesystem::create_directories(m_root / "theme-colors", error);
    m_prepared = !error;
    if (m_prepared) trim();
}

void ArtworkDiskCache::trim() noexcept {
    struct Entry {
        std::filesystem::path path;
        std::uintmax_t size{};
        std::filesystem::file_time_type modified{};
    };
    std::vector<Entry> entries;
    std::uintmax_t total{};
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(
             m_root, std::filesystem::directory_options::skip_permission_denied, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || error) continue;
        const auto size = iterator->file_size(error);
        if (error) continue;
        entries.push_back({iterator->path(), size, iterator->last_write_time(error)});
        if (!error) total += size;
    }
    if (total <= m_maximumBytes) return;
    std::sort(entries.begin(), entries.end(),
        [](const Entry& left, const Entry& right) { return left.modified < right.modified; });
    for (const auto& entry : entries) {
        if (total <= m_maximumBytes) break;
        if (std::filesystem::remove(entry.path, error) && !error) {
            total -= std::min(total, entry.size);
        }
        error.clear();
    }
}

std::optional<ArtworkPixels> ArtworkDiskCache::loadPixels(
    const std::string& identity, std::uint32_t maximumDimension) noexcept {
    std::scoped_lock lock(m_mutex);
    prepare();
    if (!m_prepared || identity.empty() || maximumDimension == 0
        || maximumDimension > kMaximumCachedDimension) return std::nullopt;
    const auto path = pixelPath(m_root, identity, maximumDimension);
    PixelHeader header;
    std::ifstream input(path, std::ios::binary);
    if (!input || !input.read(reinterpret_cast<char*>(&header), sizeof(header))
        || header.magic != kPixelMagic || header.version != kCacheVersion
        || header.maximumDimension != maximumDimension || header.width == 0 || header.height == 0
        || header.width > maximumDimension || header.height > maximumDimension
        || header.payloadBytes != header.width * header.height * 4U
        || header.payloadBytes > kMaximumPixelBytes) return std::nullopt;
    ArtworkPixels pixels{ArtworkStatus::ready, header.width, header.height};
    pixels.bgra.resize(header.payloadBytes);
    if (!input.read(reinterpret_cast<char*>(pixels.bgra.data()), header.payloadBytes)
        || checksum(pixels.bgra.data(), pixels.bgra.size()) != header.checksum) {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return std::nullopt;
    }
    touch(path);
    return pixels;
}

void ArtworkDiskCache::storePixels(
    const std::string& identity, std::uint32_t maximumDimension, const ArtworkPixels& pixels) noexcept {
    if (pixels.status != ArtworkStatus::ready || identity.empty() || maximumDimension == 0
        || maximumDimension > kMaximumCachedDimension || pixels.width == 0 || pixels.height == 0
        || pixels.width > maximumDimension || pixels.height > maximumDimension
        || pixels.bgra.size() != static_cast<std::size_t>(pixels.width) * pixels.height * 4U
        || pixels.bgra.size() > kMaximumPixelBytes) return;
    std::scoped_lock lock(m_mutex);
    prepare();
    if (!m_prepared) return;
    const PixelHeader header{kPixelMagic, kCacheVersion, maximumDimension, pixels.width, pixels.height,
        static_cast<std::uint32_t>(pixels.bgra.size()), checksum(pixels.bgra.data(), pixels.bgra.size())};
    if (writeAtomically(pixelPath(m_root, identity, maximumDimension),
            header, pixels.bgra.data(), pixels.bgra.size())
        && (++m_pixelWritesSinceTrim >= 16U
            || m_maximumBytes < static_cast<std::uintmax_t>(kMaximumPixelBytes) * 16U)) {
        m_pixelWritesSinceTrim = 0;
        trim();
    }
}

std::optional<ThemePalette> ArtworkDiskCache::loadTheme(const std::string& identity) noexcept {
    std::scoped_lock lock(m_mutex);
    prepare();
    if (!m_prepared || identity.empty()) return std::nullopt;
    const auto path = themePath(m_root, identity);
    ThemeHeader header;
    std::array<std::uint32_t, 19> values{};
    std::ifstream input(path, std::ios::binary);
    if (!input || !input.read(reinterpret_cast<char*>(&header), sizeof(header))
        || header.magic != kThemeMagic || header.version != kCacheVersion
        || header.payloadBytes != sizeof(values)
        || !input.read(reinterpret_cast<char*>(values.data()), sizeof(values))
        || checksum(values.data(), sizeof(values)) != header.checksum) {
        input.close();
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        return std::nullopt;
    }
    touch(path);
    return decodeTheme(values);
}

void ArtworkDiskCache::storeTheme(const std::string& identity, const ThemePalette& palette) noexcept {
    std::scoped_lock lock(m_mutex);
    prepare();
    if (!m_prepared || identity.empty()) return;
    const auto values = encodeTheme(palette);
    const ThemeHeader header{kThemeMagic, kCacheVersion, static_cast<std::uint32_t>(sizeof(values)),
        checksum(values.data(), sizeof(values))};
    (void)writeAtomically(themePath(m_root, identity), header, values.data(), sizeof(values));
}

ArtworkDiskCache& profileArtworkCache() noexcept {
    static ArtworkDiskCache cache([] {
        try {
            if (core_api::is_quiet_mode_enabled()) return std::filesystem::path{};
            const auto native = filesystem::g_get_native_path(core_api::get_profile_path());
            return pathFromUtf8(native.c_str()) / "refrain" / "cache";
        } catch (...) {
            return std::filesystem::path{};
        }
    }());
    return cache;
}

} // namespace refrain
