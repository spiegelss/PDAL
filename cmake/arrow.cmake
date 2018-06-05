#
# Arrow Support
#
find_package(Arrow)
set_package_properties(Arrow PROPERTIES TYPE OPTIONAL
    PURPOSE "Provides data access with Apache Arrow")
if (ARROW_FOUND)
    include_directories("${ARROW_INCLUDE_DIR}")
endif()
