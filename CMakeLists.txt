cmake_minimum_required(VERSION 3.17)

project(SentinelFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

set(SENTINEL_CONFIG_INSTALL_DIR
    "${CMAKE_INSTALL_FULL_SYSCONFDIR}/sentinelfs"
    CACHE PATH "Directory for SentinelFS configuration files")
set(SENTINEL_SYSTEMD_DIR
    "/lib/systemd/system"
    CACHE PATH "Systemd unit directory for sentinel_daemon.service")

set(SENTINEL_DAEMON_BIN "${CMAKE_INSTALL_FULL_BINDIR}/sentinel_daemon")
set(SENTINEL_CONFIG_PATH "${SENTINEL_CONFIG_INSTALL_DIR}/sentinel.conf")

set(SENTINEL_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${SENTINEL_GENERATED_DIR}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/include/Version.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/core/include/Version.h
               @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/sentinel_daemon.service
               ${SENTINEL_GENERATED_DIR}/sentinel_daemon.service
               @ONLY)

add_subdirectory(core)
add_subdirectory(plugins/filesystem)
add_subdirectory(plugins/network)
add_subdirectory(plugins/storage)
add_subdirectory(plugins/ml)
add_subdirectory(app/cli)
add_subdirectory(app/daemon)
add_subdirectory(tests)

set(SENTINEL_BINARIES sentinel_daemon sentinel_cli)
set(SENTINEL_LIBS sentinel_core)

install(TARGETS ${SENTINEL_BINARIES}
        RUNTIME DESTINATION bin)

install(TARGETS ${SENTINEL_LIBS}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

install(DIRECTORY core/include/ DESTINATION include)

# Install config template and ensure default config exists with secure perms
install(FILES config/sentinel.conf
        DESTINATION ${SENTINEL_CONFIG_INSTALL_DIR}
        RENAME sentinel.conf.example
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ)

set(_sentinel_default_conf "${CMAKE_CURRENT_SOURCE_DIR}/config/sentinel.conf")
install(CODE "
  file(MAKE_DIRECTORY \"$ENV{DESTDIR}${SENTINEL_CONFIG_INSTALL_DIR}\")
  if(NOT EXISTS \"$ENV{DESTDIR}${SENTINEL_CONFIG_INSTALL_DIR}/sentinel.conf\")
    file(INSTALL DESTINATION \"$ENV{DESTDIR}${SENTINEL_CONFIG_INSTALL_DIR}\"
         TYPE FILE
         FILES \"${_sentinel_default_conf}\"
         RENAME sentinel.conf
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ)
  endif()
")

install(FILES ${SENTINEL_GENERATED_DIR}/sentinel_daemon.service
        DESTINATION ${SENTINEL_SYSTEMD_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB;RPM")
endif()

set(CPACK_PACKAGE_NAME "sentinelfs")
set(CPACK_PACKAGE_VENDOR "SentinelFS")
set(CPACK_PACKAGE_CONTACT "ops@sentinelfs.io")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SentinelFS daemon and CLI for secure delta-sync")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "SentinelFS Maintainers")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl3, libstdc++6")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_REQUIRES "openssl")
include(CPack)
