find_package(EdupalsBase REQUIRED)
find_package(PkgConfig REQUIRED)

include_directories(${EDUPALS_BASE_INCLUDE_DIRS})

add_library(llxgvagate SHARED libllxgvagate.cpp http.cpp filedb.cpp)
target_link_libraries(llxgvagate Edupals::Base ${CURL_LIBRARIES})
set_target_properties(llxgvagate PROPERTIES SOVERSION 1 VERSION "1.0.0")
install(TARGETS llxgvagate LIBRARY DESTINATION "lib")
