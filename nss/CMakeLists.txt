
include_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(EdupalsBase REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

add_library(nss_gvagate SHARED nss.cpp)
target_link_libraries(nss_gvagate Edupals::Base ${SYSTEMD_LIBRARIES})
set_target_properties(nss_gvagate PROPERTIES SOVERSION 2 VERSION "2.0.0")

install(TARGETS nss_gvagate LIBRARY DESTINATION "lib")
