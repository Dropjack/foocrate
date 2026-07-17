#include "artwork_loader.h"
#include "artwork_cache.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>

namespace foocrate {
namespace {

using Microsoft::WRL::ComPtr;

class ComApartment {
public:
    ComApartment() noexcept : m_result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ~ComApartment() {
        if (SUCCEEDED(m_result)) {
            CoUninitialize();
        }
    }
    [[nodiscard]] bool available() const noexcept { return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE; }

private:
    HRESULT m_result{};
};

void hashBytes(hasher_md5& hasher, hasher_md5_state& state, const void* value, std::size_t size) {
    hasher.process(state, value, size);
}

void hashString(hasher_md5& hasher, hasher_md5_state& state, const char* value) {
    const auto length = value ? std::strlen(value) : 0U;
    hashBytes(hasher, state, &length, sizeof(length));
    if (length != 0) hashBytes(hasher, state, value, length);
}

[[nodiscard]] bool hashPath(
    hasher_md5& hasher, hasher_md5_state& state, const char* path, abort_callback& aborter) {
    if (!path || filesystem::g_is_remote_or_unrecognized(path)) return false;
    foobar2000_io::t_filestats stats;
    bool writeable{};
    filesystem::g_get_stats(path, stats, writeable, aborter);
    hashString(hasher, state, path);
    hashBytes(hasher, state, &stats.m_size, sizeof(stats.m_size));
    hashBytes(hasher, state, &stats.m_timestamp, sizeof(stats.m_timestamp));
    return true;
}

[[nodiscard]] std::optional<std::string> artworkIdentity(
    const GUID& artworkId, const album_art_extractor_instance_v2::ptr& extractor,
    abort_callback& aborter) {
    try {
        auto hasher = hasher_md5::get();
        auto state = hasher->initialize();
        constexpr std::uint32_t identityVersion = 1U;
        hashBytes(*hasher, state, &identityVersion, sizeof(identityVersion));
        hashBytes(*hasher, state, &artworkId, sizeof(artworkId));
        const auto paths = extractor->query_paths(artworkId, aborter);
        if (paths.is_empty()) return std::nullopt;
        const auto pathCount = paths->get_count();
        hashBytes(*hasher, state, &pathCount, sizeof(pathCount));
        for (t_size index = 0; index < pathCount; ++index) {
            if (!hashPath(*hasher, state, paths->get_path(index), aborter)) return std::nullopt;
        }
        return std::string(hasher->get_result(state).asString().c_str());
    } catch (const exception_aborted&) {
        throw;
    } catch (...) {
        return std::nullopt;
    }
}

[[nodiscard]] ArtworkPixels decodeArtwork(
    album_art_data_ptr data, abort_callback& aborter, std::uint32_t maxDimension) {
    ArtworkPixels result;
    aborter.check();
    if (data.is_empty() || data->size() == 0 || data->size() > static_cast<std::size_t>(UINT_MAX)) {
        result.status = ArtworkStatus::unavailable;
        return result;
    }

    ComApartment apartment;
    if (!apartment.available()) {
        result.status = ArtworkStatus::unavailable;
        return result;
    }

    std::vector<std::uint8_t> encoded(data->size());
    std::memcpy(encoded.data(), data->data(), encoded.size());

    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(factory.ReleaseAndGetAddressOf())))
        || FAILED(factory->CreateStream(stream.ReleaseAndGetAddressOf()))
        || FAILED(stream->InitializeFromMemory(encoded.data(), static_cast<DWORD>(encoded.size())))
        || FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand,
            decoder.ReleaseAndGetAddressOf()))
        || FAILED(decoder->GetFrame(0, frame.ReleaseAndGetAddressOf()))) {
        result.status = ArtworkStatus::unavailable;
        return result;
    }

    aborter.check();
    UINT sourceWidth{};
    UINT sourceHeight{};
    if (FAILED(frame->GetSize(&sourceWidth, &sourceHeight)) || sourceWidth == 0 || sourceHeight == 0) {
        result.status = ArtworkStatus::unavailable;
        return result;
    }

    const auto boundedDimension = std::max(1U, static_cast<UINT>(maxDimension));
    const auto scale = std::min(1.0, static_cast<double>(boundedDimension)
        / static_cast<double>(std::max(sourceWidth, sourceHeight)));
    const auto width = std::max(1U, static_cast<UINT>(std::lround(sourceWidth * scale)));
    const auto height = std::max(1U, static_cast<UINT>(std::lround(sourceHeight * scale)));

    ComPtr<IWICBitmapSource> source = frame;
    ComPtr<IWICBitmapScaler> scaler;
    if (width != sourceWidth || height != sourceHeight) {
        if (FAILED(factory->CreateBitmapScaler(scaler.ReleaseAndGetAddressOf()))
            || FAILED(scaler->Initialize(frame.Get(), width, height, WICBitmapInterpolationModeFant))) {
            result.status = ArtworkStatus::unavailable;
            return result;
        }
        source = scaler;
    }

    ComPtr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(converter.ReleaseAndGetAddressOf()))
        || FAILED(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        result.status = ArtworkStatus::unavailable;
        return result;
    }

    const auto stride64 = static_cast<std::uint64_t>(width) * 4ULL;
    const auto bytes64 = stride64 * static_cast<std::uint64_t>(height);
    if (stride64 > UINT_MAX || bytes64 > UINT_MAX || bytes64 > 64ULL * 1024ULL * 1024ULL) {
        result.status = ArtworkStatus::unavailable;
        return result;
    }

    aborter.check();
    result.bgra.resize(static_cast<std::size_t>(bytes64));
    if (FAILED(converter->CopyPixels(nullptr, static_cast<UINT>(stride64),
            static_cast<UINT>(bytes64), result.bgra.data()))) {
        result = {};
        result.status = ArtworkStatus::unavailable;
        return result;
    }
    aborter.check();
    result.status = ArtworkStatus::ready;
    result.width = width;
    result.height = height;
    return result;
}

