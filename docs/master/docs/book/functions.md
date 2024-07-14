# Functions

A function is a chunk of code representing a specific piece of the program functionality introduced by *fn* keyword. A function can be called using the call operator `()`, we can provide any number of arguments into the function and get *return* value back on a call side.

A function is usually associated with a name and can be later called by this name followed by the call operator. To keep the syntax consistent, the function declaration looks similar to a variable declaration. The philosophy under the hood is the same, we associate some symbol (name) with some value (in this case a function literal). However, a function declaration is required to be always immutable.

Functions can be declared in the global or a local scope (one function can be nested in another one).

```rust
my_global_function :: fn () {
    print("Hello from global function!!!\n");
} // Semicolon is optional here.

test_fn :: fn () #test {
    my_local_function :: fn () {
        print("Hello from local function!!!\n");
    }; // Semicolon is required here.

    // Call functions.
    my_global_function();
    my_local_function();
}
```

!!! note
    Local functions cannot use any variables from the parent function. No variable capturing is supported, for now, you have to pass a context explicitly into the function as an argument.

## Function Arguments

Function arguments are values passed into the function from the outside. Arguments, the function expects, are listed in the *argument list* in the function declaration. Each argument is declared as the name and type separated by the semicolon. Function arguments in BL are immutable (an argument itself cannot be changed in the function body). See the following example:

```rust
Person :: struct {
    age: s32;
}

// Function taking two arguments.
my_function :: fn (person_1: *Person, person_2: Person, age: s32) {
    age = 30; // This is invalid 'age' is immutable.
    person_1 = null; // The same for the 'person_1' argument.

    // The 'person_1' here is immutable (you cannot change the pointer to something else),
    // however you can modify members using this pointer.
    // The C equivalent would be something like 'struct Person *const person_1'.
    person_1.age = age;

    // On the other hand the 'person_2' is passed into the function by value, so the
    // following expression is invalid; you cannot modify its members.
    parson_2.age = age;

    // You can use de-reference to modify the whole person data; you do not change
    // the pointer itself (it still points to the same memory, just content of this
    // memory is being changed).
    person: Person;
    @person_1 = person;

    // In case you want to modify the argument value, you should create a local copy.
    local_age := age;
    local_age += 2;
}

```

### Arguments With Default Value

In some cases we want to provide less boilerplate API and call functions only with some of the arguments from the argument list, this is possible using argument default values.

```rust
compare :: fn (a: f32, b: f32, epsilon: f32 = 0.1f) bool {
    return std.abs(a - b) < epsilon;
}

main :: fn () s32 {
    compare(13.f, 12.f, 0.001f); // We don't use the default value.
    compare(13.f, 12.f); // We use default value.

    return 0;
}
```

The explicit `f32` type is optional for the `epsilon` with default value since we have the value to get the type from, so the following code is also valid.

```rust
compare :: fn (a: f32, b: f32, epsilon := 0.1f) bool {
    return std.abs(a - b) < epsilon;
}
```

One limitation here is that the arguments with default values must go very last in the argument list. Currently, there is no way how to specify namely which argument we want to call on the call side.

```rust
compare :: fn (epsilon := 0.1f, a: f32, b: f32) bool {
    return std.abs(a - b) < epsilon;
}
```

```text
test2.bl:1:16: error(0035): All arguments with default value must be listed last in the function
                            argument list. Before arguments without default value.
>  1 | compare :: fn (epsilon := 0.1f, a: f32, b: f32) bool {
     |                ^^^^^^^
   2 |     return std.abs(a - b) < epsilon;
```

### Call Location

One special feature very useful for debugging is `#call_location` which can be used as a default argument value. Each time the function is called, the `#call_location` is replaced by a pointer to the `CodeLocation` variable, containing information about where the function was called in the source code.

```rust
my_assert :: fn (expression: bool, location : *CodeLocation = #call_location) {
    if !expression {
        print("Assert called in '%' on line %.\n", location.file, location.line);
    }
}

main :: fn () s32 {
    my_assert(false);
    return 0;
}
```

```text
Assert called in 'C:/Develop/bl/tests/test2.bl' on line 9.
```

### Compile-time Known Arguments

