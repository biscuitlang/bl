# Introduction

*Note that the following documentation may be in some cases incomplete and we'll try to improve it soon...*

Biscuit Language (BL) is an experimental, general-purpose, compiled or interpreted programming language inspired by new C/C++ followers such as Odin, JAI and Zig. The main goal of the language is to create a simple, explicit and pleasant environment for programmers looking for something more abstract than C but not bloated as C++.

Due to this, some of the popular and common concepts are not supported in the language, i.e. there are no *objects*, complicated inheritance or exceptions in BL. However, some higher-level features such as polymorphic (template) functions, function overloading, runtime type information and compile-time execution are present.

BL compiler is open-source software (distributed under the MIT license) written in C. The compiler itself comes with a slowly growing *Standard Library* providing various functionality out of the box. The BL compiler uses *LLVM* backend to produce the final optimized executable.

Currently, all major 64-bit operating systems are supported (Windows, Linux and Mac), however, most of the language development is done on Windows and the compiler or *Standard Library* may be less reliable on other platforms.

The following sections of this document should guide you through the language from simple, fundamental basics to more complicated concepts. We suppose the reader is familiar with other programming languages such as C or C++ and does have some basic knowledge of how to use a terminal and other programming tools.

So let's start with *Hello World* program...

# Hello World

In this chapter, we try to create a simple BL program printing out "Hello world!" message into the command line.

## The Main Function

To start a new project, we simply need to create a file containing the *main* function used as an application entry point. So let's call our file *hello.bl*. Every BL executable requires the *main* function to be present in the program. Every *main* function is supposed to return *s32* (32-bit signed number) as its result state.

The content of *hello.bl* can look like this:

```bl
#import "std/print"

main :: fn () s32 {
	print("Hello world!\n");
	return 0;
}
```

### Print

Inside the function body, we call *print* function to print the passed string "Hello world!" into the standard output. The declaration of the *print* function may look like this:

```bl
print :: fn (format: string_view, args: ...) s32 {
	// ...
}
```

It's declared inside *std/print* module. From the declaration you can see it takes *format* and *args* as input arguments. The type of the *format* argument is compiler internal type *string_view* (representation of any string in the language); *args* argument type is *...* which allows any number of additional values to be passed into the function.

### Return

The last statement in the *main* function body is the `return` statement. The `return` statement, as in other languages, terminates the execution of the current function and provides some return value. In our case, it's just a 0.

## Compile & Run

To compile our simple program, just type `blc hello.bl` into the terminal (in the directory containing `hello.bl` file). The new `out.exe` executable should be created next to your source file.

This way the BL compiler (called `blc`) can produce native executable binary for the platform you're running, however, since the BL code can be also interpreted, we can directly execute our program using the same `blc` command by adding `-run` parameter before the source file name: `blc -run hello.bl`. The *main* function from our example is executed without any native compiled binary creation needed.

The output should look like this:

```bash
blc -run hello.bl

Executing 'main' in compile time...
Hello world!
Execution finished with state: 0

Finished in 0.031 seconds.
```

# Hello Project

As you maybe noticed from the previous example, our executable file was called *out.exe*, so what if we want to change the name without needing to do it manually after each recompilation? To fix this we need to feed the compiler with some additional settings; we need to create a configuration build file to do so.

One common approach to adjusting the build settings configuration is having some kind of build system involved in the language ecosystem. If you're coming from the C/C++ world, you may know *CMake* or similar tools to manage the project configuration and compilation. However, instead of introducing a whole new system and language, programmers must know to build something, we decided to do it using BL itself, and integrate such a feature directly into the compiler.

## Configuration

To use the BL compiler-integrated build pipeline, we have to create the configuration file first. The compiler expects the project's configuration file to be called `build.bl`. We can reuse our "Hello World" program from the previous chapter, and create `build.bl` file next to it. The most basic build configuration looks like this:

```bl
build :: fn () #build_entry {
    exe := add_executable("hello");
	add_unit(exe, "hello.bl");
    compile(exe);
}
```

The function *build* marked as a *build_entry* is later automatically executed by the compiler. Inside the function body, we create a new executable target "hello" by *add_executable* function call. Then we add our `hello.bl` source file into it, and finally, we compile the target.

## Compilation

To use our new build script, simply type `blc -build` into the terminal (in the folder where the `build.bl` is located). The newly created executable `hello.exe` should be generated next to the build file.

## How It Works

The build script file is nothing else than the regular BL program executed directly when `-build` compiler flag is used. That means we can do any advanced programming here, you can use `fs` module API to create new files, and directories or to generate any advanced build output.

