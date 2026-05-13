
include_directories(${CMAKE_SOURCE_DIR}/lib)

add_executable(llx-gva-gate llxgvagate.cpp)
target_link_libraries(llx-gva-gate llxgvagate)
install(TARGETS llx-gva-gate RUNTIME PERMISSIONS SETUID SETGID OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

add_executable(llx-gva-border llxgvaborder.cpp)
target_link_libraries(llx-gva-border llxgvagate)
install(TARGETS llx-gva-border RUNTIME PERMISSIONS SETUID SETGID OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