So far, in all examples, arguments passed to functions were processed in runtime. In some cases, we may require the argument to be compile-time known. A function having at least one compile-time known argument is called *mixed* function. We can do so simply by adding `#comptime` directive after the argument declaration.

```rust
load_data :: fn (BUFFER_SIZE: s32 #comptime) {
    // 'SIZE' is compile-time known constant here.
    buffer: [BUFFER_SIZE]s32;
    // ...
}
```

Since the `BUFFER_SIZE` is compile-time known constant, it can be used as size in an array type definition. This obviously means the `BUFFER_SIZE` argument needs to be compile-time constant when the function is called, otherwise the compiler generates an error.

Internally, `load_data` function does not exist until the compiler hits a call to this function; we don't know what the value of `BUFFER_SIZE` is in advance. The compiler will generate a unique implementation with `BUFFER_SIZE` argument removed from the argument list and converted into compile-time known constant value for each compiled call to this function in the code. At this point, you may see some possible disadvantages. Since the compile-time argument is removed from the argument list, the *mixed* function cannot follow *C call conventions* and cannot be *exported* or *external*. Also, instantiating a new function implementation for each call in the code can lead to a bigger executable and slow down the compiler.

One important thing, we can do with *mixed* functions is having also types as input arguments. See the implementation of `new` function from the *standard library*:

```rust
// Allocate memory on heap for value of 'T' type.
new :: fn (T: type #comptime, preferred_allocator: *Allocator = null, loc := #call_location)
          (ptr: *T, err: Error) #inline {
    mem, err :: alloc(sizeof(T), alignof(T), preferred_allocator, loc);
    return auto mem, err;
}

main :: fn () s32 {
    number_on_heap :: new(s32);
    free(auto number_on_heap);
    return 0;
}
```

** Pros **

- We can pass types into functions.
- We can reference the compile-time arguments in the argument list (i.e. use them as return type).

** Cons **

- Can produce larger binary and slow down the compilation process.
- Compile-time known argument cannot be used as a default value of another argument.
- Type-checking is very limited since we don't know `comptime` arguments in advance; the generated implementation is type-checked each time the function is called.
- Mixed functions do not represent any stack-allocated memory (we cannot get its address).
- Don't follow C calling conventions:
    - Cannot be `extern`.
    - Cannot be `export`.

### Variable Argument Count

In BL, we can have a function taking 0-N values in the argument list, let's start with an example:

```rust
sum :: fn (nums: ...s32) s32 {
    // nums is slice of s32
    result := 0;
    loop i := 0; i < nums.len; i += 1 {
        result += nums[i];
    }

    return result;
}

main :: fn () s32 {
    print("%\n", sum(10, 20, 30));
    print("%\n", sum(10, 20));
    print("%\n", sum());

    return 0;
}
```

The `nums` argument type is `...s32`, that means we expect any number of  `s32` integers to be passed into the function. This is just syntax sugar for passing a pointer to an array of integers. When `sum` function is called, the compiler will implicitly generate a temporary array containing all passed arguments and then forward this array into the function. Inside the function we can use common `.len` slice member to get a count of passed integers and access each one using `[]` operator. This approach may cause some overhead compared to C version of the same feature, however, it's way more safe and ergonomic.

We can use `...Any` to allow values of any type to be passed into the function, or just `...` shortcut to do the same. One good example is the `print` function from the *standard library*.

```rust
print :: fn (format: string_view, args: ...) s32 {
    // ...
}
```
** Pros **

- We can pass any number of arguments we want.
- Safe and easy to use.

** Cons **

- Some overhead may be introduced by implicit conversion to an array.
- Must be the last in the argument list.
- Don't follow C calling conventions:
    - Cannot be `extern`.
    - Cannot be `export`.

## Return Value

Each function can eventually return some value using the *return* statement. The *return* statement returns the execution back to the caller, so the execution of the function ends in case the *return* is reached. The return value is optional and can be specified in the function declaration *header* after the argument list. There is no *void* type (like in C or C++) to say the function does not return, we simply leave the return value type empty. The following example shows the function without the return value.

```rust
say :: fn (is_hello: bool) {
    if is_hello {
        print("Hello!");
        return; // We can use 'return' without values.
    }
    print("Hi!");
    // Return here is optional.
}
```

Another example shows the function returning an integer.

