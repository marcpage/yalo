#include "../yalo/yalo.h"

class DebugSink : public yalo::ISink {
public:
    std::string& logBuffer;

    DebugSink(std::string& buffer):logBuffer(buffer) {}
    virtual void log(const std::string& line) override {logBuffer += line;}
    virtual ~DebugSink()=default;
};

class NullSink : public yalo::ISink {
public:
    virtual void log(const std::string& /*line*/) override {}
    virtual ~NullSink()=default;
};

class ThrowingSink : public yalo::ISink {
public:
    virtual void log(const std::string& /*line*/) override {throw std::runtime_error("ThrowingSink exception");}
    virtual ~ThrowingSink()=default;
};

static bool testLevel(yalo::Level level) {
    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(level);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    lLog << "--Log--";
    lErr << "--Err--";
    lWarn << "--Warn--";
    lInfo << "--Info__";
    lDebug << "--Debug--";
    lVerbose << "--Verbose--";
    lTrace << "--Trace--";

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    
    const auto lines = std::count(log.begin(), log.end(), '\n');
    const auto success = lines == static_cast<int>(level);

    if (!success) {
        fprintf(stderr, "FAIL: testLevel(%d) => lines = %d\n", level, static_cast<int>(lines));
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

#define TEST_TYPE(type) testType<type>(#type)
template<typename T>
static bool testType(const std::string& typeName) {
    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    lLog << static_cast<T>(0);

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto success = log.size() > 3 
            && log.substr(log.size() - 3, 3) == " 0\n";
    
    if (!success) {
        fprintf(stderr, "FAIL: TEST_TYPE(%s)\n", typeName.c_str());
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testPointer() {
    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    lLog << static_cast<void*>(0);

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto success = log.size() > 2
            && log.substr(log.size() - 2, 2) == "0\n";
    
    if (!success) {
        fprintf(stderr, "FAIL: testPointer()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testTraceIf() {
    bool success = true;

    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Trace);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    if (!log.empty()) {
        success = false;
    }

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    success = success && log.find("!log.empty() => false") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testTraceIf()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testTraceWhile() {
    int increment = 0;
    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Trace);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    while (increment < 3) {
        increment += 1;
    }

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto success = log.find("increment < 3 => false") != std::string::npos
                      && log.find("increment < 3 => true") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testTraceWhile()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

#define TEST_SWITCH(type) testSwitch<type>(#type)
template<typename T>
static bool testSwitch(const std::string& typeName) {
    bool success = true;

    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Trace);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    for (int i = 0; i < 3; ++i) {
        switch (static_cast<T>(i)) {
            case static_cast<T>(0): break;
            case static_cast<T>(1): break;
            case static_cast<T>(2): break;
            default: success = false; break;
        }
    }

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    success = success && log.find("static_cast<T>(i) => 0") != std::string::npos
                      && log.find("static_cast<T>(i) => 1") != std::string::npos
                      && log.find("static_cast<T>(i) => 2") != std::string::npos;
    
    if (!success) {
        fprintf(stderr, "FAIL: TEST_SWITCH(%s)\n", typeName.c_str());
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testBadLogFile() {
    bool success = true;

    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    try {
        yalo::Logger::addSink(std::unique_ptr<yalo::FileSink>(new yalo::FileSink("bin/bogus/_/log.txt")));
    } catch(const std::system_error& exception) {
        success = success && std::string(exception.what()).find("bin/bogus/_/log.txt") != std::string::npos;
    }
        
    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    if (!success) {
        fprintf(stderr, "FAIL: testBadLogFile()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static std::string readFileContents(const std::string& path) {
    FILE* file = ::fopen(path.c_str(), "r");
    std::string contents;

    if (nullptr == file) {
        return "";
    }

    ::fseek(file, 0, SEEK_END);
    const size_t fileSize = static_cast<size_t>(::ftell(file));
    ::rewind(file);
    contents.assign(fileSize, '\0');
    const auto bytesRead = ::fread(&contents[0], 1, fileSize, file);
    ::fclose(file);
    return bytesRead == fileSize ? contents : std::string();
}

static bool testLogFile() {
    bool success = true;

    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    yalo::Logger::addSink(std::unique_ptr<yalo::FileSink>(new yalo::FileSink("bin/testLogFile.txt")));
    
    lLog << "test";

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto contents = readFileContents("bin/testLogFile.txt");

    success = !contents.empty() && log == contents;

    if (!success) {
        fprintf(stderr, "FAIL: testLogFile()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
        fprintf(stderr, "[%s]\n", contents.c_str());
    }

    return success;
}

static bool testException() {
    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    yalo::Logger::addSink(std::unique_ptr<yalo::FileSink>(new yalo::FileSink("bin/testLogFile.txt")));
    
    try {
        throw std::runtime_error("Runtime Error");
    } catch(const std::runtime_error& exception) {
        lLog << exception;
    }

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto success = log.find("Runtime Error") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testException()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testLoggerExcpetion() {
    yalo::Logger::clearSinks();
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    yalo::Logger::addSink(std::unique_ptr<ThrowingSink>(new ThrowingSink()));

    lLog << "test";

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto success = log.find("ThrowingSink exception") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testPointer()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testPadding() {
    yalo::Logger::clearSinks();
    yalo::Logger::setInserterSpacing(yalo::Logger::InserterPad);
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    lLog << std::string("test") << 5;

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto success = log.find("test 5") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testPadding()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testNoPadding() {
    yalo::Logger::clearSinks();
    yalo::Logger::setInserterSpacing(yalo::Logger::InserterAsIs);
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    lLog << std::string("test") << 5;

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto success = log.find("test5") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testPadding()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static void thread_logging(int identifier) {
    for (int i = 0; i < 100; ++i) {
        lLog << "thread #" << identifier << " iteration #" << i;
    }
}

static bool testThreading() {
    yalo::Logger::clearSinks();
    yalo::Logger::setInserterSpacing(yalo::Logger::InserterAsIs);
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    std::thread t1(thread_logging, 1);
    std::thread t2(thread_logging, 2);
    std::thread t3(thread_logging, 3);
    std::thread t4(thread_logging, 4);
    std::thread t5(thread_logging, 5);
    std::thread t6(thread_logging, 6);
    std::thread t7(thread_logging, 7);
    std::thread t8(thread_logging, 8);
    std::thread t9(thread_logging, 9);

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();
    t6.join();
    t7.join();
    t8.join();
    t9.join();

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    const auto lines = std::count(log.begin(), log.end(), '\n');
    const auto success = lines == 900;

    if (!success) {
        fprintf(stderr, "FAIL: testPadding()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testFormatGMT() {
    yalo::Logger::clearSinks();
    yalo::Logger::setInserterSpacing(yalo::Logger::InserterAsIs);
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter(yalo::DefaultFormatter::GMT)));
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    lLog << std::string("test") << 5;

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter()));

    const auto success = log.find("test5") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testPadding()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testFilePattern() {
    yalo::Logger::clearSinks();
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter(yalo::DefaultFormatter::GMT)));
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    yalo::Logger::setLevel(yalo::Verbose, "test_yalo.cpp");

    lDebug << "testing";

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter()));

    const auto success = log.find("testing") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testFilePatterns()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testFileNoPattern() {
    yalo::Logger::clearSinks();
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter(yalo::DefaultFormatter::GMT)));
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    yalo::Logger::setLevel(yalo::Verbose, "-test_yalo.cpp");
    yalo::Logger::setLevel(yalo::Info, "test_yalo.cpp");

    lDebug << "testing";

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter()));

    const auto success = log.find("testing") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testFileNoPattern()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool createFile(const std::string& path, const std::string& contents) {
    const auto file = ::fopen(path.c_str(), "w");

    if (nullptr == file) {
        return false;
    }

    const auto amount = ::fwrite(reinterpret_cast<const char *>(contents.c_str()), 1, contents.size(), file);
    ::fclose(file);
    return amount == contents.size();
}

static bool testCommandFile() {
    const auto commands = R"(
        setFormatDefaultGMT
        setFormatDefault
        addSink: bin/testCommandFile.log
        resetLevels: Log
        pad
        noPad
        setLevel:Error
        setLevel:Debug=test_yalo.cpp
    )";
    bool success = createFile("bin/testCommandFile.txt", commands);

    yalo::Logger::clearSinks();
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter(yalo::DefaultFormatter::GMT)));
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    yalo::Logger::setSettingsFile("bin/testCommandFile.txt");

    lDebug << "testing";

    yalo::Logger::setSettingsFile("bin/nonexistant/path/testCommandFile.txt");
    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter()));

    success = success && log.find("testing") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testCommandFile()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testCommandFileCreated() {
    const auto commands = R"(
        setFormatDefaultGMT
        setFormatDefault
        addSink: bin/testCommandFileCreated.log
        resetLevels: Log
        pad
        noPad
        setLevel:Error
        setLevel:Debug=test_yalo.cpp
    )";

    yalo::Logger::clearSinks();
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter(yalo::DefaultFormatter::GMT)));
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    yalo::Logger::setSettingsFile("bin/testCommandFileCreated.txt", 0);
    bool success = createFile("bin/testCommandFileCreated.txt", commands);

    lDebug << "testing";

    yalo::Logger::setSettingsFile("bin/nonexistant/path/testCommandFileCreated.txt");
    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter()));

    success = success && log.find("testing") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testCommandFileCreated()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testCommandFileUpdated() {
    const auto commands = R"(
        setFormatDefaultGMT
        setFormatDefault
        addSink: bin/testCommandFileUpdated.log
        resetLevels: Log
        pad
        noPad
        setLevel:Error
        setLevel:Debug=test_yalo.cpp
    )";
    const auto newCommands = R"(
        setFormatDefaultGMT
        addSink: bin/testCommandFileUpdated.log
        resetLevels: Log
        pad
        setLevel:Log
        setLevel:Debug=test_yalo.cpp
    )";

    yalo::Logger::clearSinks();
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter(yalo::DefaultFormatter::GMT)));
    yalo::Logger::resetLevels(yalo::Log);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));
    bool success = createFile("bin/testCommandFileUpdated.txt", commands);
    yalo::Logger::setSettingsFile("bin/testCommandFileUpdated.txt", 0);
    success = success && createFile("bin/testCommandFileUpdated.txt", newCommands);

    lDebug << "testing";

    yalo::Logger::setSettingsFile("bin/nonexistant/path/testCommandFileUpdated.txt");
    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter()));

    success = success && log.find("testing") != std::string::npos;

    if (!success) {
        fprintf(stderr, "testCommandFileUpdated()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

static bool testConditionals() {
    const auto value1 = 5;

    yalo::Logger::clearSinks();
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter(yalo::DefaultFormatter::GMT)));
    yalo::Logger::resetLevels(yalo::Warning);
    std::string log;
    yalo::Logger::addSink(std::unique_ptr<DebugSink>(new DebugSink(log)));

    lWarning(value1 > 2) << "too big";
    lError(value1 < 10) << "too small";

    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));
    yalo::Logger::setFormat(std::unique_ptr<yalo::DefaultFormatter>(new yalo::DefaultFormatter()));

    auto success = log.find("too big") != std::string::npos;
    success = success && log.find("too small") != std::string::npos;
    success = success && log.find("value1 > 2") != std::string::npos;
    success = success && log.find("value1 < 10") != std::string::npos;

    if (!success) {
        fprintf(stderr, "FAIL: testConditionals()\n");
        fprintf(stderr, "[%s]\n", log.c_str());
    }

    return success;
}