[[nodiscard]] ArtworkPixels loadSingleArtworkBounded(const metadb_handle_ptr& target,
    const GUID& artworkId, abort_callback& aborter, std::uint32_t maxDimension,
    bool usePersistentCache) noexcept {
    try {
        if (target.is_empty()) return {ArtworkStatus::missing};
        metadb_handle_list items;
        items.add_item(target);
        pfc::list_t<GUID> ids;
        ids.add_item(artworkId);
        const auto extractor = album_art_manager_v2::get()->open(items, ids, aborter);
        const auto identity = usePersistentCache
            ? artworkIdentity(artworkId, extractor, aborter) : std::nullopt;
        if (identity) {
            if (const auto cached = profileArtworkCache().loadPixels(*identity, maxDimension)) return *cached;
        }
        auto pixels = decodeArtwork(extractor->query(artworkId, aborter), aborter, maxDimension);
        if (identity) profileArtworkCache().storePixels(*identity, maxDimension, pixels);
        return pixels;
    } catch (const exception_aborted&) {
        return {ArtworkStatus::aborted};
    } catch (const exception_album_art_not_found&) {
        return {ArtworkStatus::missing};
    } catch (...) {
        return {ArtworkStatus::unavailable};
    }
}

} // namespace

ArtworkPixels loadFrontArtwork(const metadb_handle_ptr& target, abort_callback& aborter) noexcept {
    return loadArtwork(target, album_art_ids::cover_front, aborter);
}

ArtworkPixels loadArtwork(const metadb_handle_ptr& target, const GUID& artworkId, abort_callback& aborter) noexcept {
    return loadSingleArtworkBounded(target, artworkId, aborter, 1024U, false);
}

ArtworkPixels loadGroupArtwork(
    metadb_handle_list_cref targets, const GUID& artworkId, abort_callback& aborter,
    std::uint32_t maxDimension) noexcept {
    try {
        if (targets.get_count() == 0) return {ArtworkStatus::missing};
        pfc::list_t<GUID> ids;
        ids.add_item(artworkId);
        const auto extractor = album_art_manager_v2::get()->open(targets, ids, aborter);
        const auto identity = maxDimension <= 320U
            ? artworkIdentity(artworkId, extractor, aborter) : std::nullopt;
        if (identity) {
            if (const auto cached = profileArtworkCache().loadPixels(*identity, maxDimension)) return *cached;
        }
        auto pixels = decodeArtwork(extractor->query(artworkId, aborter), aborter, maxDimension);
        if (pixels.status == ArtworkStatus::ready) {
            if (identity) profileArtworkCache().storePixels(*identity, maxDimension, pixels);
            return pixels;
        }
        if (pixels.status == ArtworkStatus::aborted) return pixels;
    } catch (const exception_aborted&) {
        return {ArtworkStatus::aborted};
    } catch (...) {
        // A group may legitimately contain different embedded covers. If the core cannot
        // produce one group result, keep the group order and select the first decodable item.
    }
    return selectFirstReadyArtwork(targets.get_count(), [&](std::size_t index) {
        return loadSingleArtworkBounded(targets.get_item(index), artworkId, aborter,
            maxDimension, maxDimension <= 320U);
    });
}

std::optional<ThemePalette> loadArtworkTheme(
    const metadb_handle_ptr& target, const GUID& artworkId, abort_callback& aborter) noexcept {
    try {
        if (target.is_empty()) return std::nullopt;
        metadb_handle_list items;
        items.add_item(target);
        pfc::list_t<GUID> ids;
        ids.add_item(artworkId);
        const auto extractor = album_art_manager_v2::get()->open(items, ids, aborter);
        const auto identity = artworkIdentity(artworkId, extractor, aborter);
        if (identity) {
            if (const auto cached = profileArtworkCache().loadTheme(*identity)) return cached;
        }
        const auto pixels = decodeArtwork(extractor->query(artworkId, aborter), aborter, 1024U);
        if (pixels.status != ArtworkStatus::ready) return std::nullopt;
        const auto palette = extractArtworkTheme(pixels.bgra, pixels.width, pixels.height);
        if (identity && palette) profileArtworkCache().storeTheme(*identity, *palette);
        return palette;
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace foocrate