```rust
add :: fn (a: s32, b: s32) s32 {
    return a + b; // Return is mandatory.
}

main :: fn () s32 {
    result := add(1, 2);
    return 0;
}
```

A function may return multiple values at once like this:

```rust
foo :: fn () (s32, bool) {
    // We separate each value by comma.
    return 666, true;
}

main :: fn () s32 {
    // s32 goes into int1 and bool into boolean1
    int1, boolean1 := foo();

    // Not all values must be used.
    // s32 value goes into int2.
    int2 := foo();

    // We can use '_'  blank identifier to ignore some values.
    _, boolean2 := foo();

    // We can access each return value like it was structure.
    boolean3 := foo()._0; // '_0' is builtin index of first value.
    boolean3 := foo()._1; // '_1' is builtin index of second value.

    return 0;
}
```

The returned result values can be *unrolled* on the call side; we can initialize more variables with different type at once, but we have to keep the order of return values specified in the function declaration.

Each returned value can have a name:

```rust
foo :: fn () (number: s32, is_valid: bool) {
    return 666, true;
}

main :: fn () s32 {
    // s32 goes into int1 and bool into boolean1
    int1, boolean1 := foo();

    // We can access each return value like it was structure.
    boolean2 := foo().is_valid;

    return 0;
}
```

Internally the compiler creates an implicit structure and returns all values as a single one; that's why the call-side use of the call results is called *unrolling*. In case we return a lot of values, the compiler may introduce some optimizations to avoid returning large data from a function.

This feature comes in handy in cases where we want to include also a possible *error* as the result. One common approach to addressing an error handling goes like this:

```rust
{% include "../examples/open_file.bl" %}
```

The `open_file` function in this case returns the file stream and possible error in case the file cannot be opened.

## Anonymous Functions And Callbacks

Sometimes a function may be used only once as an *callback* function passed into some other function. In such a case we can simplify the declaration and keep the function unnamed. One good example is the `sort` function declared like this:

```rust
sort :: fn (list: []s32, cmp: *fn(a: *s32, b: *s32) bool)  {
    loop i := 0; i < list.len; i += 1 {
        loop j := i+1; j < list.len; j += 1 {
            if cmp(&list[i], &list[j]) {
                swap(&list[i], &list[j]);
            }
        }
    }
}
```

We pass a `list` slice of numbers and we want it to be sorted with use of some custom `cmp` comparator. The comparator in this case is a pointer to any function taking *\*s32*, *\*s32* and returning *bool*. The easiest way to provide such a function is to create an anonymous callback and pass its address.

```c
main :: fn () s32 {
    numbers: []s32;
    alloc_slice(&numbers, 10);
    defer free_slice(&numbers);
    loop i := 0; i < numbers.len; i += 1 { numbers[i] = i; }
    print("%\n", numbers);

    // Here we pass pointer to anonymous function into the 'sort'.
    sort(numbers, &fn (a: *s32, b: *s32) bool {
        return @a < @b;
    });

    print("%\n", numbers);
    return 0;
}
```
## Function Overloading

More functions can be associated with one name with explicit function overloading groups. A call to a group of functions is replaced with a proper function call during compilation, based on provided arguments.

```rust
group :: fn { s32_add; f32_add; }

s32_add :: fn (a: s32, b: s32) s32 {
    return a + b;
}

f32_add :: fn (a: f32, b: f32) f32 {
    return a + b;
}

main :: fn () s32 {
    i :: group(10, 20);
    j :: group(0.2f, 13.534f);
    print("i = %\n", i);
    print("j = %\n", j);
    return 0;
}
```
!!! note
     There is no additional runtime overhead caused by function overloading.

!!! note
     Ordering of functions inside the group is arbitrary.


Functions can be declared directly inside the overload group:

```rust
group :: fn {
    fn (a: s32, b: s32) s32 {
        return a + b;
    };

    fn (a: f32, b: f32) f32 {
        return a + b;
    };
}
```

### Overload Resolution

When a function group is called the function overload resolution takes into account multiple options to sort all possible call candidates by priority. The candidate function with the highest priority is used. In case there are multiple functions with the same priority found in the group, the compiler complains about ambiguous a function. In case there is no call candidate, the first one is used. This usually leads to an error later if the function interface is not compatible.

**The overload resolution is based on:**

