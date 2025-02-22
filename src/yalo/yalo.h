#pragma once

#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <mutex>
#include <cstdint>
#include <sstream>
#include <stdio.h>
#include <exception>
#include <cerrno>
#include <type_traits>

#define lLog yalo::Logger(yalo::Log, __FILE__, __LINE__, __func__)
#define lErr yalo::Logger(yalo::Error, __FILE__, __LINE__, __func__)
#define lWarn yalo::Logger(yalo::Warning, __FILE__, __LINE__, __func__)
#define lInfo yalo::Logger(yalo::Info, __FILE__, __LINE__, __func__)
#define lDebug yalo::Logger(yalo::Debug, __FILE__, __LINE__, __func__)
#define lVerbose yalo::Logger(yalo::Verbose, __FILE__, __LINE__, __func__)
#define lTrace yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__)

namespace yalo {

enum Level {
    Log = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4,
    Verbose = 5,
    Trace = 6,
};

class ISink {
public:
    virtual void log(const std::string& line)=0;
    virtual ~ISink()=default;
};

class Logger;

class IFormatter {
public:
    virtual ~IFormatter()=default;
    virtual std::string format(const std::string& line, size_t thread, const Logger& logger)=0;
    virtual std::string format(const std::exception& exception)=0;
};

class Logger {
public:
    typedef std::unique_ptr<ISink> ISinkPtr;
    typedef std::unique_ptr<IFormatter> IFormatterPtr;
    enum InserterSpacing {InserterPad, InserterAsIs};

    static void addSink(ISinkPtr method);
    static void clearSinks();
    static void setFormat(IFormatterPtr formatter);
    static void setLevel(Level level);
    static bool shown(Level level);
    static void setInserterSpacing(InserterSpacing spacing);

    Logger(Level level, const char* file=nullptr, const int line=0, const char* function=nullptr);
    ~Logger();

    Logger& log_line(const std::string& line);
    template<typename T>
    T log_expression(const std::string& flow, const std::string& expression, T result);
    bool log_expression_bool(const std::string& flow, const std::string& expression, bool result);

    template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    Logger& operator<<(T value);
    
    Logger& operator<<(const std::string& str);
    Logger& operator<<(const char* str);
    Logger& operator<<(const void* ptr);
    Logger& operator<<(float value);
    Logger& operator<<(double value);
    Logger& operator<<(const std::exception& exception);

    typedef std::vector<std::thread::id> ThreadList;
    typedef std::lock_guard<std::mutex> Lock;
    typedef std::vector<ISinkPtr> SinkList;

    const Level levelRequested;
    const char* file;
    const int line;
    const char* function;

private:
    std::string _stream;
    enum Mutex {ThreadListMutex, SinkListMutex, FormatterMutex};
    enum Action {Change, NoChange};
    static std::mutex& _mutex(Mutex mutexType);
    static size_t _thread_index();
    static Level _level(Level level, Action action=Change);
    static SinkList& _sinks_needs_lock(); // must Lock(_muetx(SinkListMutex))
    static IFormatterPtr& _formatter(IFormatterPtr update);
    static IFormatterPtr& _formatter();
    static InserterSpacing _spacing(InserterSpacing spacing, Action action=Change);
    Logger& _append(const std::string& value);
};

class DefaultFormatter : public IFormatter {
public:
    enum Location { GMT, Local };
    static std::string date(Location location);
    static std::string levelString(Level level);

    DefaultFormatter(Location location=Local);
    ~DefaultFormatter()=default;
    virtual std::string format(const std::string& line, size_t thread, const Logger& logger) override;
    virtual std::string format(const std::exception& exception) override;

    typedef std::runtime_error RuntimeError;
private:
    Location _location;
    static char* _writable(std::string& str);
    static std::string _ms_str(const std::chrono::system_clock::time_point& time);
    static std::string& _replace(std::string& str, const std::string& search, const std::string& replace);
};

class StreamSink : public ISink {
public:
    enum CloseAction {AutoClose, DoNotClose};
    StreamSink(FILE* stream, const std::string& name, CloseAction action=AutoClose);
    virtual ~StreamSink();

