include_directories(${PROJECT_BINARY_DIR} 
	${MYSQL_INCLUDE_DIR}
	${MYSQLPP_INCLUDE_DIR})

add_executable(logonserver LogonServer.cc)
set_target_properties(logonserver PROPERTIES COMPILE_FLAGS "-Wno-shadow")
target_link_libraries(logonserver muduo_protobuf_codec mmvx_proto ${MYSQL_LIBRARY} ${MYSQLPP_LIBRARY})

install(TARGETS logonserver DESTINATION bin)
