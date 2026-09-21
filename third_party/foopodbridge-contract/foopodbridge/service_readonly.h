// SPDX-License-Identifier: 0BSD
#pragma once
#include "foopodbridge/service_v1.h"

namespace foopodbridge::contract {
// ABI 1.1 adds queryable extensions; ABI 1.0 vtables and GUIDs remain unchanged.
inline constexpr std::uint32_t readonly_contract_minor = 1;
enum class track_text : std::uint32_t { title, artist, album, relative_path };
enum class read_media_kind : std::uint32_t { unknown, music, audiobook, other };
enum class read_playlist_kind : std::uint32_t { master, ordinary, smart, opaque };
enum class read_state : std::uint32_t {
    not_mounted, unidentified, unsupported_filesystem, format_pending, initializable,
    ready_read_only, database_corrupt, recovery_required, read_error
};
enum class read_evidence : std::uint32_t { unknown, structure_known, fixture_round_trip, device_read_verified, device_write_verified };

class library_snapshot_v1 : public service_base {
public:
    virtual std::uint32_t get_track_count() noexcept = 0;
    virtual bool get_track(std::uint32_t index, std::uint32_t& id, std::uint64_t& persistent_id, read_media_kind& kind) noexcept = 0;
    virtual bool get_track_text(std::uint32_t index, track_text field, pfc::string_base& out) = 0;
    // Includes the master Library as a distinct playlist kind.
    virtual std::uint32_t get_playlist_count() noexcept = 0;
    virtual bool get_playlist(std::uint32_t index, std::uint64_t& id, read_playlist_kind& kind, std::uint32_t& member_count, pfc::string_base& name) = 0;
    virtual bool get_playlist_member(std::uint32_t playlist, std::uint32_t member, std::uint32_t& track_id) noexcept = 0;
    FB2K_MAKE_SERVICE_INTERFACE(library_snapshot_v1, service_base);
};
class device_snapshot_readonly_v1 : public device_snapshot_v1 {
public:
    virtual std::uint64_t get_revision() noexcept = 0;
    virtual read_state get_read_state() noexcept = 0;
    virtual read_evidence get_evidence() noexcept = 0;
    virtual bool has_stable_identity() noexcept = 0;
    virtual bool get_capacity(std::uint64_t& total, std::uint64_t& available) noexcept = 0;
    virtual void get_profile(pfc::string_base& out) = 0;
    virtual void get_status_description(pfc::string_base& out) = 0;
    virtual bool get_library(service_ptr_t<library_snapshot_v1>& out) noexcept = 0;
    FB2K_MAKE_SERVICE_INTERFACE(device_snapshot_readonly_v1, device_snapshot_v1);
};
class device_provider_readonly_v1 : public device_provider_v1 {
public:
    // Refresh only schedules work. It never performs I/O on the caller's thread.
    virtual bool refresh() noexcept = 0;
    virtual bool is_scanning() noexcept = 0;
    virtual bool is_current(const char* token, std::uint64_t generation, std::uint64_t revision) noexcept = 0;
    virtual void get_provider_status(pfc::string_base& out) = 0;
    FB2K_MAKE_SERVICE_INTERFACE(device_provider_readonly_v1, device_provider_v1);
};
} // namespace foopodbridge::contract
