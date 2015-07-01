add_custom_command(OUTPUT licenseplate.pb.cc licenseplate.pb.h
	COMMAND protoc
	ARGS --cpp_out . ${CMAKE_CURRENT_SOURCE_DIR}/licenseplate.proto -I${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS licenseplate.proto
	VERBATIM )

set_source_files_properties(licenseplate.pb.cc PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-shadow")
include_directories(${PROJECT_BINARY_DIR})


add_executable(multi_licenseplate_client client.cc licenseplate.pb.cc)
target_link_libraries(multi_licenseplate_client muduo_protobuf_codec mmvx_proto)

add_executable(multi_licenseplate_server server.cc licenseplate.pb.cc)
target_link_libraries(multi_licenseplate_server muduo_protobuf_codec mmvx_proto)

add_executable(multi_licenseplate_gate gate.cc licenseplate.pb.cc)
target_link_libraries(multi_licenseplate_gate muduo_protobuf_codec mmvx_proto)

install(TARGETS multi_licenseplate_client DESTINATION bin)
install(TARGETS multi_licenseplate_server DESTINATION bin)
install(TARGETS multi_licenseplate_gate DESTINATION bin)
