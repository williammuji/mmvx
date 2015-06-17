set(base_SRCS
  Aes.cc
  AsyncLogging.cc
  Condition.cc
  CountDownLatch.cc
  Date.cc
  Exception.cc
  FileUtil.cc
  LogFile.cc
  LoggerOutput.cc
  Logging.cc
  LogStream.cc
  ProcessInfo.cc
  Timestamp.cc
  TimeZone.cc
  Thread.cc
  ThreadPool.cc
  Lua.cc
  )

add_library(muduo_base ${base_SRCS})
target_link_libraries(muduo_base pthread rt lua ssl crypto)

add_library(muduo_base_cpp11 ${base_SRCS})
target_link_libraries(muduo_base_cpp11 pthread rt lua ssl)
set_target_properties(muduo_base_cpp11 PROPERTIES COMPILE_FLAGS "-std=c++0x")

install(TARGETS muduo_base DESTINATION lib)
install(TARGETS muduo_base_cpp11 DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/muduo/base)

if(NOT CMAKE_BUILD_NO_EXAMPLES)
  add_subdirectory(tests)
endif()