1. Argument count.
2. Argument types.
3. Type casting.
4. Conversion to slice.
5. Conversion to any.

!!! note
     The return type does not affect choosing the best call candidate.

**Resolving the best call candidate is done in two passes:**

1. Pick all possible candidates based on call-side argument count when:
    - The argument count is exactly matching the count of arguments required by the function interface.
    - All arguments, up to the first defaulted or variable count argument in the function interface, are provided.

2. Iterate over previously picked functions and rank them by comparing call-side arguments with each function's interface arguments one by one:
    - The type is exactly the same. (Rank +3)
    - Can be implicitly casted. (Rank +2)
    - Can be implicitly converted. (Rank +2)
    - Can be implicitly converted to `Any`. (Rank +1)
    - Can be added to the variable count argument array. (Rank +1)

3. Use the function with the highest rank.

```rust
a :: fn (_: []u8)                {}
b :: fn (_: string)              {}
c :: fn (_: Any)                 {}
d :: fn (_: s32, _: bool = true) {}

group :: fn { a; b; c; d; }

// a: rank = 3 <- used
// b: rank = 0
// c: rank = 1
// d: rank = 0
group("hello");

// a: rank = 2 (can be implicitly converted to []u8)
// b: rank = 3 <- used
// c: rank = 1
// d: rank = 0
str: string;
group(str);

// a: rank = 0
// b: rank = 0
// c: rank = 1
// d: rank = 3 <- used
group(10);

// a: rank = 0
// b: rank = 0
// c: rank = 0
// d: rank = 6 <- used
group(10, false);

// a: rank = 0
// b: rank = 0
// c: rank = 1
// d: rank = 2 <- used (implicitly casted s8 to s32)
i: s8;
group(10);
```

## Polymorphic Functions

Polymorphic function (aka templated function or generic function) is a well-known concept in many programming languages. It's a sort of meta-programming method reducing a boilerplate code. The basic idea is the automatic generation of functions doing the same operation but using different types. Instead of rewriting the function for every type needed, we just specify it as a "recipe" for later generations.

Consider the following function doing an addition of two values, when we want to use the function with multiple different types, we must explicitly rewrite the same function for every type needed:

```c
add_s32 :: fn (a: s32, b: s32) s32 {
    return a + b;
}

// Same for floats.
add_f32 :: fn (a: f32, b: f32) f32 {
    return a + b;
}
```

In this case, we can use polymorph instead:

```c
add :: fn (a: ?T, b: T) T {
    return a + b;
}
```

Value of `T` represents any type, in this case, chosen based on usage of the function. The question
mark before `T` says the first `T` is the master polymorph type. The compiler tries to replace all
master types with the type of argument on the call side and register the new type alias `T` in the
function scope.

Example of usage:

```c
main :: fn () s32 {
    result_1 : s32 = add(10, 20);
    result_2 : f32 = add(1.4f, 42.5f);

    return 0;
}
```

Notice that we call the same function, first with integers and second with floats. The type of `T`
is based on the first argument type (because the master type is defined as the type of the first
argument).  The second argument type, in this case, must be the same type as the master
because `b` use, as its type, alias `T`. The same alias is used also as a return type.

So two functions are generated internally:

```c
.add.1 :: fn (a: s32, b: s32) s32 {
    return a + b;
}

.add.2 :: fn (a: f32, b: f32) f32 {
    return a + b;
}
```

!!! note
    Content of polymorphic function is semantically analyzed only when the function is used.

### Nested Master Type

Polymorph master type replacement can be used also as a nested member in more complex types.

```rust
sum :: fn (slice: []?T) T {
    result: T; // We can use T inside the function as well
    loop i := 0; i < slice.len; i += 1 {
        result += slice[i];
    }
    return result;
}

// Sum function accepts any slice as an input and any array is implicitly convertible
// to slice.

// Use the function with static array
arr_static := [3]s32.{10, 20, 30};
sum(arr_static); // T = s32

// Use the function with dynamic array
arr_dynamic: [..]f32;
defer array_terminate(&arr_dynamic);
array_push(&arr_dynamic, 10.f);
array_push(&arr_dynamic, 20.f);
array_push(&arr_dynamic, 30.f);

sum(arr_dynamic); // T = f32
```

### Multiple Polymorph Masters

More than one polymorphic master can be declared inside the function argument list:

