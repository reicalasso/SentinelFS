add_library(discovery_udp SHARED
    discovery_udp.cpp
)

target_include_directories(discovery_udp PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(discovery_udp PRIVATE
    peer_registry
    pthread
)

set_target_properties(discovery_udp PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
