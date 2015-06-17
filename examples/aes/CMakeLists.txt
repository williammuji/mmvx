include_directories(${PROJECT_BINARY_DIR} 
	${OpenSSL_INCLUDE_DIR})

add_executable(igetest igetest.cc)
target_link_libraries(igetest muduo_base ${OpenSSL_LIBRARIES})
