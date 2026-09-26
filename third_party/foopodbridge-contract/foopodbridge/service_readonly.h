// SPDX-License-Identifier: 0BSD
#pragma once
#include "foopodbridge/service_v1.h"

namespace foopodbridge::contract {
// ABI 1.x adds queryable extensions; earlier vtables and GUIDs remain unchanged.
inline constexpr std::uint32_t readonly_contract_minor = 5;
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
    // Current mounted root for read-only playback consumers; never grants write access.
    virtual void get_mount_root(pfc::string_base& out) = 0;
    FB2K_MAKE_SERVICE_INTERFACE(device_snapshot_readonly_v1, device_snapshot_v1);
};
enum class recovery_point_type : std::uint32_t { pending_transaction = 1, last_known_good = 2 };
class device_snapshot_recovery_v1 : public device_snapshot_readonly_v1 {
public:
    // These are explicit candidates from the current immutable read snapshot.
    // They are not write authorization; execution must revalidate all evidence.
    virtual std::uint32_t get_recovery_point_count() noexcept = 0;
    virtual bool get_recovery_point(std::uint32_t index, recovery_point_type& kind,
        std::uint64_t& sequence, pfc::string_base& id) = 0;
    FB2K_MAKE_SERVICE_INTERFACE(device_snapshot_recovery_v1, device_snapshot_readonly_v1);
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
class device_provider_recovery_v1 : public device_provider_readonly_v1 {
public:
    // Validates a selection against the current discovery revision. This is
    // read-only and does not grant permission to execute a recovery.
    virtual bool is_current_recovery_point(const char* token, std::uint64_t generation,
        std::uint64_t revision, recovery_point_type kind, const char* id) noexcept = 0;
    FB2K_MAKE_SERVICE_INTERFACE(device_provider_recovery_v1, device_provider_readonly_v1);
};
enum class recovery_access_code : std::uint32_t { allowed, stale_selection, task_not_authorized, service_stopped, busy };
enum class recovery_execution_stage : std::uint32_t { checking, verifying_backup, recovering, committing, cleaning, finished };
enum class recovery_result_code : std::uint32_t { completed, cancelled, failed, busy, recovery_required, cleaning_required };
class recovery_execution_v1 : public service_base {
public:
    virtual void get_operation_id(pfc::string_base& out) = 0;
    virtual recovery_execution_stage get_stage() noexcept = 0;
    virtual bool request_cancel() noexcept = 0;
    // False while running; completion never hides a committed database behind cancellation.
    virtual bool get_result(recovery_result_code& code, bool& committed, bool& cancel_deferred,
        pfc::string_base& diagnostic) = 0;
    FB2K_MAKE_SERVICE_INTERFACE(recovery_execution_v1, service_base);
};
class device_provider_recovery_execution_v1 : public device_provider_recovery_v1 {
public:
    // Only opaque selection fields cross the ABI. No task grant, path, device key,
    // signing input or baseline permission can be supplied by a consumer.
    virtual recovery_access_code get_recovery_access(const char* token, std::uint64_t generation,
        std::uint64_t revision, recovery_point_type kind, const char* point_id) noexcept = 0;
    virtual recovery_access_code begin_recovery(const char* token, std::uint64_t generation,
        std::uint64_t revision, recovery_point_type kind, const char* point_id,
        service_ptr_t<recovery_execution_v1>& out) noexcept = 0;
    FB2K_MAKE_SERVICE_INTERFACE(device_provider_recovery_execution_v1, device_provider_recovery_v1);
};
} // namespace foopodbridge::contract
