find_package(EdupalsBase REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)

include_directories(${EDUPALS_BASE_INCLUDE_DIRS})

add_library(llxgvagate SHARED libllxgvagate.cpp http.cpp hash.cpp)
target_link_libraries(llxgvagate Edupals::Base OpenSSL::SSL ${CURL_LIBRARIES})
set_target_properties(llxgvagate PROPERTIES SOVERSION 1 VERSION "1.0.0")
