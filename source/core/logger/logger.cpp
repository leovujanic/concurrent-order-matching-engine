#include <utility>
#include <type_traits>
#include <iostream>

#include "logger.h"
#include "memory_mapped_file_backend.hpp"

using namespace std;

namespace core
{

void Logger::initialise(const LoggerArguments& configuration)
{
    // Disc write period
    m_writePeriodInMicroseconds = configuration.m_writePeriodInMilliSeconds * 1000;

    // Ring buffer size
    m_buffer.reset(new core::RingBufferMPMC<LogEntry>(configuration.m_bufferSize));

    // Log level
    m_logLevel = static_cast<std::underlying_type<LogLevel>::type >(configuration.m_logLevel);

    // Memory mapped file sink
    if (configuration.m_memoryMappedFileName.length() > 0)
    {
        m_backendEnabled = true;
        m_backend.setResourceName(configuration.m_memoryMappedFileName);
        m_backend.setRotationSize(configuration.m_rotationSizeInBytes);
    }

    m_copyToStdout = configuration.m_copyToStdout;
}

void Logger::log(const LogLevel level, const string& sender, const string& message)
{
    auto logLevel = static_cast<std::underlying_type<LogLevel>::type >(level);

    if (m_logLevel >= logLevel)
    {
        LogEntry entry(level, sender, message);
        pushLogToLogBuffer(entry);
    }
}

void Logger::pushLogToLogBuffer(const LogEntry& log)
{
    m_buffer->push(log);
}

void* Logger::run()
{
    if ( m_backendEnabled == false && m_copyToStdout == false)
    {
        return nullptr; // Early exit
    }

    // Open backend
    if (m_backendEnabled)
    {
        m_backend.open();
    }

    while ( true )
    {
        while (m_buffer->count() > 0)
        {
            auto log = m_buffer->pop();

            if (m_backendEnabled)
            {
                m_backend.process(log);
            }

            if (m_copyToStdout)
            {
                std::cout << log.getMessage() << std::endl;
            }
        }

        if (isFinishing() == true)
        {
            break;
        }

        applyWaitStrategy(m_writePeriodInMicroseconds);
    }

    // Close backend
    if (m_backendEnabled)
    {
        m_backend.close();
    }

    return nullptr;
}

}//namespace