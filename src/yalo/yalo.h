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
#include <string.h>
#include <map>

#define lFatal yalo::Logger(yalo::Fatal, __FILE__, __LINE__, __func__)
#define lFatalIf(condition) yalo::Logger(yalo::Fatal, __FILE__, __LINE__, __func__, condition, #condition)
#define lLog yalo::Logger(yalo::Log, __FILE__, __LINE__, __func__)
#define lErr yalo::Logger(yalo::Error, __FILE__, __LINE__, __func__)
#define lErrIf(condition) yalo::Logger(yalo::Error, __FILE__, __LINE__, __func__, condition, #condition)
#define lWarn yalo::Logger(yalo::Warning, __FILE__, __LINE__, __func__)
#define lWarnIf(condition) yalo::Logger(yalo::Warning, __FILE__, __LINE__, __func__, condition, #condition)
#define lInfo yalo::Logger(yalo::Info, __FILE__, __LINE__, __func__)
#define lDebug yalo::Logger(yalo::Debug, __FILE__, __LINE__, __func__)
#define lVerbose yalo::Logger(yalo::Verbose, __FILE__, __LINE__, __func__)
#define lTrace yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__)

namespace yalo {

enum Level {
    Fatal = 0,
    Log = 1,
    Error = 2,
    Warning = 3,
    Info = 4,
    Debug = 5,
    Verbose = 6,
    Trace = 7,
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
    static void setSettingsFile(const std::string& path, int checkIntervalSeconds=10);
    static void setLevel(Level level, const std::string& pattern="");
    static void resetLevels(Level level);
    static bool shown(Level level, const std::string& file="");
    static void setInserterSpacing(InserterSpacing spacing);

    Logger(Level level, const char* file=nullptr, const int line=0, const char* function=nullptr, bool doLog=true, const char* condition=nullptr);
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    ~Logger();

    Logger& log_line(const std::string& line);
    template<typename T>
    T logExpression(const std::string& flow, const std::string& expression, T result);
    bool logExpressionBool(const std::string& flow, const std::string& expression, bool result);

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
    typedef std::map<Level, std::string> FileLevels;
    typedef std::chrono::system_clock::time_point Timestamp;

    const Level levelRequested;
    const char* file;
    const int line;
    const char* function;
    const char* condition;

private:
    std::string _stream;
    const bool _doLog;
    enum Mutex {ThreadListMutex, SinkListMutex, FormatterMutex, LevelsMutex, SettingsMutex};
    enum Action {Change, NoChange};
    static std::mutex& _mutex(Mutex mutexType);
    static size_t _threadIndex();
    static FileLevels& _levelsNeedsLock(); // must Lock(_mutex(LevelsMutex))
    static SinkList& _sinksNeedsLock(); // must Lock(_muetx(SinkListMutex))
    static IFormatterPtr& _formatter(IFormatterPtr update);
    static IFormatterPtr& _formatter();
    static InserterSpacing _spacing(InserterSpacing spacing, Action action=Change);
    static void _settingsFile(const std::string& path="", int checkIntervalSeconds=0);
    static std::string _settingsContents(const std::string& path, int checkIntervalSeconds);
    static bool _fileMatches(const std::string& file, const std::string& pattern);
    static std::string _readFile(const std::string& path);
    static Level _fromString(const std::string &level);
    static std::string _trim(const std::string &str);
    Logger& _append(const std::string& value);
    Logger& _logLineCore(const std::string& line);
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
    StreamSink(const StreamSink&) = delete;
    StreamSink& operator=(const StreamSink&) = delete;
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

        _sinksNeedsLock().push_back(std::move(method));
    }
}

inline void Logger::clearSinks() {
    Lock protection(_mutex(SinkListMutex));

    _sinksNeedsLock().clear();
}

inline void Logger::setFormat(IFormatterPtr formatter) {
    _formatter(std::move(formatter));
}