```rust
table_insert :: fn (table: *Table, key: ?TKey, value: ?TValue) {
    // ...
}
```

### Specify Implementation For Type

In some cases we want to specify explicitly what implementation should be used for some specific type, i.e. in a function doing a comparison of two values, we can provide specific handling for string:

```rust
is_equal :: fn { // function group
    // Implementation used for strings only.
    fn (a: string_view, b: string_view) bool {
        return std.str_compare(a, b);
    };

    // Implementation used for all other types.
    fn (a: ?T, b: T) bool {
        return a == b;
    };
}
```

**Limitations:**

   - Polymorph master type cannot have a default value.
   - Type-checking is very limited since we don't know types of the arguments in advance.
   - Polymorph functions does not represent any stack allocated memory (we cannot get its address).
   - Don't follow C calling conventions:
       - Cannot be `extern`.
       - Cannot be `export`.

## Function Directives

The function directives can be specified after the function return type declaration:

```rust
get_age :: fn () s32 #inline {}
```

### inline/noinline

Tells the compiler whether it should try to inline the called function. Inlining may not be possible in some cases, however in general it can improve the runtime speed. Inline functions should not be too complex.

### extern

An extern function is a function implemented in a foreign library linked to the program. Such a function defines only an interface but cannot be implemented (does not have a body). The `#extern` directive can be optionally followed by the linkage name of the external symbol. If the linkage name is not specified, the function name is used instead. Having external functions allows the use of any existing C ABI compatible library.

The extern functions must strictly follow *C call conventions*.

```rust
my_malloc :: fn (size: size_t) void_ptr #extern "malloc";
free :: fn (ptr: void_ptr) #extern;
```

### export

Functions with an `export` directive are exported from the binary when a program is compiled as a shared library (with `-shared` flag). So the function may be called from the other libraries or executables after successful linking. The `#export` directive can be optionally followed by the linkage name of the exported symbol. If the linkage name is not specified, the function name is used instead.

The export functions must strictly follow *C call conventions*. That means, the function cannot be polymorphic (generated in compile time).

```rust
my_add :: fn (a: s32, b: s32) s32 #export "add" {
    return a + b;
}
```

### comptime

Every call to such a function is going to be evaluated in compile-time and replaced by constant eventually.

```rust
hash_string :: fn (s: string_view) u32 #comptime {
    return std.str_hash(s);
}
```

The `hash` function can be later called the same way as any other regular function, but the call itself is replaced by the constant result value in the final binary.

```rust
main :: fn () s32 {
    // 'hash_string' is executed in compile-time and 'hash' value is initialized
    // later in runtime with pre-calculated constant.
    // Called function does not exist in the final binary.
    hash := hash_string("Hello!");
    print("%\n", hash);
    return 0;
}
```

So the comptime function has no runtime overhead.

**Pros:**

- Since all comptime functions are evaluated in compile-time, there is no runtime overhead.
- The result of the comptime function call is also compile time known.
- Compile time function can return types and can be used in type definitions.

**Cons:**

- Every argument passed, must be known in compile-time.
- Returning pointers from comptime functions is not a good idea.
- An internal execution stack for compile-time evaluated functions is limited to 128kB; compile time execution of too complicated stuff may cause stack overflows.

### enable_if

The `#enable_if` directive can be used to conditionally specify whether a certain function should be included or excluded from a final binary. This might be used for debug-only functions like debug logs, profiling code etc.

Following code is supposed to measure the runtime of the main function only in debug mode (when `bool` expression after `#enable_if` directive evaluates `true` in compile-time). Calls to these functions are completely removed in release mode as well as the implementation.

```rust
main :: fn () s32 {
    measure_runtime_in_debug_only();
    defer measure_runtime_in_debug_only_end();

    // do something here

    return 0;
}

measure_runtime_in_debug_only :: fn () #enable_if IS_DEBUG {
    // ...
}

measure_runtime_in_debug_only_end :: fn () #enable_if IS_DEBUG {
    // ...
}
```

**Notes:**

- The function and all its calls are fully analyzed even if the function is disabled.
- The conditional function might return values, but in case the function is disabled the returned value on the call side is implicitly changed to `void` type. Such behavior is intentionally chosen to prevent possible issues with uninitialized variables.
