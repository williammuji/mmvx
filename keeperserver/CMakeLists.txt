include_directories(${PROJECT_BINARY_DIR})

add_executable(keeperserver KeeperServer.cc)
set_target_properties(keeperserver PROPERTIES COMPILE_FLAGS "-Wno-shadow")
target_link_libraries(keeperserver muduo_protobuf_codec mmvx_proto)

install(TARGETS keeperserver DESTINATION bin)