inline void Logger::setSettingsFile(const std::string& path, int checkIntervalSeconds) {
    _settingsFile(path, checkIntervalSeconds);
}

inline void Logger::setLevel(Level level, const std::string& pattern) {
    /*
        If you set Error, "" then all files will be shown for Error or Log
        Any levels higher than Error and have "" set, will be cleared.
        To clear, the higher levels *must* match the pattern exactly
    */
    Lock lock(_mutex(LevelsMutex));
    auto& levels = _levelsNeedsLock();
    const auto start = static_cast<int>(level);
    const auto max = static_cast<int>(Trace);

    levels[level] = pattern;

    for (int index = start; index <= max; ++index) {
        const auto lvl = static_cast<Level>(index);

        if (pattern == levels[lvl]) {
            levels.erase(lvl);
        }
    }
}

inline void Logger::resetLevels(Level level) {
    Lock lock(_mutex(LevelsMutex));
    auto& levels = _levelsNeedsLock();

    levels.clear();
    levels[level] = "";
}

inline bool Logger::shown(Level loggedLevel, const std::string& file) {
    /*
        If the file has been set to the requested loggedLevel or higher,
        it can be logged.
    */
    Lock lock(_mutex(LevelsMutex));
    auto& levels = _levelsNeedsLock();
    const auto start = static_cast<int>(loggedLevel);
    const auto max = static_cast<int>(Trace);

    for (int index = start; index <= max; ++index) {
        const auto level = static_cast<Level>(index);
        const auto found = levels.find(level);

        if (found == levels.end()) {
            continue; // nothing authorized for this level
        }

        if (_fileMatches(file, found->second)) {
            return true;
        }
    }
    return false;
}

inline void Logger::setInserterSpacing(InserterSpacing spacing) {
    _spacing(spacing);
}

inline Logger::Logger(Level level, const char* fl, const int ln, const char* func, bool doLog, const char* cond)
    :levelRequested(level), file(fl), line(ln), function(func), condition(cond), _stream(), _doLog(doLog) {}

inline Logger::~Logger() {
    if (levelRequested != Fatal && (!_doLog || _stream.empty())) {
        return;
    }

    try {
        log_line(_stream);
    } catch(...) {
        // too late
    }

    if (_doLog && levelRequested == Fatal) {
        ::abort();
    }
}

inline Logger& Logger::log_line(const std::string& logLine) {
    _settingsFile(); // check for dynamic changes

    if (!shown(levelRequested, file)) {
        return *this;
    }

    return _logLineCore(logLine);
}

template<typename T>
inline T Logger::logExpression(const std::string& flow, const std::string& expression, T result) {
    log_line(flow + ": " + expression + " => " + std::to_string(result));
    return result;
}

inline bool Logger::logExpressionBool(const std::string& flow, const std::string& expression, bool result) {
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
    static std::mutex levelsMutex;
    static std::mutex settingsMutex;

    switch(mutexType) {
        case ThreadListMutex:
            return threadList;
        case SinkListMutex:
            return sinkList;
        case FormatterMutex:
            return formatterMutex;
        case LevelsMutex:
            return levelsMutex;
        case SettingsMutex:
            return settingsMutex;
        default:
            throw std::invalid_argument("mutexType is invalid: " 
                + std::to_string(static_cast<int>(mutexType)));
    }
}

inline size_t Logger::_threadIndex() {
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

inline Logger::FileLevels& Logger::_levelsNeedsLock() {
    static FileLevels levels;

    if (levels.size() == 0) {
        levels[Error] = "";
    }

    return levels;
}

inline Logger::SinkList& Logger::_sinksNeedsLock() {
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

inline Logger& Logger::_logLineCore(const std::string& logLine) {
    typedef std::pair<std::string, std::string> ExceptionLogger;
    typedef std::vector<ExceptionLogger> ExceptionList;
    ExceptionList failed_sinks;
    Lock lock(_mutex(SinkListMutex));
    auto& sinks = _sinksNeedsLock();
    const auto formatted_line = _formatter()->format(logLine, _threadIndex(), *this);

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
                                                    _threadIndex(), *this));
                } catch(const std::exception&) {
                    // we tried
                }
            }
        }
    }

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

