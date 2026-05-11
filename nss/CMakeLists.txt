
include_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(EdupalsBase REQUIRED)

add_library(nss_llxgvagate SHARED gate.cpp)
target_link_libraries(nss_llxgvagate llxgvagate Edupals::Base)
set_target_properties(nss_llxgvagate PROPERTIES SOVERSION 2 VERSION "2.0.0")

install(TARGETS nss_llxgvagate LIBRARY DESTINATION "lib")

add_library(nss_llxgvaborder SHARED border.cpp)
target_link_libraries(nss_llxgvaborder llxgvagate Edupals::Base)
set_target_properties(nss_llxgvaborder PROPERTIES SOVERSION 2 VERSION "2.0.0")

install(TARGETS nss_llxgvaborder LIBRARY DESTINATION "lib")
