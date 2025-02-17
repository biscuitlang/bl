# System

Collection of operating system interface imported implicitly.

## Standard IO

Standard input and output is implemented using the builtin [Stream](modules_io.html) abstraction, so all common stream manipulation methods
like `read` and `write` can be used. Use following methods to obtain the stream handle.

```bl
#import "std/io"

OsStdIoStream :: struct #base Stream {
    handle: win32.HANDLE;
}

// Standard input stream
os_stdin :: fn () *OsStdIoStream
// Standard output stream.
os_stdout :: fn () *OsStdIoStream
// Standard output error stream.
os_stderr :: fn () *OsStdIoStream
```

In general, you can use i.e. Standard Output Stream for printing into the console, however using [print](modules_print.html) function is
more elegant in most situations.

!!! note
    On Windows the terminal output is encoded to UTF-8 by default using winapi function `SetConsoleOutputCP`.

## os_execute

```bl
os_execute :: fn (command: string_view, _allocator : *Allocator = null) s32 #inline
```

Execute shell `command` and return the command output state as an integer. Internally allocates memory using passed `_allocator`, in case no allocator is specified, `application_context.temporary_allocator` is used.

## os_get_last_error

```bl
os_get_last_error :: fn () (s32, string_view) #inline
```

Return last known operating system dependent error code.

## os_get_exec_path

```bl
os_get_exec_path :: fn () string
```

Returns a full path to the currently running executable; internally a new string is allocated and must be deleted after use.
The path may be empty in case of an error.

## os_get_backtrace

```bl
os_get_backtrace :: fn (skip_frames := 0, max_frame_count := 64, _allocator : *Allocator = null) []CodeLocation
```

Returns current execution stack trace obtained from native executable debug information. Function is available only in native binary execution mode. Output slice of [CodeLocations](modules_a.html#CodeLocation) contains stack frame records starting from the `os_get_backtrace` caller function + `skip_frames`. The `max_frame_count` can limit maximum count of obtained frames.

Internally allocates memory using passed `_allocator`, in case no allocator is specified, `application_context.temporary_allocator` is used.

!!! warning
    Currently this function is implemented only on Windows and does nothing on all other supported platforms.
