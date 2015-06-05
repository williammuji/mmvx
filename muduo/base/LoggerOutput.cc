#include <muduo/base/LoggerOutput.h>
#include <muduo/base/Logging.h>

namespace muduo
{
boost::scoped_ptr<muduo::LogFile> outputFile;

namespace detail
{
void loggerOutput(const char* msg, int len)
{
  assert(outputFile != NULL);
  outputFile->append(msg, len);
}
void loggerFlush()
{
  assert(outputFile != NULL);
  outputFile->flush();
}
}

void setLoggerOutputFile(const string& basename,
                         size_t rollSize,
                         bool threadSafe,
                         int flushInterval,
                         int checkEveryN)
{
  outputFile.reset(new muduo::LogFile(basename, rollSize, threadSafe, flushInterval, checkEveryN));
  muduo::Logger::setOutput(detail::loggerOutput);
  muduo::Logger::setFlush(detail::loggerFlush);
}
}
