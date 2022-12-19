
include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(llx-gva-gate llxgvagate.cpp)
target_link_libraries(llx-gva-gate llxgvagate)
