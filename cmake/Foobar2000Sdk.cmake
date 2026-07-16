set(FOOCRATE_THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party")
cmake_path(NORMAL_PATH FOOCRATE_THIRD_PARTY_DIR)

set(FOOCRATE_PFC_DIR "${FOOCRATE_THIRD_PARTY_DIR}/pfc")
set(FOOCRATE_FOOBAR2000_DIR "${FOOCRATE_THIRD_PARTY_DIR}/foobar2000")
set(FOOCRATE_COLUMNS_UI_SDK_DIR "${FOOCRATE_THIRD_PARTY_DIR}/columns_ui-sdk-8.1.0")
set(FOOCRATE_LIBPPUI_DIR "${FOOCRATE_THIRD_PARTY_DIR}/libPPUI")

foreach(required_path IN ITEMS
  "${FOOCRATE_PFC_DIR}/pfc.h"
  "${FOOCRATE_FOOBAR2000_DIR}/SDK/foobar2000.h"
  "${FOOCRATE_FOOBAR2000_DIR}/foobar2000_component_client/component_client.cpp"
  "${FOOCRATE_COLUMNS_UI_SDK_DIR}/ui_extension.h"
  "${FOOCRATE_LIBPPUI_DIR}/CListControl.h"
)
  if(NOT EXISTS "${required_path}")
    message(FATAL_ERROR "Required SDK file is missing: ${required_path}")
  endif()
endforeach()

set(FOOCRATE_PFC_SOURCES
  audio_math.cpp
  audio_sample.cpp
  base64.cpp
  bigmem.cpp
  bit_array.cpp
  bsearch.cpp
  charDownConvert.cpp
  cpuid.cpp
  crashWithMessage.cpp
  filehandle.cpp
  filetimetools.cpp
  guid.cpp
  nix-objects.cpp
  other.cpp
  pathUtils.cpp
  pfc-fb2k-hooks.cpp
  printf.cpp
  selftest.cpp
  SmartStrStr.cpp
  sort.cpp
  splitString2.cpp
  stdafx.cpp
  string-compare.cpp
  string-conv-lite.cpp
  string-lite.cpp
  string_base.cpp
  string_conv.cpp
  threads.cpp
  timers.cpp
  unicode-normalize.cpp
  utf8.cpp
  wildcard.cpp
  win-objects.cpp
)
list(TRANSFORM FOOCRATE_PFC_SOURCES PREPEND "${FOOCRATE_PFC_DIR}/")

add_library(foocrate_pfc STATIC ${FOOCRATE_PFC_SOURCES})
target_include_directories(foocrate_pfc SYSTEM PUBLIC "${FOOCRATE_THIRD_PARTY_DIR}")
target_compile_definitions(foocrate_pfc PUBLIC _UNICODE UNICODE NOMINMAX)
target_compile_options(foocrate_pfc PRIVATE /W3 /permissive- /Zc:__cplusplus)

set(FOOCRATE_FOOBAR2000_SDK_SOURCES
  abort_callback.cpp
  advconfig.cpp
  album_art.cpp
  app_close_blocker.cpp
  audio_chunk.cpp
  audio_chunk_channel_config.cpp
  cfg_var.cpp
  cfg_var_legacy.cpp
  chapterizer.cpp
  commandline.cpp
  commonObjects.cpp
  completion_notify.cpp
  componentversion.cpp
  configStore.cpp
  config_io_callback.cpp
  config_object.cpp
  console.cpp
  dsp.cpp
  dsp_manager.cpp
  file_cached_impl.cpp
  file_info.cpp
  file_info_const_impl.cpp
  file_info_impl.cpp
  file_info_merge.cpp
  file_operation_callback.cpp
  filesystem.cpp
  filesystem_helper.cpp
  foosort.cpp
  fsItem.cpp
  guids.cpp
  hasher_md5.cpp
  image.cpp
  input.cpp
  input_file_type.cpp
  link_resolver.cpp
  mainmenu.cpp
  main_thread_callback.cpp
  mem_block_container.cpp
  menu_helpers.cpp
  menu_item.cpp
  menu_manager.cpp
  metadb.cpp
  metadb_handle.cpp
  metadb_handle_list.cpp
  output.cpp
  packet_decoder.cpp
  playable_location.cpp
  playback_control.cpp
  playlist.cpp
  playlist_loader.cpp
  popup_message.cpp
  preferences_page.cpp
  replaygain.cpp
  replaygain_info.cpp
  service.cpp
  stdafx.cpp
  tag_processor.cpp
  tag_processor_id3v2.cpp
  threaded_process.cpp
  titleformat.cpp
  track_property.cpp
  ui.cpp
  ui_element.cpp
  utility.cpp
)
list(TRANSFORM FOOCRATE_FOOBAR2000_SDK_SOURCES PREPEND "${FOOCRATE_FOOBAR2000_DIR}/SDK/")

add_library(foocrate_foobar2000_sdk STATIC ${FOOCRATE_FOOBAR2000_SDK_SOURCES})
target_include_directories(foocrate_foobar2000_sdk SYSTEM PUBLIC
  "${FOOCRATE_THIRD_PARTY_DIR}"
  "${FOOCRATE_FOOBAR2000_DIR}"
)
target_compile_definitions(foocrate_foobar2000_sdk PUBLIC _UNICODE UNICODE NOMINMAX)
target_compile_options(foocrate_foobar2000_sdk PRIVATE /W3 /permissive- /Zc:__cplusplus)
target_link_libraries(foocrate_foobar2000_sdk PUBLIC foocrate_pfc)

add_library(foocrate_component_client STATIC
  "${FOOCRATE_FOOBAR2000_DIR}/foobar2000_component_client/component_client.cpp"
)
target_include_directories(foocrate_component_client SYSTEM PUBLIC
  "${FOOCRATE_THIRD_PARTY_DIR}"
  "${FOOCRATE_FOOBAR2000_DIR}"
)
target_compile_definitions(foocrate_component_client PUBLIC _UNICODE UNICODE NOMINMAX)
target_compile_options(foocrate_component_client PRIVATE /W3 /permissive- /Zc:__cplusplus)
target_link_libraries(foocrate_component_client PUBLIC foocrate_foobar2000_sdk foocrate_pfc)

set(FOOCRATE_SHARED_SOURCES
  audio_math.cpp
  crash_info.cpp
  filedialogs.cpp
  filedialogs_vista.cpp
  font_description.cpp
  minidump.cpp
  modal_dialog.cpp
  stdafx.cpp
  systray.cpp
  text_drawing.cpp
  utf8.cpp
  utf8api.cpp
  Utility.cpp
)
list(TRANSFORM FOOCRATE_SHARED_SOURCES PREPEND "${FOOCRATE_FOOBAR2000_DIR}/shared/")

add_library(foocrate_shared SHARED ${FOOCRATE_SHARED_SOURCES})
set_target_properties(foocrate_shared PROPERTIES OUTPUT_NAME "shared")
target_include_directories(foocrate_shared SYSTEM PUBLIC
  "${FOOCRATE_THIRD_PARTY_DIR}"
  "${FOOCRATE_FOOBAR2000_DIR}"
)
target_compile_definitions(foocrate_shared PRIVATE
  _CRT_SECURE_NO_WARNINGS
  SHARED_EXPORTS
  _WINDLL
  _UNICODE
  UNICODE
  NOMINMAX
)
target_compile_options(foocrate_shared PRIVATE /W3 /permissive- /Zc:__cplusplus)
target_link_libraries(foocrate_shared PRIVATE
  foocrate_pfc
  comctl32
  imagehlp
  uxtheme
  dbghelp
)

add_library(foocrate_columns_ui_sdk STATIC
  "${FOOCRATE_COLUMNS_UI_SDK_DIR}/container_window_v3.cpp"
  "${FOOCRATE_COLUMNS_UI_SDK_DIR}/ui_extension.cpp"
  "${FOOCRATE_COLUMNS_UI_SDK_DIR}/win32_helpers.cpp"
)
target_include_directories(foocrate_columns_ui_sdk SYSTEM PUBLIC
  "${FOOCRATE_THIRD_PARTY_DIR}"
  "${FOOCRATE_COLUMNS_UI_SDK_DIR}"
)
target_compile_definitions(foocrate_columns_ui_sdk PUBLIC _UNICODE UNICODE NOMINMAX)
target_compile_options(foocrate_columns_ui_sdk PRIVATE /W3 /permissive- /Zc:__cplusplus)
target_link_libraries(foocrate_columns_ui_sdk PUBLIC foocrate_foobar2000_sdk foocrate_pfc foocrate_shared shlwapi)

# libPPUI requires WTL only when its concrete controls are built. Step 03 establishes
# the stable include target; later UI code can opt into concrete sources as needed.
add_library(foocrate_libppui_sdk INTERFACE)
target_include_directories(foocrate_libppui_sdk SYSTEM INTERFACE
  "${FOOCRATE_THIRD_PARTY_DIR}"
  "${FOOCRATE_LIBPPUI_DIR}"
)
target_link_libraries(foocrate_libppui_sdk INTERFACE foocrate_pfc)