inline void Logger::_settingsFile(const std::string& newPath, int checkIntervalSeconds) {
    std::string contents = _settingsContents(newPath, checkIntervalSeconds);
    size_t start = 0;

    if (!newPath.empty()) {
        Logger(Log)._logLineCore("New Settings File: " + newPath);
    }

    if (contents.empty()) {
        return; // no need to process
    }

    while (start < contents.size()) {
        const auto eol = contents.find('\n', start);
        const auto line = _trim(contents.substr(start, eol - start));
        const auto eoc = line.find(':');
        const auto command = eoc == std::string::npos ? line : _trim(line.substr(0, eoc));
        const auto data = eoc < line.size() ? _trim(line.substr(eoc + 1)) : std::string();
        
        if (line.empty()) {
            // ignore empty lines
        } else if (command == "clearSinks") {
            Logger(Log)._logLineCore("Clearing Sinks");
            clearSinks();
        } else if (command == "setFormatDefault") {
            setFormat(std::unique_ptr<DefaultFormatter>(new DefaultFormatter()));
            Logger(Log)._logLineCore("Resetting format to default");
        } else if (command == "setFormatDefaultGMT") {
            setFormat(std::unique_ptr<DefaultFormatter>(new DefaultFormatter(DefaultFormatter::GMT)));
            Logger(Log)._logLineCore("Resetting format to default GMT");
        } else if (command == "addSinkStdErr") {
            addSink(std::unique_ptr<StdErrSink>(new StdErrSink()));
            Logger(Log)._logLineCore("Adding stderr sink");
        } else if (command == "addSinkStdOut") {
            addSink(std::unique_ptr<StdOutSink>(new StdOutSink()));
            Logger(Log)._logLineCore("Adding stdout sink");
        } else if (command == "addSink") {
            if (!data.empty()) {
                try {
                    addSink(std::unique_ptr<FileSink>(new FileSink(data)));
                    Logger(Log)._logLineCore("Adding sink to " + data);
                } catch(const std::exception& exception) {
                    Logger(Log)._logLineCore("Error adding sink to " + data 
                                                + ": " + exception.what());
                }
            } else {
                Logger(Log)._logLineCore("Failed to add sink: " + line);
            }
        } else if (command == "resetLevels") {
            resetLevels(_fromString(data));
            Logger(Log)._logLineCore("resetLevels to " + std::to_string(_fromString(data)));
        } else if (command == "pad") {
            setInserterSpacing(InserterPad);
            Logger(Log)._logLineCore("Turned padding on");
        } else if (command == "noPad") {
            setInserterSpacing(InserterAsIs);
            Logger(Log)._logLineCore("Turned padding off");
        } else if (command == "setLevel") {
            const auto equals = data.find('=');
            const auto level = _fromString(data.substr(0, equals));
            const auto pattern = equals < data.size() 
                                    ? _trim(data.substr(equals+1)) 
                                    : std::string();

            setLevel(level, pattern);
            Logger(Log)._logLineCore("Set level #" + std::to_string(level) 
                                  + " pattern = '" + pattern + "'");
        } else {
            Logger(Log)._logLineCore("Unknown command '" + command + "': " + line);
        }

        start = eol < contents.size() ? eol + 1 : eol;
    }
}

