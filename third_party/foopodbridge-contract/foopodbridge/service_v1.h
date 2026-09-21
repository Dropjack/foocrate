// SPDX-License-Identifier: 0BSD
#pragma once

#include <cstdint>

#include <WinSock2.h>
#include <windows.h>

#include <mmsystem.h>
#include <objidl.h>

#include <foobar2000/SDK/foobar2000.h>

namespace foopodbridge::contract {

inline constexpr std::uint32_t abi_major_v1 = 1;
inline constexpr std::uint32_t abi_minor_v1 = 0;

enum class runtime_state : std::uint32_t {
    ready_no_device_provider = 1,
    ready = 2,
    shutting_down = 3,
};

enum class device_kind : std::uint32_t {
    unknown = 0,
    ipod_photo = 1,
    ipod_classic = 2,
};

enum class device_state : std::uint32_t {
    unsupported = 0,
    read_only = 1,
    writable = 2,
    busy = 3,
    removed = 4,
    error = 5,
};

enum class capability : std::uint64_t {
    none = 0,
    read_library = 1ull << 0,
    write_music = 1ull << 1,
    write_audiobooks = 1ull << 2,
    manage_playlists = 1ull << 3,
    manage_smart_playlists = 1ull << 4,
    manage_artwork = 1ull << 5,
    soundcheck = 1ull << 6,
    gapless = 1ull << 7,
};

enum class operation_kind : std::uint32_t {
    import_music = 1,
    import_audiobooks = 2,
    delete_tracks = 3,
    edit_playlist = 4,
    edit_smart_playlist = 5,
    refresh_ratings = 6,
    restore_backup = 7,
};

enum class operation_state : std::uint32_t {
    planning = 1,
    awaiting_confirmation = 2,
    executing = 3,
    cancelling = 4,
    succeeded = 5,
    succeeded_with_warnings = 6,
    failed = 7,
    recovery_required = 8,
};

enum class operation_result_code : std::uint32_t {
    success = 0,
    unsupported = 1,
    preflight_blocked = 2,
    transaction_failed = 3,
    recovery_required = 4,
    device_removed = 5,
    service_unavailable = 6,
};

class capability_matrix_v1 : public service_base {
public:
    virtual device_kind get_device_kind() noexcept = 0;
    virtual std::uint64_t get_capability_flags() noexcept = 0;
    virtual bool is_writable() noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(capability_matrix_v1, service_base);
};

class device_snapshot_v1 : public service_base {
public:
    virtual void get_stable_id(pfc::string_base& out) = 0;
    virtual void get_display_name(pfc::string_base& out) = 0;
    virtual std::uint64_t get_generation() noexcept = 0;
    virtual device_state get_state() noexcept = 0;
    virtual void get_capabilities(service_ptr_t<capability_matrix_v1>& out) noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(device_snapshot_v1, service_base);
};

class device_snapshot_list_v1 : public service_base {
public:
    virtual std::uint32_t get_count() noexcept = 0;
    virtual bool get_item(std::uint32_t index, service_ptr_t<device_snapshot_v1>& out) noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(device_snapshot_list_v1, service_base);
};

class operation_request_v1 : public service_base {
public:
    virtual operation_kind get_kind() noexcept = 0;
    virtual void get_request_id(pfc::string_base& out) = 0;
    virtual void get_device_id(pfc::string_base& out) = 0;
    virtual std::uint64_t get_snapshot_generation() noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(operation_request_v1, service_base);
};

class operation_plan_v1 : public service_base {
public:
    virtual void get_plan_id(pfc::string_base& out) = 0;
    virtual void get_device_id(pfc::string_base& out) = 0;
    virtual std::uint64_t get_snapshot_generation() noexcept = 0;
    virtual bool is_executable() noexcept = 0;
    virtual std::uint32_t get_warning_count() noexcept = 0;
    virtual void get_summary(pfc::string_base& out) = 0;

    FB2K_MAKE_SERVICE_INTERFACE(operation_plan_v1, service_base);
};

class operation_result_v1 : public service_base {
public:
    virtual operation_result_code get_code() noexcept = 0;
    virtual void get_operation_id(pfc::string_base& out) = 0;
    virtual void get_message(pfc::string_base& out) = 0;

    FB2K_MAKE_SERVICE_INTERFACE(operation_result_v1, service_base);
};

class operation_handle_v1 : public service_base {
public:
    virtual void get_operation_id(pfc::string_base& out) = 0;
    virtual operation_state get_state() noexcept = 0;
    virtual std::uint32_t get_progress_basis_points() noexcept = 0;
    virtual bool request_cancel() noexcept = 0;
    virtual bool get_plan(service_ptr_t<operation_plan_v1>& out) noexcept = 0;
    virtual bool get_result(service_ptr_t<operation_result_v1>& out) noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(operation_handle_v1, service_base);
};

class device_event_callback_v1 : public service_base {
public:
    // Providers deliver callbacks on foobar2000's main thread. Consumers must
    // still discard late callbacks after their subscription has been closed.
    virtual void on_snapshot_generation_changed(std::uint64_t generation) noexcept = 0;
    virtual void on_provider_unavailable() noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(device_event_callback_v1, service_base);
};

class subscription_v1 : public service_base {
public:
    virtual void cancel() noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(subscription_v1, service_base);
};

class device_provider_v1 : public service_base {
public:
    virtual void get_snapshots(service_ptr_t<device_snapshot_list_v1>& out) noexcept = 0;
    virtual bool subscribe(
        service_ptr_t<device_event_callback_v1> callback,
        service_ptr_t<subscription_v1>& out) noexcept = 0;
    virtual bool begin_plan(
        service_ptr_t<operation_request_v1> request,
        service_ptr_t<operation_handle_v1>& out) noexcept = 0;
    virtual bool execute_plan(
        service_ptr_t<operation_plan_v1> plan,
        service_ptr_t<operation_handle_v1>& out) noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE(device_provider_v1, service_base);
};

class service_v1 : public service_base {
public:
    virtual std::uint32_t get_contract_major() noexcept = 0;
    virtual std::uint32_t get_contract_minor() noexcept = 0;
    virtual void get_component_version(pfc::string_base& out) = 0;
    virtual runtime_state get_runtime_state() noexcept = 0;
    virtual bool get_device_provider(service_ptr_t<device_provider_v1>& out) noexcept = 0;

    FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(service_v1);
};

} // namespace foopodbridge::contract
