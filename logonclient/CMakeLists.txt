include_directories(${PROJECT_BINARY_DIR})

add_executable(logonclient LogonClient.cc ForwardClient.cc)
set_target_properties(logonclient PROPERTIES COMPILE_FLAGS "-Wno-shadow")
target_link_libraries(logonclient muduo_protobuf_codec mmvx_proto)

install(TARGETS logonclient DESTINATION bin)
