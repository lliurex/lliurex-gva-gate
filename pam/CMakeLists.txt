include_directories(${CMAKE_SOURCE_DIR}/lib)

find_package(EdupalsBase REQUIRED)

add_library(pam_llxgvagate SHARED pam.cpp)
target_link_libraries(pam_llxgvagate llxgvagate Edupals::Base -lpam -lpam_misc)
set_target_properties(pam_llxgvagate PROPERTIES PREFIX "")

install(TARGETS pam_llxgvagate LIBRARY DESTINATION "lib/security")
