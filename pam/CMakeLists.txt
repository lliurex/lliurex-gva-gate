include_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(EdupalsBase REQUIRED)
find_package(PkgConfig REQUIRED)
#pkg_check_modules(SYSTEMD REQUIRED libsystemd)

add_library(pam_gvagate SHARED pam.cpp)
target_link_libraries(pam_gvagate Edupals::Base -lpam -lpam_misc)
set_target_properties(pam_gvagate PROPERTIES SOVERSION 2 VERSION "2.0.0")

install(TARGETS pam_gvagate LIBRARY DESTINATION "lib")
