include_directories(${PROJECT_BINARY_DIR})

add_executable(hubserver HubServer.cc)
set_target_properties(hubserver PROPERTIES COMPILE_FLAGS "-Wno-shadow")
target_link_libraries(hubserver muduo_protobuf_codec mmvx_proto)

install(TARGETS hubserver DESTINATION bin)
