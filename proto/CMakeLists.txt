file(GLOB PROTOS "*.proto")

foreach(nextProto ${PROTOS})
	get_filename_component(baseName ${nextProto} NAME_WE)

	add_custom_command(OUTPUT ${baseName}.pb.cc ${baseName}.pb.h
		COMMAND protoc
		ARGS --cpp_out . ${CMAKE_CURRENT_SOURCE_DIR}/${baseName}.proto -I${CMAKE_CURRENT_SOURCE_DIR}
		DEPENDS ${baseName}.proto
		VERBATIM)

	set(PROTO_SRCS ${PROTO_SRCS} ${baseName}.pb.cc)
	set(PROTO_HEADERS ${PROTO_HEADERS} ${PROJECT_BINARY_DIR}/proto/${baseName}.pb.h)

	set_source_files_properties(${baseName}.pb.cc PROPERTIES COMPILE_FLAGS "-Wno-conversion -Wno-shadow")
	include_directories(${PROJECT_BINARY_DIR})
endforeach(nextProto ${PROTOS})


add_library(mmvx_proto ${PROTO_SRCS})
target_link_libraries(mmvx_proto protobuf)


install(TARGETS mmvx_proto DESTINATION lib)
set(HEADERS
	${PROTOS}
	${PROTO_HEADERS}
	)
install(FILES ${HEADERS} DESTINATION include/proto)

