#
# Arrow Support
#
option(WITH_ARROW
    "Build support for Apache Arrow" FALSE)
if (WITH_ARROW)
    find_package(Arrow REQUIRED)
    set_package_properties(Arrow PROPERTIES TYPE REQUIRED
            PURPOSE "Apache Arrow")
    if(ARROW_FOUND)
        set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES}
            "${ARROW_SHARED_LIB}")
        include_directories(${ARROW_INCLUDE_DIR})
        mark_as_advanced(CLEAR ARROW_INCLUDE_DIR)
        mark_as_advanced(CLEAR ARROW_SHARED_LIB)
        set(PDAL_HAVE_ARROW 1)
    endif()
endif()
