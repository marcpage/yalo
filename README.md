# yalo

Yet Another Logger C++ light-weight logging

## Features

- Single header file, less than 1,000 lines of code
- Supports Linux, macOS, and Windows
- Threadsafe (and thread identification)
- Minimal coding to log
- Logs timestamp, thread, [level](#logging-levels), file, line number, function, condition
- Can log to `stdout`, `stderr`, and/or files
- Can write custom code to log to other destinations
- Can write custom code to change the format of the logging output
- Can create/update a [file to change logging settings while code is running](#changing-log-levels-at-runtime)
- Can automaticaly log all `if`, `while`, and `switch` statements in `Trace` mode
- Can [set logging level per file or group of files](#setlevel)

## Logging Levels

```C++
yalo::Log = 0;      // Always log
yalo::Error = 1;    // Log errors (default) - likely problems
yalo::Warning = 2;  // Log warnings - probably not see problems
yalo::Info = 3;     // Log information - general information
yalo::Debug = 4;    // Log debug - information to help with debugging
yalo::Verbose = 5;  // Log verbose - more info than necessary
yalo::Trace = 6;    // Log trace - log every if, switch, and while
```

### Example logging code

```C++
    lLog << "Program version" << version_string;
    lErr << "We're all going to die!";
    lError(file == nullptr) << "crash eminent!";
    lWarn << "Warning!";
    lWarning(size > 5) << "We should work but this is kind of big" << size;
    lInfo << "How did we get here?";
    lDebug << "iteration #" << iteration << "size = " << size;
    lVerbose << "Checking if we should move forward";
    lTrace << "Here";
    if (x == 5 && y == 7) {...}
```

### Example log

The log can output the following information:

- Timestamp (local or GMT)
- Thread index (index of the order in which the threads logged)
- Log [level](#logging-levels)
- Source file
- Source line number
- Function or method
- Conditional that triggered the logging
- The log message

```html
[2025-02-23 02:52:22.912 (Sun)][0][LOG] New Settings File: bin/testCommandFile.txt
[2025-02-23 02:52:22.912 (Sun)][0][LOG] Resetting format to default GMT
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] Resetting format to default
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] Adding sink to bin/testCommandFile.log
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] resetLevels to 0
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] Turned padding on
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] Turned padding off
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] Set level #1 pattern = ''
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] Set level #4 pattern = 'test_yalo.cpp'
[2025-02-22 20:52:22.912 -0600 (Sat)][0][DBG][src/tests/test_yalo.cpp:495][testCommandFile] testing
[2025-02-22 20:52:22.912 -0600 (Sat)][0][LOG] New Settings File: bin/nonexistant/path/testCommandFile.txt
[2025-02-23 02:52:22.912 (Sun)][0][WRN][src/tests/test_yalo.cpp:523][testConditionals][value1 > 2] too big
[2025-02-23 02:52:22.912 (Sun)][0][ERR][src/tests/test_yalo.cpp:524][testConditionals][value1 < 10] too small
[2025-02-23 02:52:22.913 (Sun)][0][LOG] New Settings File: bin/nonexistant/path/testCommandFile.txt
```

## Changing log levels at Runtime

In the code you can specify a path to a file to watch for logging settings.

```
setLevel: Error
setLevel: Trace=main.cpp
addSinkStdErr
addSink:debugging.txt
```

### addSink

Same as calling `Logger::addSink(std::unique_ptr<FileSink>(new FileSink({path})));`

```
addSink:{path}
```

Starts logging to the given path.

### addSinkStdErr

Same as calling `Logger::addSink(std::unique_ptr<StdErrSink>(new StdErrSink()));`

```
addSinkStdErr
```

Starts logging to `stderr`.

*Note*: If you are already logging to `stderr` or specify this command more than once you will see duplicate lines.

### addSinkStdOut

Same as calling `Logger::addSink(std::unique_ptr<StdOutSink>(new StdOutSink()));`

```
addSinkStdOut
```

Starts logging to `stdout`.

*Note*: If you are already logging to `stdout` or specify this command more than once you will see duplicate lines.

### clearSinks

Same as calling `Logger::clearSinks();`

```
clearSinks
```

Removes all current logging sinks.

*Note*: If this is not immedeately followed by `addSinkStdOut`, `addSinkStdErr`, or `addSink` then `addSinkStdErr` will automatically happen.

### noPad

Same as calling `Logger::setInserterSpacing(InserterAsIs);`

```
noPad
```

When log streaming, no extra spacing will be inserted between inserts.

```
lLog << "test" << 5 << "more";
```

will result in logging:

```
test5more
```

### pad

Same as calling `Logger::setInserterSpacing(InserterPad);`

```
pad
```

When log streaming, an extra space will be inserted between inserts.

```
lLog << "test" << 5 << "more";
```

will result in logging:

```
test 5 more
```

### resetLevels

Same as calling `Logger::resetLevels(_fromString({level}));`

```
resetLevels:{level}
```

Clears all file pattern files and sets all logging to [{level}](#logging-levels).

### setFormatDefault

Same as calling `Logger::setFormat(std::unique_ptr<DefaultFormatter>(new DefaultFormatter()));`

```
setFormatDefault
```

Reset the log formatter back to the default, using local time.

### setFormatDefaultGMT

Same as calling `Logger::setFormat(std::unique_ptr<DefaultFormatter>(new DefaultFormatter(DefaultFormatter::GMT)));`

```
setFormatDefaultGMT
```

Reset the log formatter back to the default, using GMT time.

### setLevel

Same as calling `Logger::setLevel({level}, {pattern});`
```
setLevel:{level}
setLevel:{level}={pattern}
```

The first entry will set the default global [{level}](#logging-levels).
The second entry will set the [{level}](#logging-levels) for the given [file pattern](#file-patterns).

#### File Patterns

You can specify a file matching pattern to change the logging level per file or group of files.
The pattern consists of case-sensitive substrings to look for in the file, separated by semicolon (`;`).
If a pattern begins with a minus (`-`) then it will not match that pattern.
The evaluation walks through all the patterns with positive patterns being ORed in and negative patterns removing files.

##### File pattern example

```
src/;-src/include/;-main.cpp
```

This is interpreted as: files that contain `src/` in them, but do not include `src/include` and do not include `main.cpp`.

Basically take your filename and walk through the pattern and add it if at matches a positive and remove it when it matches a negative.

```
src/;-src/include/;.cpp
```

This example shows all files under `src/`, but not files in `src/include/`, but do include `.cpp` files (including those in `src/include/`).
