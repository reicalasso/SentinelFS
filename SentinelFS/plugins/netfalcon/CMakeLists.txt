# CMake configuration for NetFalcon network plugin
cmake_minimum_required(VERSION 3.10)

project(NetFalconPlugin)

add_library(netfalcon SHARED
    src/NetFalconPlugin.cpp
    src/TransportRegistry.cpp
    src/SessionManager.cpp
    src/DiscoveryService.cpp
    src/TCPTransport.cpp
    src/RelayTransport.cpp
    src/QUICTransport.cpp
    src/WebRTCTransport.cpp
)

# Enable QUIC with ngtcp2 if available
find_library(NGTCP2_LIB ngtcp2)
find_library(NGTCP2_CRYPTO_GNUTLS ngtcp2_crypto_gnutls)
find_library(GNUTLS_LIB gnutls)

if(NGTCP2_LIB AND NGTCP2_CRYPTO_GNUTLS AND GNUTLS_LIB)
    message(STATUS "ngtcp2 + GnuTLS found - enabling QUIC support")
    target_compile_definitions(netfalcon PRIVATE HAVE_NGTCP2 HAVE_GNUTLS)
    target_link_libraries(netfalcon PRIVATE ${NGTCP2_LIB} ${NGTCP2_CRYPTO_GNUTLS} ${GNUTLS_LIB})
else()
    message(STATUS "ngtcp2/GnuTLS not found - QUIC support disabled")
endif()

# Enable WebRTC with libdatachannel if available
find_library(DATACHANNEL_LIB datachannel)
if(DATACHANNEL_LIB)
    message(STATUS "libdatachannel found - enabling WebRTC support")
    target_compile_definitions(netfalcon PRIVATE HAVE_WEBRTC)
    target_link_libraries(netfalcon PRIVATE ${DATACHANNEL_LIB})
else()
    message(STATUS "libdatachannel not found - WebRTC support disabled")
endif()

target_include_directories(netfalcon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
)

target_link_libraries(netfalcon PRIVATE sentinel_core)

# Set output name to match plugin loading convention
set_target_properties(netfalcon PROPERTIES
    OUTPUT_NAME "netfalcon"
    PREFIX "lib"
)
