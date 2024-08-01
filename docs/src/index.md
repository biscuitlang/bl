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

## Master (0.11.0)

The latest unstable version of the compiler.

- [Github Page](https://github.com/travisdoor/bl)

### Changelog
```text
Add 'then' keyword and inline if statements.
Add --dirty-mode compiler flag.
Add SIMD implementation for some hot code paths (Windows only).
Add automatic static array element count using [_]s32 syntax for compound initializers.
Add builder configuration api for build pipelines.
Add dlib VM runtime support.
Add fmod and trunc math functions.
Add support for x86_64-unknown-linux-gnu target architecture triple.
Add ternary if expression.
Add untyped compound expressions.
Fix group initialization of multiple variables at once in global scope.
Fix invalid position of vargs generated before call causing stack problem in VM execution.
Fix member access directly on call result.
Fix reports of unused symbols declared after usage in local scopes.
Fix rpmalloc missing terminations for threads causing deadlocks sometimes.
Fix unary not operator precedence.
Fix unroll on global variables.
Improved documentation generator (added more value expressions).
RPMalloc used for the compiler.
Reduced amount of types generated internally by the compiler (use cache).
Reworked thread pool.
Rewrite string handling inside the compiler.
```

## Release (0.10.0)

The latest release compiler version documentation can be found [here](versions/0.10.0/).

## Older Versions

- [0.9.0](versions/0.9.0/)
- [0.8.0](versions/0.8.0/)
- [0.7.2](versions/0.7.2/)
- [0.7.1](versions/0.7.1/)
- [0.7.0](versions/0.7.0/)
- [0.6.0](versions/0.6.0/)
