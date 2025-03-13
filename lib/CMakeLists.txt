find_package(EdupalsBase REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CRYPT REQUIRED libcrypt)

include_directories(${EDUPALS_BASE_INCLUDE_DIRS})

add_library(llxgvagate SHARED libllxgvagate.cpp filedb.cpp exec.cpp)
target_link_libraries(llxgvagate Edupals::Base ${CRYPT_LIBRARIES})
set_target_properties(llxgvagate PROPERTIES SOVERSION 1 VERSION "1.0.0")
install(TARGETS llxgvagate LIBRARY DESTINATION "lib")

install(FILES "libllxgvagate.hpp"
    DESTINATION "include/lliurex/gvagate"
)
