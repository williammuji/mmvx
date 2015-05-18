include_directories(${PROJECT_BINARY_DIR})

add_executable(forwardserver ForwardServer.cc KeeperClient.cc)
set_target_properties(forwardserver PROPERTIES COMPILE_FLAGS "-Wno-shadow -Wno-conversion")
target_link_libraries(forwardserver muduo_protobuf_codec mmvx_proto)

install(TARGETS forwardserver DESTINATION bin)
