set(CTW_PROFILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# The AOT corpus is generated locally from the user's own decrypted EBOOT and is
# never part of the source tree.
set(CTW_GENERATED_DIR "${CTW_PROFILE_DIR}/generated" CACHE PATH
    "Directory holding the psp_recomp output for the Chinatown Wars executable")
if(NOT EXISTS "${CTW_GENERATED_DIR}/generated_registry.cpp")
    message(FATAL_ERROR "No generated CTW code in ${CTW_GENERATED_DIR}; run psp_recomp --auto first")
endif()

set(PSPRECOMP_GENERATED_INLINE_LEVEL "0" CACHE STRING
    "MSVC /Ob level for generated CTW AOT units (0, 1, 2 or 3)")

file(GLOB CTW_GENERATED CONFIGURE_DEPENDS "${CTW_GENERATED_DIR}/*.cpp")

# Code overlays from OVERLAYS.PAK (psp_recomp --overlay, see gtactw scripts),
# each one routed at run time by Runtime::register_overlay().  Optional.
set(CTW_OVERLAY_GENERATED_DIR "" CACHE PATH
    "Directory holding psp_recomp --overlay output and ovl_registry.cpp")
set(CTW_HAS_OVERLAYS OFF)
if(CTW_OVERLAY_GENERATED_DIR AND EXISTS "${CTW_OVERLAY_GENERATED_DIR}/ovl_registry.cpp")
    file(GLOB CTW_OVERLAY_GENERATED CONFIGURE_DEPENDS "${CTW_OVERLAY_GENERATED_DIR}/*.cpp")
    list(APPEND CTW_GENERATED ${CTW_OVERLAY_GENERATED})
    set(CTW_HAS_OVERLAYS ON)
endif()
if(MSVC)
    if(PSPRECOMP_GENERATED_OPT_LEVEL STREQUAL "0")
        set(CTW_GENERATED_MSVC_OPT "/Od")
    elseif(PSPRECOMP_GENERATED_OPT_LEVEL STREQUAL "3")
        set(CTW_GENERATED_MSVC_OPT "/Ox")
    else()
        set(CTW_GENERATED_MSVC_OPT "/O${PSPRECOMP_GENERATED_OPT_LEVEL}")
    endif()
    set(CTW_GENERATED_MSVC_OPTIONS
        "${CTW_GENERATED_MSVC_OPT};/Ob${PSPRECOMP_GENERATED_INLINE_LEVEL};/bigobj;/GL-")
    set_source_files_properties(${CTW_GENERATED} PROPERTIES COMPILE_OPTIONS
        "${CTW_GENERATED_MSVC_OPTIONS}")
else()
    set_source_files_properties(${CTW_GENERATED} PROPERTIES COMPILE_OPTIONS
        "-O${PSPRECOMP_GENERATED_OPT_LEVEL};-g0")
endif()

# The host is shared with the VCS profile's PSP HLE/GE implementation; the VCS
# address-specific hooks are not installed for this executable.
set(CTW_HOST_SOURCES
    host/vcs_bootstrap_paths.cpp
    host/vcs_profile.cpp
    host/ctw_hle.cpp
    host/vcs_native_fast_paths.cpp
    host/ctw_native_inflate.cpp
    host/framebuffer_capture.cpp
    host/display_window.cpp
    host/audio_output.cpp
    host/vcs_config.cpp
    host/vcs_camera_input.cpp
    host/vcs_vehicle_input.cpp
    host/vcs_fps_overlay.cpp
    host/vcs_media_decoder.cpp
    host/vcs_project2dfx.cpp
    host/vcs_project2dfx_lights.cpp
    host/vcs_draw_distance_patch.cpp
    host/vcs_runtime_log.cpp
    host/vcs_hdr_post_dx12_stub.cpp
    host/ge_renderer.cpp
    host/ge_gpu_backend_dx12.cpp
    host/dx12_presenter.cpp
)

set(CTW_APP_RESOURCES)
if(WIN32)
    list(APPEND CTW_APP_RESOURCES resources/CTWNative.rc)
endif()

add_executable(CTWNative
    ${CTW_APP_RESOURCES}
    host/main.cpp
    ${CTW_HOST_SOURCES}
    ${CTW_GENERATED}
)
target_include_directories(CTWNative PRIVATE
    "${CTW_PROFILE_DIR}/host"
    "${CTW_GENERATED_DIR}"
    "${CTW_PROFILE_DIR}/third_party/ffmpeg/include")
target_link_libraries(CTWNative PRIVATE psprecomp_core ${CMAKE_DL_LIBS})
if(CTW_HAS_OVERLAYS)
    target_compile_definitions(CTWNative PRIVATE CTW_HAS_OVERLAYS=1)
endif()
set_target_properties(CTWNative PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
psprecomp_enable_host_avx(CTWNative)

if(WIN32)
    foreach(CTW_FFMPEG_LIB avcodec avformat avutil swresample swscale)
        target_link_libraries(CTWNative PRIVATE
            "${CTW_PROFILE_DIR}/third_party/ffmpeg/lib/${CTW_FFMPEG_LIB}.lib")
    endforeach()
    target_link_libraries(CTWNative PRIVATE winmm d3d12 dxgi d3dcompiler dxguid)
    add_custom_command(TARGET CTWNative POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CTW_PROFILE_DIR}/third_party/ffmpeg/bin" "$<TARGET_FILE_DIR:CTWNative>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CTW_PROFILE_DIR}/third_party/ffmpeg/COPYING.LGPLv2.1"
            "$<TARGET_FILE_DIR:CTWNative>/COPYING.LGPLv2.1")
else()
    foreach(CTW_FFMPEG_LIB avcodec avformat avutil swresample swscale)
        find_library(CTW_SYSTEM_${CTW_FFMPEG_LIB} NAMES ${CTW_FFMPEG_LIB} REQUIRED)
        target_link_libraries(CTWNative PRIVATE ${CTW_SYSTEM_${CTW_FFMPEG_LIB}})
    endforeach()
endif()

if(MSVC)
    set_source_files_properties(host/main.cpp ${CTW_HOST_SOURCES}
        DIRECTORY "${CTW_PROFILE_DIR}" PROPERTIES COMPILE_OPTIONS
        "$<$<CONFIG:Release>:/Ob3>;$<$<CONFIG:RelWithDebInfo>:/Ob3>")
    target_link_options(CTWNative PRIVATE /STACK:67108864)
elseif(MINGW)
    # Same 64 MB main-thread stack as the MSVC build; the C++ runtime linked in so the exe needs no extra DLLs.
    target_link_options(CTWNative PRIVATE -Wl,--stack,67108864 -static)
endif()

add_custom_command(TARGET CTWNative POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CTW_PROFILE_DIR}/config/CTWNative.ini" "$<TARGET_FILE_DIR:CTWNative>/CTWNative.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CTW_PROFILE_DIR}/config/ProperShaders.ini" "$<TARGET_FILE_DIR:CTWNative>/ProperShaders.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CTW_PROFILE_DIR}/third_party/cloudworks/ATTRIBUTION.md"
        "$<TARGET_FILE_DIR:CTWNative>/CLOUDWORKS_ATTRIBUTION.md")
