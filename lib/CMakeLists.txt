find_package(EdupalsBase REQUIRED)

include_directories(${EDUPALS_BASE_INCLUDE_DIRS})

add_library(llxgvagate SHARED libllxgvagate.cpp)
target_link_libraries(llxgvagate Edupals::Base)
set_target_properties(llxgvagate PROPERTIES SOVERSION 1 VERSION "1.0.0")
