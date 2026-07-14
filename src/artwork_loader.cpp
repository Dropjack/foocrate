#include "artwork_loader.h"

#include <windows.h>
#include <mmsystem.h>
#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace refrain {
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

[[nodiscard]] ArtworkPixels decodeArtwork(album_art_data_ptr data, abort_callback& aborter) {
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

    constexpr UINT maxDimension = 1024;
    const auto scale = std::min(1.0, static_cast<double>(maxDimension)
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

} // namespace

ArtworkPixels loadFrontArtwork(const metadb_handle_ptr& target, abort_callback& aborter) noexcept {
    try {
        if (target.is_empty()) {
            return {ArtworkStatus::missing};
        }
        metadb_handle_list items;
        items.add_item(target);
        pfc::list_t<GUID> ids;
        ids.add_item(album_art_ids::cover_front);
        const auto extractor = album_art_manager_v2::get()->open(items, ids, aborter);
        const auto data = extractor->query(album_art_ids::cover_front, aborter);
        return decodeArtwork(data, aborter);
    } catch (const exception_aborted&) {
        return {ArtworkStatus::aborted};
    } catch (const exception_album_art_not_found&) {
        return {ArtworkStatus::missing};
    } catch (...) {
        return {ArtworkStatus::unavailable};
    }
}

ArtworkPixels loadGroupArtwork(
    metadb_handle_list_cref targets, const GUID& artworkId, abort_callback& aborter) noexcept {
    try {
        if (targets.get_count() == 0) return {ArtworkStatus::missing};
        pfc::list_t<GUID> ids;
        ids.add_item(artworkId);
        const auto extractor = album_art_manager_v2::get()->open(targets, ids, aborter);
        return decodeArtwork(extractor->query(artworkId, aborter), aborter);
    } catch (const exception_aborted&) {
        return {ArtworkStatus::aborted};
    } catch (const exception_album_art_not_found&) {
        return {ArtworkStatus::missing};
    } catch (...) {
        return {ArtworkStatus::unavailable};
    }
}

} // namespace refrain
