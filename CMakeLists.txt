cmake_minimum_required(VERSION 3.25)

project(FooCrate VERSION 1.0.3 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "FooCrate currently supports Windows only.")
endif()

if(NOT MSVC)
  message(FATAL_ERROR "FooCrate requires the Microsoft Visual C++ toolchain.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "FooCrate must be configured for x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(CTest)
include(cmake/Foobar2000Sdk.cmake)

add_library(foo_crate MODULE
  src/artwork_cache.cpp
  src/artwork_cache.h
  src/artwork_loader.cpp
  src/artwork_loader.h
  src/album_browser_model.h
  src/component.cpp
  src/component_identity.h
  src/compact_track_list_model.h
  src/foo_crate.rc
  src/lyrics_host.cpp
  src/lyrics_host.h
  src/playback_panel.cpp
  src/playback_panel.h
  src/playback_state.h
  src/playlist_config_model.h
  src/playlist_grouping_model.h
  src/playlist_interaction_model.h
  src/playlist_browser_model.h
  src/playlist_preferences.cpp
  src/playlist_view_model.h
  src/now_playing_state.h
  src/playback_layout.h
  src/playback_location_model.h
  src/playback_order_icon.h
  src/now_playing_toolbar.cpp
  src/now_playing_preferences.cpp
  src/dependencies_preferences.cpp
  src/preferences.cpp
  src/settings.cpp
  src/settings.h
  src/settings_model.h
  src/theme_model.cpp
  src/theme_model.h
)

set_target_properties(foo_crate PROPERTIES
  PREFIX ""
  OUTPUT_NAME "foo_crate"
)

target_include_directories(foo_crate PRIVATE src)
target_compile_definitions(foo_crate PRIVATE
  _UNICODE
  UNICODE
  _WINDLL
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
target_compile_options(foo_crate PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8 /external:W0)
target_link_options(foo_crate PRIVATE /INCREMENTAL:NO)
target_link_libraries(foo_crate PRIVATE
  foocrate_foobar2000_sdk
  foocrate_component_client
  foocrate_pfc
  foocrate_shared
  foocrate_columns_ui_sdk
  foocrate_libppui_sdk
  comctl32
  d2d1
  dwrite
  windowscodecs
  shlwapi
  shell32
  winmm
)

if(BUILD_TESTING)
  add_executable(foocrate_identity_tests tests/component_identity_tests.cpp)
  target_include_directories(foocrate_identity_tests PRIVATE src)
  target_compile_definitions(foocrate_identity_tests PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(foocrate_identity_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME component_identity COMMAND foocrate_identity_tests)

  add_executable(foocrate_playback_state_tests tests/playback_state_tests.cpp)
  target_include_directories(foocrate_playback_state_tests PRIVATE src)
  target_compile_definitions(foocrate_playback_state_tests PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(foocrate_playback_state_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playback_state COMMAND foocrate_playback_state_tests)

  add_executable(foocrate_playback_layout_tests tests/playback_layout_tests.cpp)
  target_include_directories(foocrate_playback_layout_tests PRIVATE src)
  target_compile_options(foocrate_playback_layout_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playback_layout COMMAND foocrate_playback_layout_tests)

  add_executable(foocrate_playback_location_model_tests tests/playback_location_model_tests.cpp)
  target_include_directories(foocrate_playback_location_model_tests PRIVATE src)
  target_compile_options(foocrate_playback_location_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playback_location_model COMMAND foocrate_playback_location_model_tests)

  add_executable(foocrate_playback_order_icon_tests tests/playback_order_icon_tests.cpp)
  target_include_directories(foocrate_playback_order_icon_tests PRIVATE src)
  target_compile_options(foocrate_playback_order_icon_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playback_order_icon COMMAND foocrate_playback_order_icon_tests)

  add_executable(foocrate_settings_model_tests tests/settings_model_tests.cpp)
  target_include_directories(foocrate_settings_model_tests PRIVATE src)
  target_compile_options(foocrate_settings_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME settings_model COMMAND foocrate_settings_model_tests)

  add_executable(foocrate_theme_model_tests tests/theme_model_tests.cpp src/theme_model.cpp)
  target_include_directories(foocrate_theme_model_tests PRIVATE src)
  target_compile_options(foocrate_theme_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME theme_model COMMAND foocrate_theme_model_tests)

  add_executable(foocrate_artwork_cache_tests
    tests/artwork_cache_tests.cpp
    src/artwork_cache.cpp
    src/theme_model.cpp
  )
  target_include_directories(foocrate_artwork_cache_tests PRIVATE src)
  target_compile_definitions(foocrate_artwork_cache_tests PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(foocrate_artwork_cache_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  target_link_libraries(foocrate_artwork_cache_tests PRIVATE
    foocrate_foobar2000_sdk
    foocrate_component_client
    foocrate_pfc
    foocrate_shared
  )
  add_test(NAME artwork_cache COMMAND foocrate_artwork_cache_tests)

  add_executable(foocrate_now_playing_state_tests tests/now_playing_state_tests.cpp)
  target_include_directories(foocrate_now_playing_state_tests PRIVATE src)
  target_compile_options(foocrate_now_playing_state_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME now_playing_state COMMAND foocrate_now_playing_state_tests)

  add_executable(foocrate_playlist_view_model_tests tests/playlist_view_model_tests.cpp)
  target_include_directories(foocrate_playlist_view_model_tests PRIVATE src)
  target_compile_options(foocrate_playlist_view_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_view_model COMMAND foocrate_playlist_view_model_tests)

  add_executable(foocrate_playlist_grouping_model_tests tests/playlist_grouping_model_tests.cpp)
  target_include_directories(foocrate_playlist_grouping_model_tests PRIVATE src)
  target_compile_options(foocrate_playlist_grouping_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_grouping_model COMMAND foocrate_playlist_grouping_model_tests)

  add_executable(foocrate_playlist_interaction_model_tests tests/playlist_interaction_model_tests.cpp)
  target_include_directories(foocrate_playlist_interaction_model_tests PRIVATE src)
  target_compile_options(foocrate_playlist_interaction_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_interaction_model COMMAND foocrate_playlist_interaction_model_tests)

  add_executable(foocrate_album_browser_model_tests tests/album_browser_model_tests.cpp)
  target_include_directories(foocrate_album_browser_model_tests PRIVATE src)
  target_compile_options(foocrate_album_browser_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME album_browser_model COMMAND foocrate_album_browser_model_tests)

  add_executable(foocrate_playlist_browser_model_tests tests/playlist_browser_model_tests.cpp)
  target_include_directories(foocrate_playlist_browser_model_tests PRIVATE src)
  target_compile_definitions(foocrate_playlist_browser_model_tests PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(foocrate_playlist_browser_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_browser_model COMMAND foocrate_playlist_browser_model_tests)
endif()

find_program(FOOCRATE_POWERSHELL_EXECUTABLE NAMES powershell REQUIRED)

add_custom_target(package_component
  COMMAND "${FOOCRATE_POWERSHELL_EXECUTABLE}"
    -NoProfile
    -ExecutionPolicy Bypass
    -File "${CMAKE_SOURCE_DIR}/scripts/package-component.ps1"
    -RepositoryRoot "${CMAKE_SOURCE_DIR}"
    -ComponentDll "$<TARGET_FILE:foo_crate>"
    -OutputDirectory "${CMAKE_SOURCE_DIR}/dist"
    -Version "${PROJECT_VERSION}"
  DEPENDS foo_crate
  VERBATIM
)
