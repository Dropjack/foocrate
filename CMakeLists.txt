cmake_minimum_required(VERSION 3.25)

project(Refrain VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "Refrain currently supports Windows only.")
endif()

if(NOT MSVC)
  message(FATAL_ERROR "Refrain requires the Microsoft Visual C++ toolchain.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Refrain must be configured for x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(CTest)
include(cmake/Foobar2000Sdk.cmake)

add_library(foo_refrain MODULE
  src/artwork_loader.cpp
  src/artwork_loader.h
  src/album_browser_model.h
  src/component.cpp
  src/component_identity.h
  src/compact_track_list_model.h
  src/foo_refrain.rc
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
  src/playlist_browser_preferences.cpp
  src/playlist_view_model.h
  src/now_playing_state.h
  src/now_playing_toolbar.cpp
  src/preferences.cpp
  src/settings.cpp
  src/settings.h
  src/settings_model.h
)

set_target_properties(foo_refrain PROPERTIES
  PREFIX ""
  OUTPUT_NAME "foo_refrain"
)

target_include_directories(foo_refrain PRIVATE src)
target_compile_definitions(foo_refrain PRIVATE
  _UNICODE
  UNICODE
  _WINDLL
  WIN32_LEAN_AND_MEAN
  NOMINMAX
)
target_compile_options(foo_refrain PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8 /external:W0)
target_link_options(foo_refrain PRIVATE /INCREMENTAL:NO)
target_link_libraries(foo_refrain PRIVATE
  refrain_foobar2000_sdk
  refrain_component_client
  refrain_pfc
  refrain_shared
  refrain_columns_ui_sdk
  refrain_libppui_sdk
  comctl32
  d2d1
  dwrite
  windowscodecs
  shlwapi
  shell32
  winmm
)

if(BUILD_TESTING)
  add_executable(refrain_identity_tests tests/component_identity_tests.cpp)
  target_include_directories(refrain_identity_tests PRIVATE src)
  target_compile_definitions(refrain_identity_tests PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(refrain_identity_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME component_identity COMMAND refrain_identity_tests)

  add_executable(refrain_playback_state_tests tests/playback_state_tests.cpp)
  target_include_directories(refrain_playback_state_tests PRIVATE src)
  target_compile_definitions(refrain_playback_state_tests PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(refrain_playback_state_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playback_state COMMAND refrain_playback_state_tests)

  add_executable(refrain_settings_model_tests tests/settings_model_tests.cpp)
  target_include_directories(refrain_settings_model_tests PRIVATE src)
  target_compile_options(refrain_settings_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME settings_model COMMAND refrain_settings_model_tests)

  add_executable(refrain_now_playing_state_tests tests/now_playing_state_tests.cpp)
  target_include_directories(refrain_now_playing_state_tests PRIVATE src)
  target_compile_options(refrain_now_playing_state_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME now_playing_state COMMAND refrain_now_playing_state_tests)

  add_executable(refrain_playlist_view_model_tests tests/playlist_view_model_tests.cpp)
  target_include_directories(refrain_playlist_view_model_tests PRIVATE src)
  target_compile_options(refrain_playlist_view_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_view_model COMMAND refrain_playlist_view_model_tests)

  add_executable(refrain_playlist_grouping_model_tests tests/playlist_grouping_model_tests.cpp)
  target_include_directories(refrain_playlist_grouping_model_tests PRIVATE src)
  target_compile_options(refrain_playlist_grouping_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_grouping_model COMMAND refrain_playlist_grouping_model_tests)

  add_executable(refrain_playlist_interaction_model_tests tests/playlist_interaction_model_tests.cpp)
  target_include_directories(refrain_playlist_interaction_model_tests PRIVATE src)
  target_compile_options(refrain_playlist_interaction_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_interaction_model COMMAND refrain_playlist_interaction_model_tests)

  add_executable(refrain_album_browser_model_tests tests/album_browser_model_tests.cpp)
  target_include_directories(refrain_album_browser_model_tests PRIVATE src)
  target_compile_options(refrain_album_browser_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME album_browser_model COMMAND refrain_album_browser_model_tests)

  add_executable(refrain_playlist_browser_model_tests tests/playlist_browser_model_tests.cpp)
  target_include_directories(refrain_playlist_browser_model_tests PRIVATE src)
  target_compile_definitions(refrain_playlist_browser_model_tests PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
  )
  target_compile_options(refrain_playlist_browser_model_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  add_test(NAME playlist_browser_model COMMAND refrain_playlist_browser_model_tests)
endif()

find_program(REFRAIN_POWERSHELL_EXECUTABLE NAMES powershell REQUIRED)

add_custom_target(package_component
  COMMAND "${REFRAIN_POWERSHELL_EXECUTABLE}"
    -NoProfile
    -ExecutionPolicy Bypass
    -File "${CMAKE_SOURCE_DIR}/scripts/package-component.ps1"
    -RepositoryRoot "${CMAKE_SOURCE_DIR}"
    -ComponentDll "$<TARGET_FILE:foo_refrain>"
    -OutputDirectory "${CMAKE_SOURCE_DIR}/dist"
  DEPENDS foo_refrain
  VERBATIM
)
