# System

Collection of operating system interface imported implicitly.

## Standard IO

Standard input and output is implemented using the builtin [Stream](/modules/io) abstraction, so all common stream manipulation methods
like `read` and `write` can be used. Use following methods to obtain the stream handle.

```c
OsStdIoStream :: struct #base std.Stream {
    handle: win32.HANDLE;
}

// Standard input stream
os_stdin :: fn () *OsStdIoStream
// Standard output stream.
os_stdout :: fn () *OsStdIoStream
// Standard output error stream.
os_stderr :: fn () *OsStdIoStream
```

In general, you can use i.e. Standard Output Stream for printing into the console, however using [print](/modules/print) function is
more elegant in most situations.

!!! note
    On Windows the terminal output is encoded to UTF-8 by defautl using winapi function `SetConsoleOutputCP`.

## os_execute

```c
os_execute :: fn (command: string_view) s32
```

Execute shell `command` and return the command output state as an integer.

## os_get_last_error

```c
os_get_last_error :: fn () (s32, string_view) #inline
```

Return last known operating system dependent error code.

## os_get_exec_path

```c
os_get_exec_path :: fn () string
```

Returns a full path to the currently running executable; internally a new string is allocated and must be deleted after use.
The path may be empty in case of an error.

## os_get_backtrace

```c
os_get_backtrace :: fn (skip_frames := 0, max_frame_count := 64) []CodeLocation
```

Returns current execution stack trace obtained from native executable debug information. This feature is available only in `DEBUG` mode
and only during native runtime. Output slice of [CodeLocations](/modules/a/#codelocation) contains stack frame records starting from the
`os_get_backtrace` caller function + `skip_frames`. The `max_frame_count` can limit maximum count of obtained frames.

!!! note
    This function internally allocate using current application context temporary allocator.

!!! warning
    Currently this function is implemented only on Windows and does nothing on all other supported platforms.
