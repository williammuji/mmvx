file(GLOB LUA_C_SRCS "*.c")
set(lua_SRCS
  ${LUA_C_SRCS}
  lua_tinker.cpp
  )

add_library(lua ${lua_SRCS})
target_link_libraries(lua dl)
set_target_properties(lua PROPERTIES COMPILE_FLAGS "-DLUA_USE_LINUX")

install(TARGETS lua DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/lua)
