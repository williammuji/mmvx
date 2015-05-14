#ifndef MUDUO_BASE_LUA_H
#define MUDUO_BASE_LUA_H

extern "C"
{
#include "lua/lua.h"
#include "lua/lualib.h"
#include "lua/lauxlib.h"  
};

#include <boost/noncopyable.hpp>
#include <lua/lua_tinker.h>
#include <muduo/base/Types.h>

namespace muduo
{
class Lua : boost::noncopyable
  {
   public:
    Lua();
    ~Lua();

    bool loadFile(const string& name);
    bool loadDir(const string& name);


    template<typename RVal>
    RVal call(const char* name)
    {
      return lua_tinker::call<RVal>(L_, name);
    }

    template<typename RVal, typename T1>
    RVal call(const char* name, T1 arg)
    {
      return lua_tinker::call<RVal>(L_, name, arg);
    }

    template<typename RVal, typename T1, typename T2>
    RVal call(const char* name, T1 arg1, T2 arg2)
    {
      return lua_tinker::call<RVal>(L_, name, arg1, arg2);
    }

    template<typename RVal, typename T1, typename T2, typename T3>
    RVal call(const char* name, T1 arg1, T2 arg2, T3 arg3)
    {
      return lua_tinker::call<RVal>(L_, name, arg1, arg2, arg3);
    }


    template<typename F>
    void def(const char* name, F func)
    {
      lua_tinker::def(L_, name, func);
    }

    template<typename T>
    void set(const char* name, T object)
    {
      lua_tinker::set(L_, name, object);
    }

    template<typename T>
    T get(const char* name)
    {
      return lua_tinker::get<T>(L_, name);
    }


    template<typename T>
    void class_add(const char* name)
    {
      lua_tinker::class_add<T>(L_, name);
    }

    template<typename T, typename P>
    void class_inh()
    {
      lua_tinker::class_inh<T, P>(L_);
    }

    template<typename T, typename F>
    void class_def(const char* name, F func)
    {
      lua_tinker::class_def<T, F>(L_, name, func);
    }

    template<typename T, typename BASE, typename VAR>
    void class_mem(const char* name, VAR BASE::*val)
    {
      lua_tinker::class_mem<T, BASE, VAR>(L_, name, *val);
    }

  private:
    lua_State* L_;
};
}

#endif