int main(const int /*argc*/, const char* const /*argv*/[]) {
    int failures = 0;
    enum TestEnum {One, Two, Three};

    yalo::Logger::addSink(std::unique_ptr<yalo::StdErrSink>(new yalo::StdErrSink()));
    yalo::Logger::addSink(std::unique_ptr<yalo::StdOutSink>(new yalo::StdOutSink()));
    yalo::Logger::clearSinks();
    yalo::Logger::addSink(std::unique_ptr<NullSink>(new NullSink()));

    for (int level = 0; level <= static_cast<int>(yalo::Trace); ++level) {
        if (!testLevel(static_cast<yalo::Level>(level))) {
            failures += 1;
        }
    }

    failures += TEST_TYPE(char) ? 0 : 1;
    failures += TEST_TYPE(unsigned char) ? 0 : 1;
    failures += TEST_TYPE(short) ? 0 : 1;
    failures += TEST_TYPE(unsigned short) ? 0 : 1;
    failures += TEST_TYPE(int) ? 0 : 1;
    failures += TEST_TYPE(unsigned int) ? 0 : 1;
    failures += TEST_TYPE(int16_t) ? 0 : 1;
    failures += TEST_TYPE(int32_t) ? 0 : 1;
    failures += TEST_TYPE(int64_t) ? 0 : 1;
    failures += TEST_TYPE(int8_t) ? 0 : 1;
    failures += TEST_TYPE(int_fast16_t) ? 0 : 1;
    failures += TEST_TYPE(int_fast32_t) ? 0 : 1;
    failures += TEST_TYPE(int_fast64_t) ? 0 : 1;
    failures += TEST_TYPE(int_fast8_t) ? 0 : 1;
    failures += TEST_TYPE(int_least16_t) ? 0 : 1;
    failures += TEST_TYPE(int_least32_t) ? 0 : 1;
    failures += TEST_TYPE(int_least64_t) ? 0 : 1;
    failures += TEST_TYPE(int_least8_t) ? 0 : 1;
    failures += TEST_TYPE(intmax_t) ? 0 : 1;
    failures += TEST_TYPE(intptr_t) ? 0 : 1;
    failures += TEST_TYPE(uint16_t) ? 0 : 1;
    failures += TEST_TYPE(uint32_t) ? 0 : 1;
    failures += TEST_TYPE(uint64_t) ? 0 : 1;
    failures += TEST_TYPE(uint8_t) ? 0 : 1;
    failures += TEST_TYPE(uint_fast16_t) ? 0 : 1;
    failures += TEST_TYPE(uint_fast32_t) ? 0 : 1;
    failures += TEST_TYPE(uint_fast64_t) ? 0 : 1;
    failures += TEST_TYPE(uint_fast8_t) ? 0 : 1;
    failures += TEST_TYPE(uint_least16_t) ? 0 : 1;
    failures += TEST_TYPE(uint_least32_t) ? 0 : 1;
    failures += TEST_TYPE(uint_least64_t) ? 0 : 1;
    failures += TEST_TYPE(uint_least8_t) ? 0 : 1;
    failures += TEST_TYPE(uintmax_t) ? 0 : 1;
    failures += TEST_TYPE(uintptr_t) ? 0 : 1;
    failures += TEST_TYPE(float) ? 0 : 1;
    failures += TEST_TYPE(double) ? 0 : 1;
    failures += testPointer() ? 0 : 1;
    failures += testTraceIf() ? 0 : 1;
    failures += testTraceWhile() ? 0 : 1;
    failures += TEST_SWITCH(char) ? 0 : 1;
    failures += TEST_SWITCH(unsigned char) ? 0 : 1;
    failures += TEST_SWITCH(short) ? 0 : 1;
    failures += TEST_SWITCH(unsigned short) ? 0 : 1;
    failures += TEST_SWITCH(int) ? 0 : 1;
    failures += TEST_SWITCH(unsigned int) ? 0 : 1;
    failures += TEST_SWITCH(int16_t) ? 0 : 1;
    failures += TEST_SWITCH(int32_t) ? 0 : 1;
    failures += TEST_SWITCH(int64_t) ? 0 : 1;
    failures += TEST_SWITCH(int8_t) ? 0 : 1;
    failures += TEST_SWITCH(int_fast16_t) ? 0 : 1;
    failures += TEST_SWITCH(int_fast32_t) ? 0 : 1;
    failures += TEST_SWITCH(int_fast64_t) ? 0 : 1;
    failures += TEST_SWITCH(int_fast8_t) ? 0 : 1;
    failures += TEST_SWITCH(int_least16_t) ? 0 : 1;
    failures += TEST_SWITCH(int_least32_t) ? 0 : 1;
    failures += TEST_SWITCH(int_least64_t) ? 0 : 1;
    failures += TEST_SWITCH(int_least8_t) ? 0 : 1;
    failures += TEST_SWITCH(intmax_t) ? 0 : 1;
    failures += TEST_SWITCH(intptr_t) ? 0 : 1;
    failures += TEST_SWITCH(uint16_t) ? 0 : 1;
    failures += TEST_SWITCH(uint32_t) ? 0 : 1;
    failures += TEST_SWITCH(uint64_t) ? 0 : 1;
    failures += TEST_SWITCH(uint8_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_fast16_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_fast32_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_fast64_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_fast8_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_least16_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_least32_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_least64_t) ? 0 : 1;
    failures += TEST_SWITCH(uint_least8_t) ? 0 : 1;
    failures += TEST_SWITCH(uintmax_t) ? 0 : 1;
    failures += TEST_SWITCH(uintptr_t) ? 0 : 1;
    failures += TEST_SWITCH(TestEnum) ? 0 : 1;
    failures += testBadLogFile() ? 0 : 1;
    failures += testLogFile() ? 0 : 1;
    failures += testException() ? 0 : 1;
    failures += testLoggerExcpetion() ? 0 : 1;
    failures += testPadding() ? 0 : 1;
    failures += testNoPadding() ? 0 : 1;
    failures += testThreading() ? 0 : 1;
    failures += testFormatGMT() ? 0 : 1;
    failures += testFilePattern() ? 0 : 1;
    failures += testFileNoPattern() ? 0 : 1;
    failures += testCommandFile() ? 0 : 1;
    failures += testCommandFileCreated() ? 0 : 1;
    failures += testCommandFileUpdated() ? 0 : 1;
    failures += testConditionals() ? 0 : 1;
    return failures;
}
