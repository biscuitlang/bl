# Flow Control

In this chapter, we'll focus on language features focused on conditional code execution and repetitive execution of some code blocks. The BL implements all well-known concepts of flow control and a little bit more. The most common way to let you execute a part of a code only in case some runtime condition is *true* is the *if* statement.

## If Else

The *if-else*, as in many other languages, splits the code into two branches and executes one or the other based on some runtime condition.  The first branch is executed only if the condition is *true*. The expression after *if* statement is required to be a *bool* or at least convertible to *bool* type. The implicit conversion is applied only to the pointer values where the *null* pointer converts to *false* implicitly.

```rust
main :: fn () s32 {
    do_it := true;

    if do_it {
        print("It's done!\n");
    }
}

```

Notice that we don't need additional brackets around the condition like in C-like languages.  We can optionally introduce the *else* block executed if the *if* condition is *false*.

```rust
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

```rust
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

```rust
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

## Switch

A *switch* can compare one numeric value against multiple values and switch execution flow to matching case. The `default` case can be used for all other values we don't explicitly handle. While the expression after *switch* keyword is supposed to be a runtime value, the *case* values must be known in compile-time. Currently, the *switch* can be used only with *integer* types.

```c
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

```rust
switch color {
    Color.RED,
    Color.GREEN { print("It's red or green!\n"); }
    Color.BLUE  { print("It's  blue!\n");  }
}
```

In case the switch is used with *enum* values, the compiler will automatically check for you if all possible cases are covered.

```rust
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

```rust
switch color {
    Color.GREEN { print("It's red or green!\n"); }
    Color.BLUE  { print("It's  blue!\n");  }
    default { print("It's some other color."); }
}
```

In the previous example we print a message for the default case, but we can use just an empty statement here:

```rust
switch color {
    Color.GREEN { print("It's red or green!\n"); }
    Color.BLUE  { print("It's  blue!\n");  }
    default;
}
```

## Loop

Another common way how to manage program control flow is looping. This is a well-known concept available in a lot of languages. We simply run some part of code N-times where N is based on some condition. In BL there is only a single *loop* keyword for loops followed by an optional condition. We can use *break* and *continue* statements inside loops. The *break* statement will simply interrupt looping and skip out of the *loop* body. The *continue* statement will immediately jump to the next loop iteration.

```rust
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

```rust
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

```rust
{% include "../examples/open_file.bl" %}
```

Where the *defer* is used to `close_file` and `str_delete`; we can nicely couple the resource *allocation* and *deallocation* together and also handle possible errors in more elegant way. In this case, the `close_file` will be called in case of any error, while opening a file, after `return 1;` or after the last `return 0;` automatically.