inline std::string Logger::_settingsContents(const std::string& newPath, int checkIntervalSeconds) {
    static std::string path;
    static std::string lastContents;
    static Timestamp lastCheck;
    static int interval = 120;

    Lock protection(_mutex(SettingsMutex));
    const auto pathChanged = !newPath.empty() && newPath != path;
    
    if (!newPath.empty()) {
        path = newPath;
        interval = checkIntervalSeconds;
        lastCheck = pathChanged ? Timestamp() : lastCheck;
    }

    if (path.empty()) {
        return ""; // no path to check
    }

    const auto now = std::chrono::system_clock::now();
    const auto duration = now - lastCheck;
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    const auto timeToCheck = seconds >= interval;

    if (!timeToCheck) {
        return "";
    }

    lastCheck = now;

    const auto contents = _readFile(path);

    if (contents.empty() || contents == lastContents) {
        return ""; // doesn't exist, can't read it, or hasn't changed
    }

    lastContents = contents;
    return contents;
}

inline std::string Logger::_readFile(const std::string& path) {
    std::string buffer;

    const auto file = ::fopen(path.c_str(), "r");

    if (nullptr == file) {
        return "";
    }

    ::fseek(file, 0, SEEK_END);
    const size_t fileSize = static_cast<size_t>(::ftell(file));
    ::rewind(file);
    buffer.assign(fileSize, '\0');
    const auto bytesRead = ::fread(&buffer[0], 1, fileSize, file);
    ::fclose(file);
    return bytesRead == fileSize ? buffer : std::string();
}

inline Level Logger::_fromString(const std::string &level) {
    if (level.empty()) {
        return Error;
    }

    switch(level[0]) {
        case 'l':
        case 'L':
            return Log;
        case 'E':
        case 'e':
            return Error;
        case 'w':
        case 'W':
            return Warning;
        case 'i':
        case 'I':
            return Info;
        case 'd':
        case 'D':
            return Debug;
        case 'v':
        case 'V':
            return Verbose;
        case 't':
        case 'T':
            return Trace;
        default:
            return Error;
        }
}

inline std::string Logger::_trim(const std::string &str) {
    auto result = str;

    while (!result.empty() && std::isspace(result[0])) {
        result.erase(0, 1);
    }

    while (!result.empty() && std::isspace(result[result.size() - 1])) {
        result.erase(result.size() - 1);
    }

    return result;
}

inline bool Logger::_fileMatches(const std::string& file, const std::string& pattern) {
    /*
        Rules:
        1. No files matches a pattern of "-"
        2. Any file matches an empty pattern (including an empty file)
        3. All positive patterns are ORed and all negative patterns are ANDed

        Example:
        - "-" match nothing
        - "" match everything
        - "-bin/" match everything except files that contain "bin/"
        - "src/;-src/include/" match everything that contains "src/" 
                                unless it contains "src/include/"
        - ".h;.cpp;-main.cpp;-test.cpp" match all files that have ".h" or ".cpp" in them
                                            but don't match any that contain "main.cpp"
                                                or "test.cpp"
    */
    size_t start = 0;
    auto good = true;

    while (start < pattern.size()) {
        const auto end = pattern.find(';', start);
        const auto part = pattern.substr(start, end-start);
        const auto negative = part.find('-') == 0;
        const auto name = part.substr(negative ? 1 : 0);
        const auto matches = file.find(name) != std::string::npos;
        
        if (matches) {
            good = !negative;
        }
        
        start = end < pattern.size() ? end + 1 : end;
    }

    return good;
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
        + (logger.file ? ("][" + std::string(logger.file) + ":" + std::to_string(logger.line)) : std::string())
        + (logger.function ? ("][" + std::string(logger.function)) : std::string())
        + (logger.condition ? ("][" + std::string(logger.condition)) : std::string())
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
        case Fatal:
            return "FTL";
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

#ifndef DISABLE_YALO_TRACE
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wkeyword-macro"
#define if(expression) if(yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__).logExpressionBool("if", #expression, expression))
#define while(expression) while(yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__).logExpressionBool("while", #expression, expression))
#define switch(expression) switch(yalo::Logger(yalo::Trace, __FILE__, __LINE__, __func__).logExpression("switch", #expression, expression))
#pragma GCC diagnostic pop
#endif