See also [Build System](modules_build.html#Build-System)

# Variables

## Syntax And Mutability

Now let's talk about the language syntax. In BL, the name comes first followed by a colon separating the name of the symbol from its type. Most type specifications in BL can be left empty if the type is obvious from the value assigned to the symbol. The second colon separates the type from the actual value of the symbol. This may be a bit confusing so let's take a look at the simplest case; the variable.

```bl
number : s32;

```

In this case, we declare a variable named *number* of type *s32* and we do not specify any value of the number (the second colon and value are missing). In this case, the type *s32* is mandatory (there is no value to guess the type from). Also, one very important thing is that if we do not specify the value of the variable explicitly, it's implicitly set to the default value (0 in this case). *Note: This is not a case in C for example where the variable stays uninitialized.*

```bl
number : s32 : 10;

```

Now our variable *number* is explicitly initialized to 10 using the literal value. All numeric literals are by default of *s32* type so we can make the same declaration like this:

```bl
number :: 10;

```

One important note about the second colon is that it affects the variable mutablitly. Variables can be declared as *mutable* the variable that can be changed at any time, or *immutable* we must initialize the variable with some value, and the value cannot be changed later. When the colon is used, we say the variable is *immutable* or *constant*.

```bl
number :: 10;
number = 20; // This is an error. Variable is immutable.

```

```text
hello.bl:3:12: error(0036): Cannot assign to immutable constant.
   2 |  number :: 10;
>  3 |  number = 20;
     |            ^
   4 |  return 0;

error: Compilation of target 'hello' failed.
```


To declare the variable as *mutable* we have to do the following:

```bl
number := 10; // '=' means the variable is mutable.
number = 20;  // This is OK now.

```

And in case there is no value specified, the variable is every time considered to be mutable.

```bl
number: s32; // Default value 0.
number = 20; // This is also OK.

```

## Mutability Of Structure Members

Mutability introduced to variables of *struct* type is preserved also to all members unless a dereference is introduced explicitly or implicitly. That means the direct member modification is not allowed for immutable variables of *struct* type, however, if accessed via a pointer, the value can be changed.

```bl
Person :: struct {
    name: string_view;
    age: s32;
}

main :: fn () s32 {
    person :: Person.{};
    person.age = 10; // ERROR

    person_ptr :: &person;
    person_ptr.name = "foo"; // OK, access via pointer.

    person_ptr = null; // ERROR, the pointer itself is immutable.
    return 0;
}
```

## Initialization

As already mentioned, all variables can be initialized to some value in the declaration. In case the initialization value is not specified, the variable is implicitly "zero" initialized.

```bl
number: s32;   // Default value 0.
boolean: bool; // This is also false.

```

In case we want to disable the implicit initialization for some reason, we can use `#noinit` directive after the variable type.

```bl
number: s32 #noinit;
boolean: bool #noinit;

```

Both variables are left uninitialized (containing some random data from the stack). Uninitialized variables should not be used before they are set to some meaningful value.

This can be useful in some cases when the default initialization may be expensive; i.e. The initialized variable is a large struct, and we explicitly initialize all its members later.

```bl
Data :: struct {
	a: s32;
	b: bool;
	// ... lot of other members
}

data: Data #noinit; // Disable default initialization.
data.a = 10;
data.b = true;
// Initialize all other members...

```

## Local

Local variables have their lifetime limited to the current block (code usually surrounded by curly braces). They are allocated on the stack frame of an owning function and become invalid when a function returns.

```bl
main :: fn () s32 {
	my_local_integer: s32;

	{
		another_one: s32;
	}
	return 0;
}

```

The first local variable in the previous function is declared in the function block, so it's available in the whole function since it's declared. However, the second variable is declared in the anonymous block, it's available only inside this block.

```bl
main :: fn () s32 {
	my_local_integer: s32;

	{
		my_local_integer = 1; // This is OK.
		another_one: s32;
		another_one = 2; // This is also OK.
	}

	another_one = 2; // Error!
	return 0;
}

```

```bl
hello.bl:10:5: error(0018): Unknown symbol 'another_one'.
    9 |
>  10 |         another_one = 2; // Error!
      |         ^^^^^^^^^^^
   11 |         return 0;

error: Compilation of target 'hello' failed.
```

## Global

Global variables are in general, variables declared outside of any function. Its lifetime is not limited in any way and they are available anywhere in the program. Since there is no required ordering of symbols declared in global scope, the global variable can be used even before its declaration appears in the file. Due to this, BL does not require any header files or any kind of forward declarations.

All global variables must be initialized; either to a default value or explicitly, so we cannot use `#noinit` directive the same way as we did with local variables.

```bl
// Mutable initialized to 0.
number: s32;
// Mutable initialized to 'true'.
boolean := true;

main :: fn () s32 {
    print("number  = %\n", number);
    print("boolean = %\n", boolean);
    print("text    = %\n", text);
    return 0;
}

// Immutable, declared after use.
text :: "Hello";
```

The lifetime of global variables can be explicitly limited to the thread by `#thread_local` directive added after the variable declaration. The thread local variable is later instantiated for each thread separately, so it's safe to use them without any locks. But keep in mind that each instance points to a different memory location (data are not shared between threads).

```bl
@@@examples/thread_local_variable.bl
```

## Usage Checks

The compiler will check if all declared variables are used and produce warnings eventually. However, variables declared in *non-private* global scope may be part of an API and are not checked for usage.

```bl
name := "Martin";

main :: fn () s32 {
    age :: 30;
    return 0;
}

#scope_private
nationality := "CZ";
```

```text
test2.bl:9:1: warning: Unused symbol 'nationality'. Mark the symbol as '#maybe_unused'
                       if it's intentional.
   8 | #scope_private
>  9 | nationality := "CZ";
     | ^^^^^^^^^^^
  10 |

test2.bl:4:5: warning: Unused symbol 'age'. Use blank identificator '_' if it's
                       intentional, or mark the symbol as '#maybe_unused'. If it's
                       used only in some conditional or generated code.
   3 | main :: fn () s32 {
>  4 |     age :: 30;
     |     ^^^
   5 |     return 0;
```

As you can see, the compiler gives you some possible options on how to disable these warnings.

```bl
name := "Martin";

main :: fn () s32 {
    _ :: 30; // Blank identificator.
    return 0;
}

#scope_private
nationality := "CZ" #maybe_unused;
```

## Shadowing

It's possible to declare a new variable with the same name in nested local scopes, the previous variable is *shadowed* by the new one. In general, it's not a good idea; however, there are currently no restrictions or limitations. This may change in the future.

```bl
#import "std/print"

name := "Martin";

main :: fn () s32 {
    name := "Travis";
    {
        name := "George";
        print("name = %\n", name);
    }
    print("name = %\n", name);
    return 0;
}
```


## Comptime Variables

Compile-time known variable is any immutable variable with a value known in compile-time. In some cases, it's required to do compile-time evaluations of the value. For example, the array type definition requires the element count to be known in compile time.

```bl
main :: fn () s32 {
    N := 10;
    array: [N]s32;
    return 0;
}
```

```text
test2.bl:3:13: error(0052): Array size must be compile-time constant.
   2 |     N := 10;
>  3 |     array: [N]s32;
     |             ^
   4 |     return 0;
```

To fix this, it's enough to make the N variable immutable:

```bl
main :: fn () s32 {
    N :: 10; // Changed from = to :.
    array: [N]s32;
    return 0;
}
```

The `N` variable is initialized with compile-time known integer literal, however, if the initialization value is not known in compile-time, we'll get the same error again even if `N` is immutable.

```bl
foo := 10;

main :: fn () s32 {
    N :: foo;
    array: [N]s32;
    return 0;
}
```

```text
test2.bl:5:13: error(0052): Array size must be compile-time constant.
   4 |     N :: foo;
>  5 |     array: [N]s32;
     |             ^
   6 |     return 0;
```

# Types

## Fundamental Types

| Name        | Description                   |
|-------------|-------------------------------|
| s8          | Signed 8-bit number.          |
| s16         | Signed 16-bit number.         |
| s32         | Signed 32-bit number.         |
| s64         | Signed 64-bit number.         |
| u8          | Unsigned 8-bit number.        |
| u16         | Unsigned 16-bit number.       |
| u32         | Unsigned 32-bit number.       |
| u64         | Unsigned 64-bit number.       |
| usize       | Unsigned 64-bit size.         |
| bool        | Boolean. (true/false)         |
| f32         | 32-bit floating point number. |
| f64         | 64-bit floating point number. |
| type | Type of any other type.                 |

## Pointer Type

Represents the address of some allocated data.

```bl
#import "std/test"

pointers :: fn () #test {
    i := 666;
    i_ptr : *s32 = &i; // taking the address of 'i' variable and set 'i_ptr'
    j := @i_ptr;       // pointer dereferencing

    test_true(j == i);
};
```

## Array Type

The array is an aggregate type of multiple values of the same type. Size value must be known in compile-time (unless it's not explicitly marked for automatic element count detection based on content of compound expression). Arrays can be inline initialized with compound block; type is required. Zero initializers can be used for zero initializations of whole array storage, otherwise, we must specify a value for every element in an array.

```bl
array_type :: fn () #test {
    arr1 : [10] s32; // declare zero initialized array variable
    arr1[0] = 666;

    arr1.len; // yields array element count (s64)
    arr1.ptr; // yields pointer to first element '&arr[0]'

    // inline initialization of array type
    arr2 := [10].s32{};              // Initialize all elements to 0.
    arr3 := [4]s32.{ 1, 2, 3, 4 };   // Initialize array to the sequence 1, 2, 3, 4
    arr4 := [_]s32.{ 1, 2, 3, 4 ];   // The same as previous one but the legth of the array is set automatically.
	arr5 : [4]s32 : .{ 1, 2, 3, 4 }; // With use of anonymous compound initializer.
};
```

Arrays can be implicitly converted to slices:

```bl
array_to_slice :: fn () #test {
    arr : [10] s32;
    slice : []s32 = arr;
};
```

## String Type

String type in Biscuit aka `string_view` is a slice containing a pointer to string data and string length. String literals are always zero-terminated. The `string_view` represents a string of fixed length. In case you want a dynamically allocated string use `string` type and its associated methods. Values of `string` can be implicitly converted to `string_view`.

```bl
string_type :: fn () #test {
    msg : string_view = "Hello world\n";
    msg.len; // character count of the string
    msg.ptr; // pointer to the string content
};
```

## Slice

The array slice consists of the array length and pointer to the first array element.

Slice layout:

```bl
Slice :: struct {
    len: s64;
    ptr: *T
};
```

```bl
array_slice :: fn () #test {
    arr :: [4]s32.{1, 2, 3, 4};
    slice : []s32 = arr;
    loop i := 0; i < slice.len; i += 1 {
        print("%\n", slice[i]);
    }
};
```

!!! note
    `alloc_slice` can be used to allocate a slice on the heap.

## Struct Type

The structure is a composite type representing a group of data as a single type. The structure is as an array another way to define a user data type, but types of structure members could be different. It can be used in situations when it's better to group data into one unit instead of interacting with separate units.

A structure can be declared with the use of *struct* keyword.

```bl
Person :: struct {
    id: s32;
    name: string_view;
    age: s32;
}
```

Structure Person in the example consists of id, name and age. Now we can create a variable of this type and fill it with data. To access a person's member fields use `.` operator.

```bl
main :: fn () s32 {
    my_person: Person; // Create instance of type Person
    my_person.id = 1;
    my_person.age = 20;
    my_person.name = "Martin";

    return 0;
}
```

Inline initialization is also possible. We can use a compound expression to set all members at once.

```bl
main :: fn () s32 {
    // Set all data in person to 0
    my_person1 := Person.{};

    // Initialize all members.
    my_person2 := Person.{ 1, "Martin", 20 };

    // We can explicitly name the members we want to initialize.
    my_person3 := Person.{ id = 1, name = "Martin", age = 20 };

    // We can change the order.
    my_person4 := Person.{ name = "Martin", age = 20, id = 1 };

    // Or initialize only something. In such a case the rest is initialized to 0 by default.
    my_person5 := Person.{ name = "Martin" };

    // Untyped compound initializer can be used too.
    my_persor6 : Person = .{ name = "Martin" };

    return 0;
}
```

Structure content can be printed by print function.

```bl
main :: fn () s32 {
    my_person := Person.{ 1, "Martin", 20 };
    print("%\n", my_person);

    return 0;
}
```

```bl
Person {id = 1, name = Martin, age = 20}
```

Due to lacking OOP support, we cannot declare member functions in structures and there is no class or object concept in the language. A common way to manipulate data is by passing them into the function as an argument.


```bl
person_add_age :: fn (person: *Person, add: s32) {
    // Age can be modified even if the 'person' argument is immutable.
    person.age += add;
}
```

### Implicit Composition

A structure can extend any type with the use of `#base <T>`. This is a kind of inheritance (similar to the C style) where inheritance can be simulated by composition. The `#base <T>` inserts `base: T`; as the first member into the structure. The compiler can use this information later to provide more inheritance-related features like merging of scopes to enable direct access to base-type members via `.` operator or implicit cast from a child to a parent pointer type.

**Example of structure extension:**

```bl
Entity :: struct {
    id: s32
}

// Player has base type Entity
Player :: struct #base Entity {
    // base: Entity; is implicitly inserted as first member
    name: string_view;
};

Wall :: struct #base Entity {
    height: s32
};

Enemy :: struct #base Entity {
    health: s32
};

// Multi-level extension Boss -> Enemy -> Entity
Boss :: struct #base Enemy {
    // Extended struct can be empty.
};

struct_extending :: fn () #test {
    p: Player;
    p.id = 10; // direct access to base-type members
    p.name = "Travis";
    assert(p.base.id == 10); // access via .base

    w: Wall;
    w.id = 11;
    w.height = 666;

    e: Enemy;
    e.id = 12;
    e.health = 100;

    b: Boss;
    b.id = 13;

    // implicit down cast to entity
    update(&p);
    update(&w);
    update(&e);
    update(&b);
}

update :: fn (e: *Entity) {
    print("id = %\n", e.id);
}
```

### Member Tagging

Compile-time `u64` tag can be assigned to struct members. The tag value is later accessible via Type Info API in `TypeInfoStructMember.tag`.

```bl
@@@examples/struct_member_tags.bl

```



## Union Type

The union is a special composite type representing the value of multiple types. Union size is always equal to the size of the biggest member type and the memory offset of all members is the same. Union is usually associated with some *enum* providing information about the stored type.

```bl
Token :: union {
    as_string: string_view;
    as_int: s32;
}

Kind :: enum {
    String;
    Int;
}

test_union :: fn () #test {
    token1: Token;
    token2: Token;

    // Token has total size of the biggest member.
    assert(sizeof(token1 == sizeof(string_view));

    token1.as_string = "This is string";
    consumer(&token, Kind.String);

    token2.as_int = 666;
    consumer(&token, Kind.Int);
}

consumer :: fn (token: *Token, kind: TokenKind) {
    switch kind {
        Kind.String { print("%\n", token.as_string); }
        Kind.Int    { print("%\n", token.as_int); }
        default { panic(); }
    }
}
```

## Any Type

Any type is a special builtin structure containing the pointer to TypeInfo and to the data. Any value can be implicitly casted to this type while passed into a function.

```bl
Any :: struct #compiler {
    type_info: *TypeInfo;
    data: *u8
};
```

The *Any* value should never own the original data!

Implicit conversion to *Any* type may cause stack allocation of a temporary variable on the call side in case the original value does not represent stack or heap-allocated memory. This may cause a *hidden* overhead in some cases.

```bl
...
foo(10); // temp for '10' is created here
...

foo :: fn (v: Any) {}
```

For types converted to *Any* compiler implicitly sets `type_info` field to a pointer to the *TypeType* type-info and the data field to the pointer to the actual type-info of the converted type.

```bl
...
foo(s32); // Type passed
...

foo :: fn (v: Any) {
    assert(v.type_info.kind == TypeKind.TYPE);

    data_info := cast(*TypeInfo) v.data;
    assert(data_info.kind == TypeKind.INT);
}
```

Any can be combined with *vargs*; a good example of this use case is a *print* function where *args* argument type is *vargs* of *Any* (\... is the same as \...Any). The *print* function can take values of any type passed as *args*.

```bl
print :: fn (format: string_view, args: ...) {
    ...
};
```

## Enum Type

The _enum_ represents an integer type with a limited set of possible named values. The underlying integer type can be explicitly specified after *enum* keyword, otherwise `s32` is used implicitly. Each possible variant lives in the *enum* namespace.

```bl
@@@examples/enums.bl
```

### Nested Enum Type

An enumerator can be nested in any _struct_ in case we want to explicitly associate the enumeration with some data type.

```bl
@@@examples/enum_nested.bl
```

## Enum Flags Type

An enumerator can be used as a definition of bit flags by adding #flags directive to the type definition. This directive slightly changes the way how the enumerator values are generated. By default, the enumerator starts with zero variant (if it's not explicitly changed by the programmer) and every following enumerator variant has a value set to the previous one plus one. The flags enumerator starts with the first variant set to 1 and the following variants are set to the left-bit-shifted value of the previous one.

Enumerators marked as flags are also serialized as a combination of atomic flags instead of just one value.

```bl
@@@examples/enum_flags.bl
```

So in comparison to conventional enumerators, bunch of binary operations are allowed for flags enumerators to easily manipulate (set, clear and check) its values. Namely:

```
&      Intersection.
|      Union.
~      Bit flip.
&=     Assign intersection.
|=     Assign union.
```

!!! note
    Since flags enumerators start implicitly with value 1, you can explicitly define `NoFlag
    = 0;` variant at the beginning of the variant list.

!!! note
     Flags enumerators should use an unsigned number type as a base type (`u32` by default).

!!! note
     It's possible to do an implicit casting of flags enumerators to its base type.


## Type Casting

Change the type of value to the other type. Conversions between integer types, from pointer to `bool` and from array to slice are generated implicitly by the compiler.

```bl
main :: fn () s32 {
    // Default type of integer literal is 's32'.
    i := 666;

    // Type of the integer literal is changed to u64.
    j : u16 = 666;

    // Implicit cast on function call.
    fn (num: u64) {
    } (j);

    // Explicit cast of 'f32' type to 's32'.
    l := 1.5f;
    m := cast(s32) l;
    return 0;
};
```

Type casting rules in BL are more strict compared to C or C++, there are no void pointers or implicit conversions between integers and enums etc. Despite this fact, an explicit cast can be in some cases replaced by *auto* cast. The *auto* cast operator does not need explicit destination type notation, it will automatically detect destination type based on expression if possible. When an *auto* operator cannot detect the type, it will keep an expression's type untouched. In such a case auto does not generate any instructions into the final binary.

```bl
main :: fn () s32 {
    s32_ptr : *s32;
    u32_ptr : *u32;

    // auto cast from *u32 to *s32
    s32_ptr = auto u32_ptr;

    // keep expession type s32
    i := auto 10;
    return 0;
};
```

## Type Decomposition

Type decomposition can be used on the composite types to get a type of any of the nested members.

```bl
Person :: struct {
    name: string_view;
    age: s32;
}

main :: fn () s32 {
    name: Person.name; // string_view type
    age: Person.age; // s32 type
    return 0;
}

```

This can be extremely useful when generic structures are used in polymorphic functions and we don't know internal member types in advance.

```bl
MyContainer :: fn (TValue: type) type #comptime {
    return struct {
        value: TValue;
    };
}

// Return type is type of TContainer member value.
get_value :: fn (container: *?TContainer) *TContainer.value {
    return &container.value;
}

main :: fn () s32 {
    container: MyContainer(u64);
    value :: get_value(&container);
    return 0;
}
```

Pointer type dereference is also possible.

```bl
Person :: struct {
    name: string_view;
    age: s32;
    parent: *Person;
}

main :: fn () s32 {
    parent_by_value: @Person.parent; // Person type.
    return 0;
}
```


# Functions

A function is a chunk of code representing a specific piece of the program functionality introduced by *fn* keyword. A function can be called using the call operator `()`, we can provide any number of arguments into the function and get *return* value back on a call side.

A function is usually associated with a name and can be later called by this name followed by the call operator. To keep the syntax consistent, the function declaration looks similar to a variable declaration. The philosophy under the hood is the same, we associate some symbol (name) with some value (in this case a function literal). However, a function declaration is required to be always immutable.

Functions can be declared in the global or a local scope (one function can be nested in another one).

```bl
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

```bl
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

```bl
math :: #import "std/math";

compare :: fn (a: f32, b: f32, epsilon: f32 = 0.1f) bool {
    return math.abs(a - b) < epsilon;
}

main :: fn () s32 {
    compare(13.f, 12.f, 0.001f); // We don't use the default value.
    compare(13.f, 12.f); // We use default value.

    return 0;
}
```

The explicit `f32` type is optional for the `epsilon` with default value since we have the value to get the type from, so the following code is also valid.

```bl
compare :: fn (a: f32, b: f32, epsilon := 0.1f) bool {
    return abs(a - b) < epsilon;
}
```

One limitation here is that the arguments with default values must go very last in the argument list. Currently, there is no way how to specify namely which argument we want to call on the call side.

```bl
compare :: fn (epsilon := 0.1f, a: f32, b: f32) bool {
    return abs(a - b) < epsilon;
}
```

```text
test2.bl:1:16: error(0035): All arguments with default value must be listed last in the function
                            argument list. Before arguments without default value.
>  1 | compare :: fn (epsilon := 0.1f, a: f32, b: f32) bool {
     |                ^^^^^^^
   2 |     return abs(a - b) < epsilon;
```

### Call Location

One special feature very useful for debugging is `#call_location` which can be used as a default argument value. Each time the function is called, the `#call_location` is replaced by a pointer to the `CodeLocation` variable, containing information about where the function was called in the source code.

```bl
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

```bl
load_data :: fn (BUFFER_SIZE: s32 #comptime) {
    // 'SIZE' is compile-time known constant here.
    buffer: [BUFFER_SIZE]s32;
    // ...
}
```

Since the `BUFFER_SIZE` is compile-time known constant, it can be used as size in an array type definition. This obviously means the `BUFFER_SIZE` argument needs to be compile-time constant when the function is called, otherwise the compiler generates an error.

Internally, `load_data` function does not exist until the compiler hits a call to this function; we don't know what the value of `BUFFER_SIZE` is in advance. The compiler will generate a unique implementation with `BUFFER_SIZE` argument removed from the argument list and converted into compile-time known constant value for each compiled call to this function in the code. At this point, you may see some possible disadvantages. Since the compile-time argument is removed from the argument list, the *mixed* function cannot follow *C call conventions* and cannot be *exported* or *external*. Also, instantiating a new function implementation for each call in the code can lead to a bigger executable and slow down the compiler.

One important thing, we can do with *mixed* functions is having also types as input arguments. See the implementation of `new` function from the *standard library*:

```bl
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

**Pros**

- We can pass types into functions.
- We can reference the compile-time arguments in the argument list (i.e. use them as return type).

**Cons**

- Can produce larger binary and slow down the compilation process.
- Compile-time known argument cannot be used as a default value of another argument.
- Type-checking is very limited since we don't know `comptime` arguments in advance; the generated implementation is type-checked each time the function is called.
- Mixed functions do not represent any stack-allocated memory (we cannot get its address).
- Don't follow C calling conventions:
    - Cannot be `extern`.
    - Cannot be `export`.

### Variable Argument Count

In BL, we can have a function taking 0-N values in the argument list, let's start with an example:

```bl
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

```bl
print :: fn (format: string_view, args: ...) s32 {
    // ...
}
```
**Pros**

- We can pass any number of arguments we want.
- Safe and easy to use.

**Cons**

- Some overhead may be introduced by implicit conversion to an array.
- Must be the last in the argument list.
- Don't follow C calling conventions:
    - Cannot be `extern`.
    - Cannot be `export`.

## Return Value

Each function can eventually return some value using the *return* statement. The *return* statement returns the execution back to the caller, so the execution of the function ends in case the *return* is reached. The return value is optional and can be specified in the function declaration *header* after the argument list. There is no *void* type (like in C or C++) to say the function does not return, we simply leave the return value type empty. The following example shows the function without the return value.

```bl
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

```bl
add :: fn (a: s32, b: s32) s32 {
    return a + b; // Return is mandatory.
}

main :: fn () s32 {
    result := add(1, 2);
    return 0;
}
```

A function may return multiple values at once like this:

```bl
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

```bl
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

```bl
@@@examples/open_file.bl
```

The `open_file` function in this case returns the file stream and possible error in case the file cannot be opened.

## Anonymous Functions And Callbacks

Sometimes a function may be used only once as an *callback* function passed into some other function. In such a case we can simplify the declaration and keep the function unnamed. One good example is the `sort` function declared like this:

```bl
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

```bl
#import "std/print"

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

```bl
#import "std/print"

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

```bl
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

- Argument count.
- Argument types.
- Type casting.
- Conversion to slice.
- Conversion to any.

!!! note
     The return type does not affect choosing the best call candidate.

**Resolving the best call candidate is done in two passes:**

- Pick all possible candidates based on call-side argument count when:
    - The argument count is exactly matching the count of arguments required by the function interface.
    - All arguments, up to the first defaulted or variable count argument in the function interface, are provided.

- Iterate over previously picked functions and rank them by comparing call-side arguments with each function's interface arguments one by one:
    - The type is exactly the same. (Rank +3)
    - Can be implicitly casted. (Rank +2)
    - Can be implicitly converted. (Rank +2)
    - Can be implicitly converted to `Any`. (Rank +1)
    - Can be added to the variable count argument array. (Rank +1)

- Use the function with the highest rank.

```bl
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

```bl
add_s32 :: fn (a: s32, b: s32) s32 {
    return a + b;
}

// Same for floats.
add_f32 :: fn (a: f32, b: f32) f32 {
    return a + b;
}
```

In this case, we can use polymorph instead:

```bl
add :: fn (a: ?T, b: T) T {
    return a + b;
}
```

Value of `T` represents any type, in this case, chosen based on usage of the function. The question
mark before `T` says the first `T` is the master polymorph type. The compiler tries to replace all
master types with the type of argument on the call side and register the new type alias `T` in the
function scope.

Example of usage:

```bl
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

```bl
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

```bl
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

```bl
table_insert :: fn (table: *Table, key: ?TKey, value: ?TValue) {
    // ...
}
```

### Specify Implementation For Type

In some cases we want to specify explicitly what implementation should be used for some specific type, i.e. in a function doing a comparison of two values, we can provide specific handling for string:

```bl
#import "std/string"

is_equal :: fn { // function group
    // Implementation used for strings only.
    fn (a: string_view, b: string_view) bool {
        return str_compare(a, b);
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

```bl
get_age :: fn () s32 #inline {}
```

### inline/noinline

Tells the compiler whether it should try to inline the called function. Inlining may not be possible in some cases, however in general it can improve the runtime speed. Inline functions should not be too complex.

### extern

An extern function is a function implemented in a foreign library linked to the program. Such a function defines only an interface but cannot be implemented (does not have a body). The `#extern` directive can be optionally followed by the linkage name of the external symbol. If the linkage name is not specified, the function name is used instead. Having external functions allows the use of any existing C ABI compatible library.

The extern functions must strictly follow *C call conventions*.

```bl
my_malloc :: fn (size: size_t) void_ptr #extern "malloc";
free :: fn (ptr: void_ptr) #extern;
```

### export

Functions with an `export` directive are exported from the binary when a program is compiled as a shared library (with `-shared` flag). So the function may be called from the other libraries or executables after successful linking. The `#export` directive can be optionally followed by the linkage name of the exported symbol. If the linkage name is not specified, the function name is used instead.

The export functions must strictly follow *C call conventions*. That means, the function cannot be polymorphic (generated in compile time).

```bl
my_add :: fn (a: s32, b: s32) s32 #export "add" {
    return a + b;
}
```

### comptime

Every call to such a function is going to be evaluated in compile-time and replaced by constant eventually.

```bl
#import "std/string"

hash_string :: fn (s: string_view) u32 #comptime {
    return str_hash(s);
}
```

The `hash` function can be later called the same way as any other regular function, but the call itself is replaced by the constant result value in the final binary.

```bl
#import "std/print"

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

```bl
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


# Comments

Simple documentation can be written directly into the code the same way as in other programming languages, simply by adding comments. The BL comments use the same syntax as in C. You can write a single-line comment or multi-line comment as needed. You can also write documentation directly into the code and let the compiler generate *markdown* files for you.

## Single-line Comments
You can use single-line comments introduced by `//` anywhere in the code. Everything up to the end of the line is considered a comment.

```bl
// This is the main function.
main :: fn () s32 {
	return 0; // We return 0 here.
}
```

## Multi-line Comments
Multi-line comments are also supported:

```bl
/*
	The main function.

	This is the entry point of our program.
*/
main :: fn () s32 /* the signed number is returned from the main */ {
	return 0;
}
```

## Documentation Comments

Since the BL compiler supports documentation generation out of the box, you can use two types of comments which are supposed to be a part of a generated document.

- `//!` Documentation of the file.
- `///` Documentation of the following symbol.

See also [Documentation](manual.html#Documentation).

# Literals

## Simple Literals

```bl
b :: true;         // bool true literal
b :: false;        // bool false literal
ptr : *s32 = null; // *s32 null pointer literal
```

## Integer Literals

Biscuit language provides constant integer literals written in various formats showed in the example bellow. Integer literals have a volatile type, when the desired type is not specified compiler will choose the best type to hold the value. Numbers requiring less space than 32 bits will be implicitly set to *s32*, numbers requiring more space than 31 bits and less space than 64 bits will be set to *s64* and numbers requiring 64 bits will be set to *u64* type. Bigger numbers are not supported and the compiler will complain. When we specify type explicitly (ex.: `foo : u8 : 10;`), an integer literal will inherit the type of the variable (same for function calls where the type is known).

```bl
i     :: 10;      // s32 literal
i_u8  : u8 : 10;  // u8 literal
i_hex :: 0x10;    // s32 literal
i_bin :: 0b1011;  // s32 literal
f     :: 13.43f;  // f32 literal
d     :: 13.43;   // f64 literal
char  :: 'i';     // u8 character literal
```

## String Literals

The string literal is represented as a constant _utf8_ array of bytes stored in the builtin value of *string_view* type.  The content of the array is allocated on the *stack* and cannot be changed in runtime.

```bl
the_name :: "Martin Dorazil";
```

A longer string value may be split into multiple lines, the result value is evaluated as a single string.

```bl
text ::
    "This "
    "is "
    "long "
    "text on multiple "
    "lines.";
```

Special characters might be encoded into string with use of escape sequence introduced by backslash character:

```bl
"\n" // New line.
"\r" // Carriage return.
"\t" // Tab.
"\"" // Double quote.
"\'" // Single quote.
"\\" // Backslash.

// ASCII code in octal format (up to 0377).
"\033" // Escape character.

// ASCII code in hex format (up to 0xFF).
"\x1B" // Escape character.
```

# Operators

## Binary Operators

```
+      Addition.
-      Subtraction.
*      Multiplication.
/      Division.
%      Remainder division.
+=     Addition and assign.
-=     Subtraction and assign.
*=     Multiplication and assign.
/=     Division and assign.
%=     Remainder division and assign.
<      Less.
>      Greater.
<=     Less or equals.
>=     Greater or equals.
==     Equals.
&&     Logical AND.
||     Logical OR.
&      Bit AND.
|      Bit OR.
^      Bit XOR.
&=     Bit AND and assign.
|=     Bit OR and assign.
^=     Bit XOR and assign.
<<     Bitshift left.
>>     Bitshift right.
```

## Unary Operators

```
\+     Positive value.
\-     Negative value.
@      Pointer dereference.
&      Address of.
!      Logical not.
~      Bit flip.
```


# Flow Control

In this chapter, we'll focus on language features focused on conditional code execution and repetitive execution of some code blocks. The BL implements all well-known concepts of flow control and a little bit more. The most common way to let you execute a part of a code only in case some runtime condition is *true* is the *if* statement.

## If Else

The *if-else*, as in many other languages, splits the code into two branches and executes one or the other based on some runtime condition.  The first branch is executed only if the condition is *true*. The expression after *if* statement is required to be a *bool* or at least convertible to *bool* type. The implicit conversion is applied only to the pointer values where the *null* pointer converts to *false* implicitly.

```bl
#import "std/print"

main :: fn () s32 {
    do_it := true;

    if do_it {
        print("It's done!\n");
    }
}

```

Notice that we don't need additional brackets around the condition like in C-like languages.  We can optionally introduce the *else* block executed if the *if* condition is *false*.

```bl
#import "std/print"

main :: fn () s32 {
    do_it := false;

    if do_it {
        print("It's done!\n");
    } else {
        print("It's not done!\n");
    }

    return 0;
}

```

In some cases we might need only simple single line piece of code executed conditionally. To make it possible, since brackets around the if statement expression are not required, we've introduced `then` keyword. All following examples are valid.

```bl
#import "std/print"

main :: fn () s32 {
	expr := true;

	if expr { print("This is true branch!\n"); }
    if !expr { print("This is true branch!\n"); } else { print("This is false branch!\n"); }

    // Then is optional here.
    if expr then { print("This is true branch!\n"); }
    if !expr then { print("This is true branch!\n"); } else { print("This is false branch!\n"); }

    // Then is required here. We can have only one statement/expression for each branch.
    if expr then print("This is true branch!\n");
    if !expr then print("This is true branch!\n") else print("This is false branch!\n");

    return 0;
}
```

We can also create the whole conditional chain:

```bl
#import "std/print"

Color :: enum {
    RED;
    GREEN;
    BLUE;
}

main :: fn () s32 {
    color := Color.GREEN;

    if color == Color.RED {
        print("It's red!\n");
    } else if color == Color.GREEN {
        print("It's green!\n");
    } else if color == Color.BLUE {
        print("It's blue!\n");
    } else {
        print("It's something else!\n");
    }

    return 0;
}
```

However, in such a situation it's better to use the *switch* statement.

### Ternary If Operator

Multiline conditional can be in some cases condensed to a single line expression. We can use ternary `if` expression for this. Unlike in C languge, there is no special operator for such expression in BL. We can just use the same syntax we already have using `if then` pattern. In case the `if` statement is used as an expression, the `else` branch is mandatory. We also cannot use block branches.

```bl
main :: fn () s32 {
    value := 50;

	number: s32;
    // Conditional using standard if statement.
    if value > 0 {
        number = 10;
    } else {
        number = 20;
    }

    // Conditional using inline if statement.
    if value > 0 then number = 10 else number = 20;

    // Direct conditional initialization using ternary if expression.
    number = if value > 0 then 10 else 20;

    return 0;
}
```

## Switch

A *switch* can compare one numeric value against multiple values and switch execution flow to matching case. The `default` case can be used for all other values we don't explicitly handle. While the expression after *switch* keyword is supposed to be a runtime value, the *case* values must be known in compile-time. Currently, the *switch* can be used only with *integer* types.

```bl
#import "std/print"

Color :: enum {
    RED;
    GREEN;
    BLUE;
}

main :: fn () s32 {
    color := Color.BLUE;
    switch color {
        Color.RED   { print("It's red!\n");   }
        Color.GREEN { print("It's green!\n"); }
        Color.BLUE  { print("It's blue!\n");  }
    }

    return 0;
}
```

It's also possible to reuse one execution block for multiple cases, just list all case values separated by comma followed by the execution block.

```bl
switch color {
    Color.RED,
    Color.GREEN { print("It's red or green!\n"); }
    Color.BLUE  { print("It's  blue!\n");  }
}
```

In case the switch is used with *enum* values, the compiler will automatically check for you if all possible cases are covered.

```bl
switch color {
    Color.GREEN { print("It's red or green!\n"); }
    Color.BLUE  { print("It's  blue!\n");  }
}
```

```text
test2.bl:9:5: warning: Switch does not handle all possible enumerator values.
   8 |     color := Color.BLUE;
>  9 |     switch color {
     |     ^^^^^^
  10 |         Color.GREEN { print("It's red or green!\n"); }

Missing case for: RED
```

If you don't want to handle all cases explicitly, you can introduce a *default* case:

```bl
switch color {
    Color.GREEN { print("It's red or green!\n"); }
    Color.BLUE  { print("It's  blue!\n");  }
    default { print("It's some other color."); }
}
```

In the previous example we print a message for the default case, but we can use just an empty statement here:

```bl
switch color {
    Color.GREEN { print("It's red or green!\n"); }
    Color.BLUE  { print("It's  blue!\n");  }
    default;
}
```

## Loop

Another common way how to manage program control flow is looping. This is a well-known concept available in a lot of languages. We simply run some part of code N-times where N is based on some condition. In BL there is only a single *loop* keyword for loops followed by an optional condition. We can use *break* and *continue* statements inside loops. The *break* statement will simply interrupt looping and skip out of the *loop* body. The *continue* statement will immediately jump to the next loop iteration.

```bl
#import "std/print"

main :: fn () s32 {
    count :: 10;
    i := 0;

    // The loop without conditions will run infinitely until return or break is hit.
    loop {
        i += 1;
        if i == count {
            // Jump out of the loop.
            break;
        }
    }

    // Iterate until the 'i' is less than 'count'.
    i = 0;
    loop i < count {
        i += 1;
    }

    // Iterate until the 'j' is less than 'count'. This is the same concept
    // as the previous one, except we declare the iterator directly in the
    // loop.
    loop j := 0; j < count; j += 1 {
        // do something amazing here
    }

    loop j := 0; j < count; j += 1 {
        // We can use 'continue' to skip printing when j is 2.
        if j == 2 { continue; }
        print("j = %\n", j);
    }

    return 0;
}
```


## Defer

One, not so common concept is a *defer* statement. The *defer* statement can be used for the "deferring" execution of some expressions. All deferred expressions will be executed at the end of the current scope in **reverse** order. This is usually useful for some cleanup (i.e. closing a file or freeing memory). When the scope is terminated by return all previous defers up the scope tree will be called after evaluation of return value. See the following example:

```bl
#import "std/print"

main :: fn () s32 {
    defer print("1\n");

    {
        // Lexical scope introduced.
        defer print("2 ");
        defer print("3 ");
        defer print("4 ");
    } // defer 4, 3, 2

    other_function();

    defer print("5 ");
    return 0;
} // defer 5, 1

other_function :: fn () s32 {
    defer print("6 ");
    defer print("7 ");

    if true {
        defer print("8 ");
        return 1;
    } // defer 8, 7, 6

    // This part is newer reached, compiler will complain.
    defer print("9 ");
    return 0;
}
```

The output:

```text
4 3 2 8 7 6 5 1
```

Another good example is the following:

```bl
@@@examples/open_file.bl
```

Where the *defer* is used to `close_file` and `str_delete`; we can nicely couple the resource *allocation* and *deallocation* together and also handle possible errors in more elegant way. In this case, the `close_file` will be called in case of any error, while opening a file, after `return 1;` or after the last `return 0;` automatically.


# Code Structure

The top-level building stone of each BL program is called assembly. Each assembly can be compiled into executable file or library.

A BL code itself is organized into scopes. Each scope groups entities inside, and can limit their visibility from the outside. Scopes can be used also to avoid naming collisions. They are organized into a tree structure, where one scope may contain a bunch of other scopes. In this chapter, we describe all possible scope kinds in detail.

## Global Scope

The global scope is implicitly created root scope of the assembly containing builtin symbols provided by the compiler out of the box. The user code is organized into modules, where implicitly created `__assembly_module` acts like an entry point of the program, and contains the `main` entry function. Other modules can be imported into your program by `#import` directive. The `#load` directive can be used to insert individual source files.

## Module Scope

The module scope is the root scope of each module (for example `std/print`, `std/string`, ... ), where each module provides some specific piece od functionality. Symbols from one module are not accessible from the other unless we use `#import` directive. When module A is imported into module B, all symbols in the module scope of A are visible to B, but not the other way around.

As mentioned in [Global Scope](manual.html#global-scope) section, even the entry point of the program is encapsulated into implicit module, however, for simplicity, we usually refer to this implicit module scope as the global scope.

### Load

```bl
#load "<path/to/your/file.bl>"
```

The easiest way to add code from other source files into your program is by using the `#load` directive. You can think about it as if it was a simple code insertion. All code from loaded files is inserted into the scope where each load is called. The target scope of the load process is important, the file is not loaded again in case it's already present the target scope, and code from loaded source file A is visible to loaded source file B (and vice-versa) in case both files are loaded in the same scope.

```bl
// Load two files into current scope.
#load "A.bl" // A can use symbols from B.
#load "B.bl" // B can use symbols from A.

#load "A.bl" // Does nothing, A is already present in this scope.

main :: fn () s32 {
    function_from_a();
    function_from_b();
    return 0;
}
```

Compiler will lookup source files according to the following rules.

**Lookup order:**

- Absolute path.
- Load target file parent directory.
- BL API directory set in `install-location/etc/bl.yaml`.
- System *PATH* environment variable.

### Import

```bl
#import "<path/to/your/module>"
```

The import directive can be used to import symbols from one module to the other. Module importing does not act like an insertion, thus even if you import two modules in the same target scope, code in both of them remains separated. Even if the same module is imported multiple times, it's compiled only once.

In case one module is imported multiple times in the same scope (without explicit namespacing), compiler will produce errors when symbols from such module are used (they are ambiguous).

Internally this process of import is called *scope injection*. The list of symbols from imported module scope is injected into the target scope.

```bl
// We're importing symbols from std/print module into scope where the
// #import directive is called.
#import "std/print"

// All symbols from 'std/print' are now visible in this scope.

main :: fn () s32 {
    print("Hello!\n"); // Call the function from 'std/print' module.
    return 0;
}
```

In some cases we wan't to encapsulate symbols from imported module to prevent collisions with our code, we can do so by wrapping the imported symbols into namespace.

```bl
print :: fn () {} // This is my print implementation.

// The print module also contains print function, so we wrap all symbols
// from the module into namespace 'foo' to prevent name collision.
foo :: #import "std/print";

main :: fn () s32 {
    // Call our print.
    print();

    // Call print from the imported module.
    foo.print("Hello\n");

    return 0;
}
```

More information about how to create modules can be found in [modules](manual.html#Modules) section.

**Lookup order:**

- Absolute path.
- Custom module directory if set in [set_module_dir](modules_build.html#set_module_dir).
- BL API directory set in `install-location/etc/bl.yaml`.
- System *PATH* environment variable.

## Private Scope

The *private* scope may be created by `#scope_private` directive in a source file, everything declared after this directive (in private block) is accessible only inside the current file. The *private* scope can be terminated by the *public* scope directive.

The main purpose of a *private* scope is to hide some internal implementations which should not be accessible from the outside world.

```bl
print_log :: fn (text: string_view) {
	// We can access the private stuff since it's in the same file.
	set_output_color(Color.BLUE);
	defer set_output_color(Color.NORMAL);

	// Use default print to print the message.
	print(text);
}

#scope_private
// All following code is visible only inside the current file.

Color :: enum {
	NORMAL;
	RED;
	BLUE;
}

set_output_color :: fn (color: Color) {
	switch color {
		Color.NORMAL { ... }
		Color.RED { ... }
		Color.BLUE { ... }
	}
}
```

Note that private block can be used to limit symbol visibility from loaded files or imported modules.

```bl
// Symbols from the print module are visible in the current module.
#import "std/print"

#scope_private

// Symbols from string module are visible only in the current file.
#import "std/string"
```

## Public Scope

The public scope `#scope_public` can be introduced in a source file to switch back from current private or module private scope. Functions or variables in public scope can be used from current file or from other files as well. All global declarations in BL are public by default.

```bl
// This function is public by default and can be called
// anywhere in the current module.
say_hello :: fn () {
    num += 1;
    print("Hello\n");
}

// Introduce the private block to limit visibility of following symbols to
// current file.
#scope_private

// This variable is private and visible only inside the current file.
num: s32;

// Switch back to public block.
#scope_public

// This function can be again called anywhere in the current module.
hello_count :: fn () {
    print("Hello count: %\n", num);
}
```

## Module Private Scope

All symbols in a module can be visible to other modules by default when imported. We might use module private scope block to limit the symbol visibility only to the current module. Consider following files to be part of the same module:

**File A**
```bl
// When module is imported this function becomes available
// to the importer.
say_hello :: fn () {
    say("Hello");
}

// Introduce scope private block; it's content is visible only in the current
// module.
#scope_module

// The imported symbols from the print module are available only in
// the current module.
#import "std/print"

// This function can be used only in the current module.
say :: fn (text: string_view) {
    print("%\n", text);
}
```

**File B**
```bl
// This function has the same visibility as 'say_hello'.
say_bye :: fn () {
    // Use module private function from File A.
    say("Bye");
}
```

## Local Scope

Local scopes are created implicitly for each function since the usage of symbols, declared in the function, is strictly limited to be used only inside that function. The compiler creates local scopes also for structures, unions and enums.

## Using

The *using* statement may be added in local scopes to reduce the amount of code you have to write. It makes the content of "used" scope directly available in the scope the *using* is living in. Be careful using this feature, it may introduce symbol ambiguity and the compiler may complain.

Currently, the *using* supports module namespaces and enums, it's not allowed for structures and unions, since it may make the code less readable, however this is still open question, and we might allow it in the future.

```bl
foo :: #import "std/print";

Color :: enum {
    RED;
    GREEN;
    BLUE;
}

main :: fn () s32 {
    foo.print("Hello\n");

    // Content of module namespace foo becomes directly available.
    using foo;
    print("Hello\n");

    set_color(Color.RED);

    // Content of enum scope Color becomes directly available.
    using Color;
    set_color(RED);

    return 0;
}
```

# Modules

A module in BL is a chunk of reusable code bundled with a configuration file in a single module *root* directory. See the following example of the *thread* module.

```text
thread/
  module.yaml       - module config
  _thread.win32.bl  - windows implementation
  _thread.posix.bl  - posix implementation
  thread.bl         - interface
  thread.test.bl    - unit tests
```

Modules can be imported into other modules using the `#import` directive followed by the module root directory path. The configuration file *module.yaml* is mandatory for each module, and must be located in the module *root* directory. In general, the configuration file tells the compiler which source files should be loaded, which libraries are needed and what is the module version. See the module configuration example.

```yaml
version: 20221021
src: "thread.bl"

x86_64-pc-windows-msvc:
  src: "_thread.win32.bl"

x86_64-pc-linux-gnu:
  src: "_thread.posix.bl"

x86_64-apple-darwin:
  src: "_thread.posix.bl"

arm64-apple-darwin:
  src: "_thread.posix.bl"
```

The first in the file is the `version` of the module (usually in the format YYYYMMDD) followed by the list of all compilation targets supported by the module.  In this example, we use a different implementation for Windows. You can get a list of all supported platforms using the `blc --target-supported` command.

To import the `thread` module use the *root* directory path:

```bl
#import "path/to/module/thread"
```

!!! note
    Modules will be redesigned in the future to support the full set of features required by the build system. The long-term plan is to have the module configuration written directly in BL.

## List of module config entries

The configuration file entries may be *global* or *platform-specific*, see the following sections.

### Global Options

The *global* options are applied to the module on all target platforms.

- `version: <N>` - Module version number used during import to distinguish various versions of the same module.
- `supported: "[triple,...]"` - Optional list of target triples supported by module separated by comma. When module depends on some platform specific libraries (which are supposed to be bundled with the module), we might allow this module only on platforms we've provided the precompiled binaries for. Error is generated when module is used on target not included in this list. You can get a list of all supported platforms using the `blc --target-supported` command.

### Global or Platform-Specific Options

All the following options may be applied globally or just for a specific target platform.

- `src: "<FILE1[,FILE2,...]>"` - List of source file paths relative to the module *root* directory separated by comma.
- `linker_opt: "<OPTIONS>"` - Additional runtime linker options.
- `linker_lib_path: "<DIR1,[DIR2,...]>"` - Additional linker lookup directories relative to the module *root* directory.
- `link: "<LIB1[,LIB2,...]>` - Libraries to link. Note that libraries listed here are dynamically loaded during compilation (may be executed in compile-time).

```yaml
# The version is required to be global.
version: 20221021

# Load this file on all platforms.
src: "my_file_imported_everytime.bl"

# Section specific for Windows.
x86_64-pc-windows-msvc:
  src: "my_file_only_for_windows.bl"

# Section specific for Linux.
x86_64-pc-linux-gnu:
  src: "my_file_only_for_linux.bl"
  linker_opt: "-lc -lm" # link these only on linux
```

## Additional Module Assets

- Unit tests implementation should be in `module-name.test.bl` file.
- Additional information should be in `README.txt` file, we might provide instruction how to compile module dependencies if there are any.
- Bundled platform specific libraries usually in folders with target triple names.


# Libraries

Since BL ABI is compatible with C, you can use C libraries from BL code pretty easily. There are two mandatory steps to do so. Create BL interface for C functions and types, and link the library. In general there are two options for linking: static and dynamic.

Since there is no CLI for adjusting linker options directly, we have to use `build.bl` file.

See also [Build System](modules_build.html#Build-System)

## Linking

First we need some C library to test:

```c
@@@../../how-to/dynamic_library/add.c
```

**Dynamic Linking**

Following build script will compile the C library first using C compiler, and then create BL exectuable linking it dynamically:

```bl
@@@../../how-to/dynamic_library/build.bl
```

**Static Linking**

Following build script will compile the C library first using C compiler, and then create BL exectuable linking it statically:

```bl
@@@../../how-to/static_library/build.bl
```

And finally our `test.bl` file using external code from the library:

```bl
@@@../../how-to/dynamic_library/test.bl
```

## Library Path

To find libraries we have to provide the linker with library paths for search. Compiler by default uses paths listed in `etc/bl.yaml` file in the compiler root directory. All paths are listed in `linker_lib_path` section, you might append this entry in case your path is missing from the list.

Another option is using of `add_lib_path` function provided in the build API.

## macOS frameworks

There are two additional functions available on macOS for linking frameworks.

```bl
// To link macOS framework by name.
link_framework(exe, "CoreFoundation");

// To add framework search path for linker.
add_framework_path(exe, "path/to/frameworks");
```

Currently these are only wrappers around `append_linker_options`. Note that macOS frameworks are not supported in compile-time execution. This might be improved in the future.

# Type Info

The full *runtime type introspection* is supported in BL. You can use `typeinfo(<TYPE>)` builtin function to get the runtime [information](modules_a.html#TypeKind) about the *TYPE*.  The `typeinfo` returns a pointer to the type metadata included in the final binary, so there is no additional unexpected overhead involved (except the binary might be slightly bigger). The type metadata information is added lazily into the binary only for required types.

This feature may be handy for automatic serialization of any kind, the good example is the [print](modules_print.html#print) which gives you automatically pretty print of any passed value or type.

```bl
#import "std/print"

main :: fn () s32 {
    // yields pointer to TypeInfo constant structure
    info := typeinfo(s32);

    if info.kind == TypeKind.INT {
        // safe cast to *TypeInfoInt
        info_int := cast(*TypeInfoInt) info;

        print("bit_count = %\n", info_int.bit_count);

        if info_int.is_signed {
            print("signed\n");
        } else {
            print("unsigned\n");
        }
    }

    return 0;
};
```


# Compiler Builtins

## Variables

List of builtin variables set by compiler:

- `IS_DEBUG` Is bool immutable variable set to true when assembly is running in debug mode.
- `IS_COMPTIME_RUN` Is bool immutable variable set to true when assembly is executed in compile time.
- `BLC_VER_MAJOR` Compiler major version number.
- `BLC_VER_MINOR` Compiler minor version number.
- `BLC_VER_PATCH` Compiler patch version number.

## Builtin Functions

### `sizeof`

```
sizeof(<expr>)
```

Returns size of any expression or type in bytes.

### `alignof`

```
alignof(<expr>) #comptime
```

Returns alignment of any expression or type.

### `typeinfo`

```
typeinfo(<expr>) #comptime
```

Returns pointer to type information structure allocated on a stack.

### `typeof`

```
typeof(<expr>) #comptime
```

Returns type of any expression.

### `compiler_error`

```
compiler_error(message: string_view) #comptime
```

Report error in compile-time.

### `compiler_warning`

```
compiler_warning(message: string_view) #comptime
```

Report warning in compile-time.

# Directives

- `#base` - See [here](manual.html#Implicit-Composition).
- `#build_entry` - See [here](modules_build.html).
- `#call_location` - See [here](manual.html#Call-Location).
- `#compiler` - Compiler internal.
- `#comptime` - Mark symbol as known in compile-time.
- `#enable_if` - See [here](manual.html#enable_if).
- `#entry` - Compiler internal.
- `#export` - See [here](manual.html#export).
- `#extern` - See [here](manual.html#extern).
- `#file` - Evaluates in `string_view` containing name of current file.
- `#flags` - See [here](manual.html#Enum-Flags-Type).
- `#if` - Mark if statement as static if evaluated in compile time.
- `#import` - See [here](manual.html#Import).
- `#inline` - Mark function as inline.
- `#intrinsic` - Compiler internal.
- `#line` - Evaluates in `s32` number of current line in the file.
- `#load` - See [here](manual.html#Load).
- `#maybe_unused` - See [here](manual.html#Usage-Checks).
- `#noinit` - See [here](manual.html#Initialization).
- `#noinline` - Disable function inlining.
- `#obsolete` - Mark function as obsolete.
	```bl
	foo :: fn () #obsolete "Use bar instead!" {}
	```
- `#scope_private` - See [here](manual.html#Private-Scope).
- `#scope_public` - See [here](manual.html#Public-Scope).
- `#scope_module` - See [here](manual.html#Module-Scope).
- `#tag` - See [here](manual.html#Member-Tagging).
- `#test` - See [here](manual.html#Unit-Testing).
- `#thread_local` - See [here](manual.html#Global).

# Documentation

Integrated self-documentation tool can be used to generate [Markdown](https://en.wikipedia.org/wiki/Markdown) files from Biscuit source files automatically. Documentation text can be attached to a file by `//!` comment prefix or to declaration by `///` comment prefix.  Such comments will be recognized by the compiler and attached to the proper declaration or file compilation unit. Use `-doc` compiler flag followed by a list of files you want to generate documentation for. Documentation output will be written to the `out` directory into the current working directory if the `--doc-out-dir` is not specified.

Use marker `@Incomplete` in the documentation comment to mark it as incomplete. The compiler will warn you about symbols with incomplete documentation during generation.

**Documentation rules:**

- Only files listed in compiler input are used as generation input (no loaded or imported files are included).
- Documentation is generated from AST; only a parsing pass is performed.
- When the out directory already exists, the compiler will only append new files and override old ones in case of collision.
- Only global and non-private declarations can be documented.
- A declaration name and declaration itself are included automatically.
- A single `md` file is produced for every input source file.

Example of documented `print` function:

```bl
/// Write string to the standard output (stdout). Format string can include format specifiers `%`
/// which are replaced by corresponding argument value passed in `args`. Value-string conversion is
/// done automatically, we can pass values of any type as an arguments, even structures or arrays.
///
/// The `print` function accepts C-like escape sequences as `\n`, `\t`, `\r`, etc.
///
/// Pointers to `Error` are dereferenced automatically; so the `print` function can print out errors
/// directly.
///
/// Count of printed bytes is returned.
print :: fn (format: string, args: ...) s32 {
    buf: [PRINT_MAX_LENGTH]u8 #noinit;
    w := _print_impl(buf, format, args);
    __os_write(OS_STDOUT, buf.ptr, auto w);
    return w;
}
```

Execution of `blc -doc print.bl` will produce following output:

````md
## print

```
print :: fn (format: string, args: ...) s32
```

Write string to the standard output (stdout). Format string can include format specifiers `%`
which are replaced by corresponding argument value passed in `args`. Value-string conversion is
done automatically, we can pass values of any type as an arguments, even structures or arrays.

The `print` function accepts C-like escape sequences as `\n`, `\t`, `\r`, etc.

Pointers to `Error` are dereferenced automatically; so the `print` function can print out errors
directly.

Count of printed bytes is returned.
````



# Compiler Usage

Biscuit language compiler is a standalone terminal application called *blc*.  It can be compiled from source code found on [GitHub](https://github.com/biscuitlang/bl) repository or downloaded from the home page as a binary executable (since the compiler is still under development, the binary versions may be outdated, currently [compilation](index.html) from the source code is preferred). All three major operating systems (Windows, macOS and Linux) are supported, but current active development is done on Windows and it usually takes some time to port the latest changes to the other platforms. The compiler executable can be found in *bin* directory it's usually a good idea to add the executable location to the system *PATH* to be accessible from other locations.

There are several options that can be passed to the compiler.

## Compiler Arguments

Run `blc --help` to get all supported arguments.

`-build`

Invoke project build pipeline. All following arguments are forwarded into the build script and ignored by the compiler itself. Use as `-build [arguments]`.

`-doc`

Generate documentation and exit.

`-init`

Creates a new project in the current folder. Use as `-init [project-name]`.

`-opt=<debug|release-fast|release-small|release-with-debug-info>`

Specify binary optimization mode (use 'debug' by default).

`-release`

Specify binary optimization mode to release. (Same as `-opt=release-fast`).

`-run`

Execute BL program using interpreter and exit. The compiler expects `<source-file>` after `-run` flag. The file name and all following command line arguments are passed into the executed program and ignored by the compiler itself. Use as `-run <source-file> [arguments]`.

`-shared`

Compile shared library.

`-silent-run`

Execute BL program using interpreter and exit. The compiler expects `<source-file>` after `-silent-run` flag. The file name and all following command line arguments are passed into the executed program and ignored by the compiler itself. This flag also suppresses all compiler console outputs. Combines `-run` and `--silent` into a single flag. Useful when called implicitly from UNIX shebang.

`--about`

Print compiler info and exit.

`--assert=<default|on|off>`

Set assert mode (`'default'` option sets assert `'on'` in debug and `'off'` in release mode).

`--ast-dump`

Print Abstract Syntax Tree (AST).

`--configure`

Generate configuration file and exit.

`--di=<dwarf|codeview>`

Set debug info format.

`--do-cleanup=<off|on>`

Toggles whether compiler should release allocated memory when compilation is done. (`off` by default).

`--doc-out-dir=<STRING>`

Set documentation output directory. (Uses `out` in current working directory by default).

`--emit-asm`

Write assembly to file.

`--emit-llvm`

Write LLVM-IR to file.

`--emit-mir`

Write MIR to file.

`--error-limit=<N>`

Set maximum reported error count.

`--full-path`

Report full file paths.

`--help`

Print usage information and exit.

`--lex-dump`

Print tokens.

`--no-analyze`

Disable analyze pass, only parse and exit.

`--no-api`

Don't load internal API.

`--no-bin`

Don't write binary to disk.

`--no-color`

Disable colored output.

`--no-jobs`

Enable single-thread mode. Useful for compiler debugging.

`--no-llvm`

Disable LLVM back-end.

`--no-usage-check`

Disable checking of unused symbols.

`--no-warning`

Ignore all warnings.

`--output=<STRING>`

Specify name of the output binary.

`--override-config=<STRING>`

Set custom path to the `bl.yaml` configuration file.

`--reg-split=<off|on>`

Enable/disable splitting of structures passed into the function by value into registers. This feature is supposed to be enabled on System V ABI compatible systems.

`--run-tests`

Execute all unit tests during compile time.

`--scope-dump-injection`

Print scope injection structure in dot Graphviz format.

`--scope-dump-parenting`

Print scope parenting structure in dot Graphviz format.

`--silent`

Disable compiler console logging.

`--stats`

Print compilation statistics.

`--syntax-only`

Check syntax and exit.

`--target-experimental`

Enable experimental compilation targets.

`--target-host`

Print current host target triple and exit.

`--target-supported`

Print all supported targets and exit. (Cross compilation is not allowed yet!)

`--tests-minimal-output`

Reduce compile-time tests (`--run-tests`) output (removes results section).

`--verbose`

Enable verbose mode.

`--verify-llvm`

Verify LLVM IR after generation.

`--version`

Print compiler version and exit.

`--vmdbg-attach`

Attach compile-time execution debugger.

`--vmdbg-break-on=<N>`

Attach compile-time execution debugger and set break point to the MIR instruction with `<N>` id.

`--where-is-api`

Print path to API folder and exit.

`--where-is-config`

Print path to default `bl.yaml` configuration file and exit.

`--work-dir=<STRING>`

Set current working directory. The compiler uses the current working directory by default to output all files.


## Configuration

Use `blc --config` to change compiler configuration; this command generates a new `bl.yaml` configuration file containing all the needed information about your system. This configuration runs automatically in case the config file was not found.

## Execution Status

- After a regular compilation process `blc` return 0 on success or a numeric maximum error code on the fail.
- When `-run` flag is specified `blc` return status returned by executed `main` function on success or numeric maximum error code on fail (compilation error or compile time execution error).
- When `--run-tests` flag is specified `blc` returns a count of failed tests on success or a numeric maximum error code on a fail.


# Script mode

Programs written in BL can easily act like shell scripts on UNIX systems due to the support of [shebang](https://en.wikipedia.org/wiki/Shebang_(Unix)) character sequence specified at the first line of the entry file. The `-silent-run` option passed to the compiler reduces all compiler diagnostic output and executes the `main` function using the integrated interpreter. No output binary is produced in such a case. The following example can be directly executed in *bash*.

```bl
#!/usr/local/bin/blc -silent-run

main :: fn () s32 {
    print("Hello!!!\n");
    return 0;
}
```

```bash
$ chmod +x main.bl
$ ./main.bl
```

All command line arguments are forwarded into the executed script and can be accessed via `command_line_arguments` builtin variable. The first argument contains the script name every time.

In case you don't want to use shebang, or your're not on the system supporting it, you can just use `blc -silent-run my-program.bl` in terminal. Note that, there is one limitation: All programs executed using builtin interpreter can use only dynamically loaded libraries, static ones are not supported, basically due to fact, we're not compiling to the native binary, so there is no linking step.

# Unit Testing

> TODO


# Compile-time Debugger

> **Warning:**
This feature is not complete, it's supposed to be used by compiler developers.

Since the compile-time execution is one of the most powerful things on BL and i.e. command-line utility scripts are executed using the interpreter almost every time, we have to provide a proper way how to debug them. In the case of the compile-time execution, no binary file is produced, so we cannot use external debuggers as gdb or Visual Studio, however, the compiler can be used for debugging directly.

Consider the following example program:

```bl
my_function :: fn () {
    ptr: *s32 = null;
    @ptr = 10; // error
}

main :: fn () s32 {
    my_function();
    return 0;
}
```

The mistake here is obvious, we're dereferencing the null pointer and the program will just crash during the execution with the  following error:

```text
$blc -run test.bl

Executing 'main' in compile time...
error: Dereferencing null pointer!

================================================================================
Obtained backtrace:
================================================================================
test.bl:3:5: Last called:
   2 |     ptr: *s32 = null;
>  3 |     @ptr = 10; // error
   4 | }

test.bl:8:5: Called from:
   7 |     debugbreak;
>  8 |     my_function();
   9 |     return 0;
```

To track down the error we can use the compiler built-in function `debugbreak`, causing the execution to be stopped when the interpreter reaches this call.

```bl
my_function :: fn () {
    ptr: *s32 = null;
    @ptr = 10; // error
}

main :: fn () s32 {
    debugbreak; // break here
    my_function();
    return 0;
}
```

When the breakpoint is specified we must execute the program with the compile-time debugger attached to get the actual break. This can be done by the `--vmdbg-attach` command-line argument.

```text
$blc --vmdbg-attach -run test.bl

Executing 'main' in compile time...

Hit breakpoint in assembly 'out'.

   6 | main :: fn () s32 {
>  7 |     debugbreak;
   8 |     my_function();
```

As you can see, the execution breaks on `debugbreak` call, and the debugger waits for user input, type `h` and hit `return` to get all available commands.

```text
: h
  h, help                             = Show this help.
  q, quit                             = Stop debugging.
  n, next                             = Step to next instruction.
  c, continue                         = Continue execution.
  p, print                            = Print current instruction.
  bt, backtrace                       = Print current backtrace.
  vs=<on|off>, verbose-stack=<on|off> = Log stack operations.
  mir=<on|off>, mir-mode=<on|off>     = Enable/disable MIR instruction level debugging.
```

Now we can step through our code, get some stack-related information, print stack traces on the user-code level, but also on MIR instruction level. Printing values of variables is supported only partially.


