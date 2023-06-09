cmake_minimum_required(VERSION 3.0)

project(lliurex-gva-gate)

add_subdirectory(lib)
add_subdirectory(bin)
add_subdirectory(nss)
add_subdirectory(pam)

install(FILES "llx-gva-gate.cfg"
    DESTINATION "etc/"
)

install(FILES "llx-gva-gate"
    DESTINATION "usr/share/pam-configs"
)
