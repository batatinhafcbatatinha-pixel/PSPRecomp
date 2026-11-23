# Generic profile: any PSP game, built from the user's own disc by tools/pspport (the launcher's path for discs
# with no profile of their own). It has no game-specific code: the PSP HLE, GE renderer, audio and input are the
# host shared with the ctw profile (profiles/ctw/host, whose address-specific hooks are not installed), linked with
# the psp_recomp output for the game's decrypted EBOOT.
set(GENERIC_PROFILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(GENERIC_HOST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ctw")

set(GENERIC_GENERATED_DIR "" CACHE PATH "Directory holding the psp_recomp --auto output for the game")
set(GENERIC_TITLE "PSPRecomp" CACHE STRING "Window title (the game's name)")
if(NOT EXISTS "${GENERIC_GENERATED_DIR}/generated_registry.cpp")
    message(FATAL_ERROR "No generated code in '${GENERIC_GENERATED_DIR}'; run tools/pspport/pspport.py prepare first")
endif()

file(GLOB GENERIC_GENERATED CONFIGURE_DEPENDS "${GENERIC_GENERATED_DIR}/*.cpp")
if(MSVC)
    set_source_files_properties(${GENERIC_GENERATED} PROPERTIES COMPILE_OPTIONS
        "/O${PSPRECOMP_GENERATED_OPT_LEVEL};/Ob0;/bigobj;/GL-")
else()
    set_source_files_properties(${GENERIC_GENERATED} PROPERTIES COMPILE_OPTIONS
        "-O${PSPRECOMP_GENERATED_OPT_LEVEL};-g0")
endif()

set(GENERIC_HOST_SOURCES
    host/vcs_bootstrap_paths.cpp
    host/vcs_profile.cpp
    host/ctw_hle.cpp
    host/vcs_native_fast_paths.cpp
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
list(TRANSFORM GENERIC_HOST_SOURCES PREPEND "${GENERIC_HOST_DIR}/")

add_executable(PSPNative
    "${GENERIC_HOST_DIR}/host/main.cpp"
    ${GENERIC_HOST_SOURCES}
    ${GENERIC_GENERATED}
)
target_include_directories(PSPNative PRIVATE
    "${GENERIC_HOST_DIR}/host"
    "${GENERIC_GENERATED_DIR}"
    "${GENERIC_HOST_DIR}/third_party/ffmpeg/include")
target_link_libraries(PSPNative PRIVATE psprecomp_core ${CMAKE_DL_LIBS})
target_compile_definitions(PSPNative PRIVATE
    PSPRECOMP_GENERIC_HOST=1
    PSPRECOMP_HOST_INI="PSPNative.ini"
    PSPRECOMP_HOST_TITLE=L"${GENERIC_TITLE}")
set_target_properties(PSPNative PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
# port_input (third_party/port_input, shared with the other recompilers): every controller SDL3 knows, with the
# player's controller choice, remaps, dead zones and sensitivities (display_window.cpp read_host_pad).
if(WIN32)
    set(GENERIC_PORT_INPUT "${CMAKE_CURRENT_LIST_DIR}/../../third_party/port_input")
    target_sources(PSPNative PRIVATE "${GENERIC_PORT_INPUT}/port_input.c")
    # The project has no C compiler enabled: port_input.c is also valid C++.
    set_source_files_properties("${GENERIC_PORT_INPUT}/port_input.c" PROPERTIES LANGUAGE CXX)
    target_include_directories(PSPNative PRIVATE "${GENERIC_PORT_INPUT}")
    target_compile_definitions(PSPNative PRIVATE PSP_PORT_INPUT=1)
    include("${GENERIC_PORT_INPUT}/port_input.cmake")
    port_input_stage(PSPNative)
    # port_dlss (third_party/port_gfx): UpscaleFilter=DLSS in the DX12 present pass. C, built as C++ here.
    set(GENERIC_PORT_GFX "${CMAKE_CURRENT_LIST_DIR}/../../third_party/port_gfx")
    target_sources(PSPNative PRIVATE "${GENERIC_PORT_GFX}/port_dlss.c")
    set_source_files_properties("${GENERIC_PORT_GFX}/port_dlss.c" PROPERTIES LANGUAGE CXX)
    target_include_directories(PSPNative PRIVATE "${GENERIC_PORT_GFX}")
    target_compile_definitions(PSPNative PRIVATE PORT_DLSS=1)
endif()
psprecomp_enable_host_avx(PSPNative)

if(WIN32)
    foreach(LIB avcodec avformat avutil swresample swscale)
        target_link_libraries(PSPNative PRIVATE "${GENERIC_HOST_DIR}/third_party/ffmpeg/lib/${LIB}.lib")
    endforeach()
    target_link_libraries(PSPNative PRIVATE winmm d3d12 dxgi d3dcompiler dxguid)
    add_custom_command(TARGET PSPNative POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${GENERIC_HOST_DIR}/third_party/ffmpeg/bin" "$<TARGET_FILE_DIR:PSPNative>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GENERIC_HOST_DIR}/third_party/ffmpeg/COPYING.LGPLv2.1"
            "$<TARGET_FILE_DIR:PSPNative>/COPYING.LGPLv2.1")
else()
    foreach(LIB avcodec avformat avutil swresample swscale)
        find_library(GENERIC_SYSTEM_${LIB} NAMES ${LIB} REQUIRED)
        target_link_libraries(PSPNative PRIVATE ${GENERIC_SYSTEM_${LIB}})
    endforeach()
endif()

if(MSVC)
    target_link_options(PSPNative PRIVATE /STACK:67108864)
elseif(MINGW)
    target_link_options(PSPNative PRIVATE -Wl,--stack,67108864 -static)
endif()

# The settings file is only written when missing, so the player's changes survive a rebuild.
add_custom_command(TARGET PSPNative POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DSRC="${GENERIC_PROFILE_DIR}/config/PSPNative.ini" -DDST="$<TARGET_FILE_DIR:PSPNative>/PSPNative.ini"
            -P "${GENERIC_PROFILE_DIR}/copy_if_missing.cmake"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GENERIC_HOST_DIR}/config/ProperShaders.ini" "$<TARGET_FILE_DIR:PSPNative>/ProperShaders.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${GENERIC_HOST_DIR}/third_party/cloudworks/ATTRIBUTION.md"
        "$<TARGET_FILE_DIR:PSPNative>/CLOUDWORKS_ATTRIBUTION.md")
