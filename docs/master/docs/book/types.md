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

```rust
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

```rust
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

```rust
array_to_slice :: fn () #test {
    arr : [10] s32;
    slice : []s32 = arr;
};
```

## String Type

String type in Biscuit aka `string_view` is a slice containing a pointer to string data and string length. String literals are always zero-terminated. The `string_view` represents a string of fixed length. In case you want a dynamically allocated string use `string` type and its associated methods. Values of `string` can be implicitly converted to `string_view`.

```rust
string_type :: fn () #test {
    msg : string_view = "Hello world\n";
    msg.len; // character count of the string
    msg.ptr; // pointer to the string content
};
```

## Slice

The array slice consists of the array length and pointer to the first array element.

Slice layout:

```rust
Slice :: struct {
    len: s64;
    ptr: *T
};
```

```rust
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

```rust
Person :: struct {
    id: s32;
    name: string_view;
    age: s32;
}
```

Structure Person in the example consists of id, name and age. Now we can create a variable of this type and fill it with data. To access a person's member fields use `.` operator.

```rust
main :: fn () s32 {
    my_person: Person; // Create instance of type Person
    my_person.id = 1;
    my_person.age = 20;
    my_person.name = "Martin";

    return 0;
}
```

Inline initialization is also possible. We can use a compound expression to set all members at once.

```rust
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

```rust
main :: fn () s32 {
    my_person := Person.{ 1, "Martin", 20 };
    print("%\n", my_person);

    return 0;
}
```

```rust
Person {id = 1, name = Martin, age = 20}
```

Due to lacking OOP support, we cannot declare member functions in structures and there is no class or object concept in the language. A common way to manipulate data is by passing them into the function as an argument.


```rust
person_add_age :: fn (person: *Person, add: s32) {
    // Age can be modified even if the 'person' argument is immutable.
    person.age += add;
}
```

A structure can extend any type with the use of `#base <T>`. This is a kind of inheritance (similar to the C style) where inheritance can be simulated by composition. The `#base <T>` inserts `base: T`; as the first member into the structure. The compiler can use this information later to provide more inheritance-related features like merging of scopes to enable direct access to base-type members via `.` operator or implicit cast from a child to a parent pointer type.

Example of structure extension:

```rust
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

## Union Type

The union is a special composite type representing the value of multiple types. Union size is always equal to the size of the biggest member type and the memory offset of all members is the same. Union is usually associated with some *enum* providing information about the stored type.

```rust
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

```rust
Any :: struct #compiler {
    type_info: *TypeInfo;
    data: *u8
};
```

The *Any* value should never own the original data!

Implicit conversion to *Any* type may cause stack allocation of a temporary variable on the call side in case the original value does not represent stack or heap-allocated memory. This may cause a *hidden* overhead in some cases.

```rust
...
foo(10); // temp for '10' is created here
...

foo :: fn (v: Any) {}
```

For types converted to *Any* compiler implicitly sets `type_info` field to a pointer to the *TypeType* type-info and the data field to the pointer to the actual type-info of the converted type.

```rust
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

```rust
print :: fn (format: string_view, args: ...) {
    ...
};
```

## Enum Type

The _enum_ represents an integer type with a limited set of possible named values. The underlying integer type can be explicitly specified after *enum* keyword, otherwise `s32` is used implicitly. Each possible variant lives in the *enum* namespace.

```rust
{% include "../examples/enums.bl" %}
```

### Nested Enum Type

An enumerator can be nested in any _struct_ in case we want to explicitly associate the enumeration with some data type.

```rust
{% include "../examples/enum_nested.bl" %}
```

## Enum Flags Type

An enumerator can be used as a definition of bit flags by adding #flags directive to the type definition. This directive slightly changes the way how the enumerator values are generated. By default, the enumerator starts with zero variant (if it's not explicitly changed by the programmer) and every following enumerator variant has a value set to the previous one plus one. The flags enumerator starts with the first variant set to 1 and the following variants are set to the left-bit-shifted value of the previous one.

Enumerators marked as flags are also serialized as a combination of atomic flags instead of just one value.

```rust
{% include "../examples/enum_flags.bl" %}
```

So in comparison to conventional enumerators, bunch of binary operations are allowed for flags enumerators to easily manipulate (set, clear and check) its values. Namely:

| Symbol | Description          |
|--------|----------------------|
| &      | Intersection.        |
| \|     | Union.               |
| ~      | Bit flip.            |
| &=     | Assign intersection. |
| \|=    | Assign union.        |

!!! note
    Since flags enumerators start implicitly with value 1, you can explicitly define `NoFlag
    = 0;` variant at the beginning of the variant list.

!!! note
     Flags enumerators should use an unsigned number type as a base type (`u32` by default).

!!! note
     It's possible to do an implicit casting of flags enumerators to its base type.


## Type Casting

Change the type of value to the other type. Conversions between integer types, from pointer to `bool` and from array to slice are generated implicitly by the compiler.

```rust
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

```rust
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

```rust
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

```c
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

```rust
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

