#include <muduo/base/Lua.h>
#include <dirent.h>
#include <sys/stat.h>
#include <muduo/base/Logging.h>

using namespace muduo;

Lua::Lua():L_(lua_open())
{
  assert(L_);
  luaopen_base(L_);
  luaL_openlibs(L_);
  luaopen_table(L_);
  luaopen_string(L_);
  luaopen_math(L_);
}

Lua::~Lua()
{
  lua_close(L_);
}

bool Lua::loadFile(const string& name)
{
  lua_tinker::dofile(L_, name.c_str());
  LOG_INFO << "Lua::loadFile " << name;
  return true;
}

bool Lua::loadDir(const string& name)
{
  DIR *dir;
  if ((dir = opendir(name.c_str())) == NULL)
  {
    LOG_ERROR << "Lua::loadDir " << name << " failed";
    return false;
  }

  struct dirent *dirp;
  while ((dirp = readdir(dir)) != NULL)
  {
    if (!strcmp(dirp->d_name, ".")
        || !strcmp(dirp->d_name, ".."))
      continue;

    char pathName[128];
    snprintf(pathName, 128, "%s/%s", name.c_str(), dirp->d_name);

    struct stat buf;
    if (lstat(pathName, &buf) >= 0 && S_ISDIR(buf.st_mode))
    {
      loadDir(pathName);
    }
    else
    {
      char *type = strrchr(dirp->d_name, '.');
      if (type && !strcmp(type, ".lua"))
      {
        if (!loadFile(pathName))
          return false;
      }
    }
  }
  closedir(dir);
  LOG_INFO << "Lua::loadDir " << name;
  return true;
}
