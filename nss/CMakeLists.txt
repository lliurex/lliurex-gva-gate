
include_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(EdupalsBase REQUIRED)

add_library(nss_llxgvagate SHARED nss.cpp)
target_link_libraries(nss_llxgvagate llxgvagate Edupals::Base)
set_target_properties(nss_llxgvagate PROPERTIES SOVERSION 2 VERSION "2.0.0")

install(TARGETS nss_llxgvagate LIBRARY DESTINATION "lib")
