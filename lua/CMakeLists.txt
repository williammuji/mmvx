file(GLOB LUA_C_SRCS "*.c")
set(lua_SRCS
  ${LUA_C_SRCS}
  lua_tinker.cpp
  )

add_library(lua ${lua_SRCS})

install(TARGETS lua DESTINATION lib)

file(GLOB HEADERS "*.h")
install(FILES ${HEADERS} DESTINATION include/lua)
