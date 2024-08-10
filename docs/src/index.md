# Biscuit Language

The Biscuit Language (BL) is simple imperative programming language using LLVM backend implemented
in C. Language syntax and all it's features are still in development and not ready for 'real' use
yet. Biscuit is designed to be simple, fast and explicit.

- Simple small language.
- Manual memory management.
- ABI compatibility with C libraries.
- Game development oriented.
- Compilation to native binary.
- Integrated interpreter.
- Offer testing tools out of the box.
- Rich type info in runtime.
- Debugging in gdb, lldb and Visual Studio.

## Example

```bl
HelloWorld :: struct {
    hello: s32;
    world: s32;
};

main :: fn () s32 {
    info :: cast(*TypeInfoStruct) typeinfo(HelloWorld);

    loop i := 0; i < info.members.len; i += 1 {
        print("% ", info.members[i].name);
    }
    print("!!!\n");

    return 0;
}
```