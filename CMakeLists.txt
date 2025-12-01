cmake_minimum_required(VERSION 3.17)

project(SentinelFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

# Option to skip system directory installations (useful for CI/CD)
option(SKIP_SYSTEM_INSTALL "Skip installation to system directories" OFF)

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
if(NOT SKIP_SYSTEM_INSTALL)
  install(FILES config/sentinel.conf
          DESTINATION ${SENTINEL_CONFIG_INSTALL_DIR}
          RENAME sentinel.conf.example
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ)

  set(_sentinel_default_conf "${CMAKE_CURRENT_SOURCE_DIR}/config/sentinel.conf")
  install(CODE "
    file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/sentinelfs\")
    set(_conf_file \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/sentinelfs/sentinel.conf\")
    if(NOT EXISTS \"\${_conf_file}\")
      file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/sentinelfs\"
           TYPE FILE
           FILES \"${_sentinel_default_conf}\"
           RENAME sentinel.conf
           PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ)
    endif()
  ")

  install(FILES ${SENTINEL_GENERATED_DIR}/sentinel_daemon.service
          DESTINATION ${SENTINEL_SYSTEMD_DIR}
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
else()
  message(STATUS "Skipping system directory installations (SKIP_SYSTEM_INSTALL=ON)")
endif()

# CPack configuration for traditional packages
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
else()
    set(CPACK_GENERATOR "TGZ")
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
set(CPACK_SET_DESTDIR ON)
include(CPack)

# Custom target for building AppImage with GUI
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_target(appimage
        COMMAND ${CMAKE_COMMAND} -E echo "Building AppImage with GUI and daemon..."
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/build_appimage.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Creating SentinelFS AppImage"
    )
endif()
