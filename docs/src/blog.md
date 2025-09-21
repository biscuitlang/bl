# Struct Metadata (21-09-2025)

## Introduction

One of the common requirements for modern languages is support for serialization, or at least some methods to make it easier. In BL, we can use compile-time generated type information to iterate over structure members, retrieving their types, offsets, and other type-related information to introduce some automation into the scene. This way, any general type can be easily serialized into JSON, XML, or UI form out of the box using type info, memory offsets, and pointers. Consider the print function, utilizing this to simply print any type passed without the need for explicit formatting strings, like in C. One downside, for more complex systems, is the lack of possibility to attach arbitrary metadata to struct members. On the one hand, we don’t want to limit the amount of user data injected into the type system; on the other hand, we want to keep type system memory requirements relatively low. Thus, the only possibility for the user right now is to use structure member tags. A tag is a simple 64-bit number baked into the struct member type information. Consider the following example:

```bl
TAG :: 10;

Data :: struct {
	member: s32 #tag TAG;
}
```

This way value of `TAG` is stored in the type system and can be later retrieved.

```bl
info :: cast(*TypeInfoStruct) Data;
loop i := 0; i < info.members.len; i += 1 {
	member :: &info.members[i];
	print("member tag: %", member.tag);
}
```

In a simple scenario, this might be enough. Consider a situation when we want to automatically serialize this structure and skip some members. We can use a tag to mark the member to be skipped.

```bl
TAG_NO_SERIALIZE :: 1;

Data :: struct {
	member: s32 #tag TAG_NO_SERIALIZE;
}
```

The serializer can later easily peek into processed member tags and skip them in case `TAG_NO_SERIALIZE` is set.

This is nice, but what if we want to introduce more tags and combine them? Well, we can use flags.

```bl
TAG_NO_SERIALIZE :: 1<<1;
TAG_NO_UI :: 1<<2;

Data :: struct {
	member1: s32 #tag TAG_NO_SERIALIZE;
	member2: s32 #tag TAG_NO_SERIALIZE | TAG_NO_UI;
}
```

So both members will be excluded from serialization, and only `member1` is visible in the UI.

Even though simple flag-based tags were enough for me for quite some time, there are some limitations. For a simple serialization or UI, this is good enough; we probably would not end up with a lot of flags (we have up to 64 of them), but what about complex UI systems where you want to specify, for example valid range of the member value, units used in UI, etc. 

After some time, I realized that a user-level solution to this problem is actually possible, without the need to modify how the compiler and type system work. We can push tags even further by abusing compile-time execution.

## Compile-time Metadata

Since a tag number can be any arbitrary 64-bit number, what if it’s actually an index to a compile-time generated array of metadata? Wild idea? Let’s explore the possibilities we have. First, we need to allocate some memory in the binary data segment to hold metadata. Note that since all metadata is written into the executable, we must preallocate a large enough chunk of memory. This brings one limitation: we might run out of memory to store all the data we need, but since the amount of data required depends purely on our code, we might just invoke an assertion at compile time when it happens. Let’s start with metadata definition. We’ll store the `unit` of the value, and a simple valid range as the `min` and `max` values.

```bl
Unit :: enum { MM; M; KM; }

Meta :: struct {
	used: bool;
	unit: Unit;
	min: s32;
	max: s32;
}
```

Now let’s allocate stack memory to hold the data:

```bl
meta_reserve :: fn (num: s32) []Meta #comptime {
	data :: new_slice(Meta, num) catch panic($);
	return data;
}

// Allocate constant holding up to 4 elements.
META :: meta_reserve(4);
```

One important note here is that once the `META` constant is initialized during compilation (by calling `meta_reserve`), we cannot modify it later. But what we cannot modify are the values of the slice itself, so the length and pointer. However, we can change the content of the memory we point to.

Another important thing to understand here is the use of `new_slice` inside `meta_reserve`; since this function is executed in compile-time and the `new_slice` internally calls `malloc`, we allocate memory in the compiler context. Sadly, there is no way to free this memory later, but since the compiler configured in *one-shot* mode does not free any allocated memory by default, it should be fine here too.

Also, note that the data is *zero-initialized*.

Now, let’s introduce a simple helper function to comfortably assign this metadata to structure members.

```bl
meta :: fn (unit: Unit, min := 0, max := 1000) u64 #comptime {
	index := 0;
	// Find next free meta entry...
	loop ; index < META.len; index += 1 {
		if META[index].used then continue;
		break;
	}

	// Just in case.
	assert(index < META.len && index >= 0);
	
	tag :: &META[index];
	tag.used = true;
	tag.unit = unit;
	tag.min  = min;
	tag.max  = max;
	
	return auto index + 1;
}
```

This function will be executed at compile-time, returning the index of the meta record + 1. We use +1 to distinguish members without any metadata assigned, so we need to subtract one later when this index is used.

We might use our helper function like this:

```bl
Data :: struct {
	member1: s32 #tag meta(Unit.MM, 10, 60);
	member2: s32 #tag meta(Unit.KM);
}
```

And finally, we can resolve metadata via the type system as usual, using the type info of the struct.

```bl
info :: cast(*TypeInfoStruct) typeinfo(Data);
	loop i := 0; i < info.members.len; i += 1 {
	member :: &info.members[i];
	if member.tag > 0 {
		// Here we subtract one!
		print("  %\n", META[auto member.tag-1]);
	}
}
```

## Conclusion

In the end, a simple number stored in the type system turned out to be kind of powerful. We might go even further by extending the encoding of the tag by using the bottom 32 bits to store regular flags, and the rest for metadata indices.
