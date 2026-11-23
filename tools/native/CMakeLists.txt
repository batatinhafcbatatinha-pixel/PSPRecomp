# Compile both host libraries without any locally generated game sources.
include("${CMAKE_CURRENT_LIST_DIR}/posix.cmake")
set(native_host_index 0)
foreach(host_dir IN ITEMS
        "${PROJECT_SOURCE_DIR}/profiles/ctw/host"
        "${PROJECT_SOURCE_DIR}/profiles/vcs/host")
    math(EXPR native_host_index "${native_host_index} + 1")
    file(GLOB host_sources CONFIGURE_DEPENDS "${host_dir}/*.cpp")
    set(host_target "psprecomp_native_host_${native_host_index}")
    add_library(${host_target} STATIC ${host_sources})
    target_include_directories(${host_target} PRIVATE "${host_dir}")
    target_link_libraries(${host_target} PRIVATE psprecomp_core ${CMAKE_DL_LIBS})
    psprecomp_posix_host(${host_target} "${host_dir}")
endforeach()
