
include_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(EdupalsBase REQUIRED)

add_library(nss_gvagate SHARED nss.cpp)
target_link_libraries(nss_gvagate llxgvagate Edupals::Base)
set_target_properties(nss_gvagate PROPERTIES SOVERSION 2 VERSION "2.0.0")

install(TARGETS nss_gvagate LIBRARY DESTINATION "lib")
