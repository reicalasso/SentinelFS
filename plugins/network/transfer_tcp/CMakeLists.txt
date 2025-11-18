add_library(transfer_tcp SHARED
    transfer_tcp.cpp
)

target_include_directories(transfer_tcp PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(transfer_tcp PRIVATE
    peer_registry
    pthread
)

set_target_properties(transfer_tcp PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
