# Linux/macOS host dependencies (SDL2 window/audio/input and FFmpeg), shared by every profile.
# The Direct3D 12 renderer exists only on Windows: here the host uses its software GE rasterizer
# (the non-Windows stubs in ge_gpu_backend_dx12.cpp and dx12_presenter.cpp) and tools/native/display.cpp
# shows the finished frame through SDL2.
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

set(PSPRECOMP_HOST_SDL_TARGET "")
if(PkgConfig_FOUND)
    pkg_check_modules(HOST_SDL IMPORTED_TARGET GLOBAL sdl2>=2.0.10)
    if(HOST_SDL_FOUND)
        set(PSPRECOMP_HOST_SDL_TARGET PkgConfig::HOST_SDL)
    endif()
endif()
if(NOT PSPRECOMP_HOST_SDL_TARGET)
    find_package(SDL2 QUIET)
    if(TARGET SDL2::SDL2)
        set(PSPRECOMP_HOST_SDL_TARGET SDL2::SDL2)
    endif()
endif()
if(NOT PSPRECOMP_HOST_SDL_TARGET)
    message(FATAL_ERROR "SDL2 development files not found (Linux: libsdl2-dev / SDL2-devel, macOS: brew install sdl2)")
endif()

# FFmpeg: pkg-config first. Without it, look for the libraries by name, including the versioned
# runtime sonames of FFmpeg 7 for hosts that install no development symlinks, and keep the
# FFmpeg 7 headers the profile ships.
set(PSPRECOMP_HOST_FFMPEG_PKG OFF)
if(PkgConfig_FOUND)
    pkg_check_modules(HOST_MEDIA IMPORTED_TARGET GLOBAL
        libavcodec libavformat libavutil libswresample libswscale)
    if(HOST_MEDIA_FOUND)
        set(PSPRECOMP_HOST_FFMPEG_PKG ON)
    endif()
endif()
set(PSPRECOMP_HOST_FFMPEG_LIBS)
if(NOT PSPRECOMP_HOST_FFMPEG_PKG)
    set(PSPRECOMP_FFMPEG_avcodec_NAMES avcodec libavcodec.so.61)
    set(PSPRECOMP_FFMPEG_avformat_NAMES avformat libavformat.so.61)
    set(PSPRECOMP_FFMPEG_avutil_NAMES avutil libavutil.so.59)
    set(PSPRECOMP_FFMPEG_swresample_NAMES swresample libswresample.so.5)
    set(PSPRECOMP_FFMPEG_swscale_NAMES swscale libswscale.so.8)
    foreach(lib avcodec avformat avutil swresample swscale)
        find_library(PSPRECOMP_SYSTEM_${lib} NAMES ${PSPRECOMP_FFMPEG_${lib}_NAMES})
        if(NOT PSPRECOMP_SYSTEM_${lib})
            message(FATAL_ERROR "FFmpeg library '${lib}' not found (Linux: libav*-dev, macOS: brew install ffmpeg)")
        endif()
        list(APPEND PSPRECOMP_HOST_FFMPEG_LIBS "${PSPRECOMP_SYSTEM_${lib}}")
    endforeach()
endif()

function(psprecomp_posix_host target host_dir)
    get_target_property(host_sources ${target} SOURCES)
    list(FILTER host_sources EXCLUDE REGEX "(^|/)(display_window|audio_output)\.cpp$")
    set_property(TARGET ${target} PROPERTY SOURCES "${host_sources}")
    if(PSPRECOMP_HOST_FFMPEG_PKG)
        # Use the headers that match the system libraries, not the vendored ones.
        get_target_property(host_includes ${target} INCLUDE_DIRECTORIES)
        if(host_includes)
            list(FILTER host_includes EXCLUDE REGEX "third_party/ffmpeg/include$")
            set_property(TARGET ${target} PROPERTY INCLUDE_DIRECTORIES "${host_includes}")
        endif()
        target_link_libraries(${target} PRIVATE PkgConfig::HOST_MEDIA)
    else()
        target_include_directories(${target} PRIVATE "${host_dir}/../third_party/ffmpeg/include")
        target_link_libraries(${target} PRIVATE ${PSPRECOMP_HOST_FFMPEG_LIBS})
    endif()
    target_sources(${target} PRIVATE
        "${PROJECT_SOURCE_DIR}/tools/native/display.cpp"
        "${PROJECT_SOURCE_DIR}/tools/native/audio.cpp")
    target_include_directories(${target} PRIVATE "${host_dir}")
    target_link_libraries(${target} PRIVATE ${PSPRECOMP_HOST_SDL_TARGET} Threads::Threads)
    foreach(source IN LISTS host_sources)
        if(source MATCHES "(^|/)main\.cpp$")
            # The guest entry runs on its own big-stack thread; tools/native/main.cpp owns main().
            set_property(SOURCE "${source}" TARGET_DIRECTORY ${target}
                APPEND PROPERTY COMPILE_DEFINITIONS main=psprecomp_guest_main)
            target_sources(${target} PRIVATE "${PROJECT_SOURCE_DIR}/tools/native/main.cpp")
        endif()
    endforeach()
endfunction()
