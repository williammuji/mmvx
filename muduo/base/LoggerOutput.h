#ifndef LOGGER_OUTPUT_H
#define LOGGER_OUTPUT_H

#include <muduo/base/LogFile.h>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
extern boost::scoped_ptr<muduo::LogFile> outputFile;
void setLoggerOutputFile(const string& basename,
                         size_t rollSize,
                         bool threadSafe = true,
                         int flushInterval = 3,
                         int checkEveryN = 1024);
}

#endif