    virtual void log(const std::string& line) override;

protected:
    std::string _name;

private:
    FILE* _stream;
    bool _close;
};

class StdErrSink : public StreamSink {
public:
    StdErrSink();
    virtual ~StdErrSink()=default;
};

class StdOutSink : public StreamSink {
public:
    StdOutSink();
    virtual ~StdOutSink()=default;
};

class FileSink : public StreamSink {
public:
    FileSink(const std::string& path);
    virtual ~FileSink()=default;

private:
    static FILE* _open(const std::string& path);
};
    
inline void Logger::addSink(ISinkPtr method) {
    if (method) {
        Lock protection(_mutex(SinkListMutex));

        _sinks_needs_lock().push_back(std::move(method));
    }
}

inline void Logger::clearSinks() {
    Lock protection(_mutex(SinkListMutex));

    _sinks_needs_lock().clear();
}

inline void Logger::setFormat(IFormatterPtr formatter) {
    _formatter(std::move(formatter));
}

inline void Logger::setLevel(Level level) {
    _level(level);
}

inline bool Logger::shown(Level level) {
    const auto current = _level(level, NoChange);

    return level <= current;
}

inline void Logger::setInserterSpacing(InserterSpacing spacing) {
    _spacing(spacing);
}

inline Logger::Logger(Level level, const char* file, const int line, const char* function)
    :levelRequested(level), file(file), line(line), function(function), _stream() {}

inline Logger::~Logger() {
    if (_stream.empty()) {
        return;
    }

    try {
        log_line(_stream);
    } catch(...) {
        // too late
    }
}

inline Logger& Logger::log_line(const std::string& logLine) {
    if (!shown(levelRequested)) {
        return *this;
    }

    typedef std::pair<std::string, std::string> ExceptionLogger;
    typedef std::vector<ExceptionLogger> ExceptionList;
    ExceptionList failed_sinks;
    Lock lock(_mutex(SinkListMutex));
    SinkList& sinks = _sinks_needs_lock();
    const auto formatted_line = _formatter()->format(logLine, _thread_index(), *this);

    if (sinks.empty()) {
        sinks.push_back(std::unique_ptr<StdErrSink>(new StdErrSink()));
    }

    for (auto sink = sinks.begin(); sink != sinks.end(); ) {
        try {
                (*sink)->log(formatted_line);
                ++sink;
        } catch (const std::exception& exception) {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wpotentially-evaluated-expression"
            const auto loggerType = *sink ? typeid(**sink).name() : "";
            #pragma GCC diagnostic pop

            failed_sinks.push_back(ExceptionLogger(_formatter()->format(exception), loggerType));
            sink = sinks.erase(sink);
        }
    }

    if (sinks.empty()) {
        sinks.push_back(std::unique_ptr<StdErrSink>(new StdErrSink()));
    }

    if (!failed_sinks.empty()) {
        for (const auto& exceptionLogger : failed_sinks) {
            for (auto& sink : sinks) {
                try {
                    sink->log(_formatter()->format("Logger[" + exceptionLogger.second + "]: " 
                                                    + exceptionLogger.first, 
                                                    _thread_index(), *this));
                } catch(const std::exception&) {
                    // we tried
                }
            }
        }
    }

    return *this;
}

template<typename T>
inline T Logger::log_expression(const std::string& flow, const std::string& expression, T result) {
    log_line(flow + ": " + expression + " => " + std::to_string(result));
    return result;
}

inline bool Logger::log_expression_bool(const std::string& flow, const std::string& expression, bool result) {
    log_line(flow + ": " + expression + " => " + (result ? "true" : "false"));
    return result;
}

template<typename T, typename>
inline Logger& Logger::operator<<(T value) {
    return _append(std::to_string(value));
}

inline Logger& Logger::operator<<(const std::string& str) {
    return _append(str);
}

inline Logger& Logger::operator<<(const char* str) {
    return _append(str);
}

inline Logger& Logger::operator<<(const void* ptr) {
    std::ostringstream oss;

    oss << std::showbase << std::uppercase << std::hex << ptr;
    return _append(oss.str());
}

inline Logger& Logger::operator<<(float value) {
    return (*this) << static_cast<double>(value);
}

inline Logger& Logger::operator<<(double value) {
    std::ostringstream oss;

    oss << value;
    return _append(oss.str());
}

inline Logger& Logger::operator<<(const std::exception& exception) {
    return _append(_formatter()->format(exception));
}

inline std::mutex& Logger::_mutex(Mutex mutexType) {
    static std::mutex sinkList;
    static std::mutex threadList;
    static std::mutex formatterMutex;

    switch(mutexType) {
        case ThreadListMutex:
            return threadList;
        case SinkListMutex:
            return sinkList;
        case FormatterMutex:
            return formatterMutex;
        default:
            throw std::invalid_argument("mutexType is invalid: " + std::to_string(mutexType));
    }
}

inline size_t Logger::_thread_index() {
    static ThreadList threads;
    const auto this_thread = std::this_thread::get_id();
    Lock protection(_mutex(ThreadListMutex));
    const auto found = std::find(threads.begin(), threads.end(), this_thread);

    if (found == threads.end()) {
        threads.push_back(this_thread);
        return threads.size() - 1;
    }

    return static_cast<size_t>(found - threads.begin());
}

inline Level Logger::_level(Level next, Action action) {
    static Level level=Error;

    if (Change == action) {
        level = next;
    }

    return level;
}

inline Logger::SinkList& Logger::_sinks_needs_lock() {
    static SinkList sinks;

    return sinks;
}

inline Logger& Logger::_append(const std::string& value) {
    if (InserterPad == _spacing(InserterPad, NoChange) && !_stream.empty()) {
        _stream += ' ';
    }

    _stream += value;
    return *this;
}

inline Logger::IFormatterPtr& Logger::_formatter(IFormatterPtr update) {
    static IFormatterPtr formatter;
    Lock protection(_mutex(FormatterMutex));

    if (update) {
        formatter = std::move(update);
    } else if (!formatter) {
        formatter = std::unique_ptr<DefaultFormatter>(new DefaultFormatter());
    }

    return formatter;
}

inline Logger::IFormatterPtr& Logger::_formatter() {
    return _formatter(nullptr);
}

inline Logger::InserterSpacing Logger::_spacing(InserterSpacing next, Action action) {
    static InserterSpacing spacing=InserterPad;

    if (Change == action) {
        spacing = next;
    }

    return spacing;
}

inline StreamSink::StreamSink(FILE* stream, const std::string& name, CloseAction action) 
    :_name(name), _stream(stream), _close(action == AutoClose) {}

inline StreamSink::~StreamSink() {
    if (_close && nullptr != _stream) {
        if (::fclose(_stream) != 0) {
            // too late
        }
    }
}

inline void StreamSink::log(const std::string&line) {
    errno = 0;
    const auto amount = ::fwrite(reinterpret_cast<const char *>(line.c_str()), 1, line.size(), _stream);
    const auto error = errno;

    if (::ferror(_stream)) {
        throw std::system_error(
            std::error_code(error, std::generic_category()),
            "Failed to log to '" + _name + "': " + ::strerror(error)
        );
    }

    if (amount < line.size()) {
        throw std::runtime_error("Incomplete write to " + _name);
    }
}

inline StdErrSink::StdErrSink()
    :StreamSink(stderr, "stderr", StreamSink::DoNotClose) {}

inline StdOutSink::StdOutSink()
    :StreamSink(stdout, "stdout", StreamSink::DoNotClose) {}

inline FileSink::FileSink(const std::string& path)
    :StreamSink(_open(path), path, StreamSink::AutoClose) {}

inline FILE* FileSink::_open(const std::string& path) {
    auto opened = ::fopen(path.c_str(), "a");

    if (nullptr == opened) {
        const auto error = errno;

        throw std::system_error(
            std::error_code(error, std::generic_category()),
            "Failed to open log '" + path + "': " + ::strerror(error)
        );
    }

    return opened;
}

inline DefaultFormatter::DefaultFormatter(Location location)
    :_location(location) {}

inline std::string DefaultFormatter::format(const std::string& line, size_t thread, const Logger& logger) {
    return "[" + date(_location)
        + "][" + std::to_string(thread)
        + "][" + levelString(logger.levelRequested)
        + "][" + logger.file + ":" + std::to_string(logger.line)
        + "][" + logger.function
        + "] " + line + "\n";
}

inline std::string DefaultFormatter::format(const std::exception& exception) {
    return std::string("Exception: ") + exception.what();
}

inline std::string DefaultFormatter::date(Location location) {
    const auto nowHiRes = std::chrono::system_clock::now();
    const auto nowSeconds = std::chrono::system_clock::to_time_t(nowHiRes);
    const auto ms = _ms_str(nowHiRes);
    struct tm now;
    std::string buffer;

    ::memset(&now, 0, sizeof(now));

    const auto result = GMT == location 
                        ? ::gmtime_r(&nowSeconds, &now) 
                        : ::localtime_r(&nowSeconds, &now);

    if (nullptr == result) {
        throw RuntimeError("Unable to get time");
    }

    buffer.assign(35, '\0');
    const auto size = ::strftime(_writable(buffer), buffer.length(), 
                                GMT == location
                                    ? "%Y-%m-%d %H:%M:%S.MS (%a)"
                                    : "%Y-%m-%d %H:%M:%S.MS %z (%a)",
                                &now);
                                
    if (0 == size) {
        throw RuntimeError("Unable to format time");
    }

    buffer.erase(size);
    return _replace(buffer, ".MS", "." + ms);
}

inline std::string DefaultFormatter::levelString(Level level) {
    switch(level) {
        case Log:
            return "LOG";
        case Error:
            return "ERR";
        case Warning:
            return "WRN";
        case Info:
            return "NFO";
        case Debug:
            return "DBG";
        case Verbose:
            return "VBS";
        case Trace:
            return "TRC";
        default:
            return "???";
    }
}

inline char* DefaultFormatter::_writable(std::string& str) {
    return str.empty() ? nullptr : &str[0];
}

inline std::string DefaultFormatter::_ms_str(const std::chrono::system_clock::time_point& time) {
    const auto now = time.time_since_epoch();
    const auto now_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now);
    auto ms_str = std::to_string((now_in_ms % 1000).count());

    while (ms_str.size() < 3) {
        ms_str = '0' + ms_str;
    }
    
    return ms_str;
}

inline std::string& DefaultFormatter::_replace(std::string& str, const std::string& search, const std::string& replace) {
    size_t pos = 0;

    while ((pos = str.find(search, pos)) != std::string::npos) {
        str.replace(pos, search.length(), replace);
        pos += replace.length();
    }

    return str;
}

}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wkeyword-macro"
#define if(expression) if(yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__).log_expression_bool("if", #expression, expression))
#define while(expression) while(yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__).log_expression_bool("while", #expression, expression))
#define switch(expression) switch(yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__).log_expression("switch", #expression, expression))
#pragma GCC diagnostic pop
