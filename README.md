# Yoga C++ (yoga-cpp)
#### version 2.0.0 (Yoga version 3.2.1)

## Background
Yoga is a layout engine by Meta (née Facebook) which is written
in C++ but exposes only a C API.

This project aims to provide stable user-facing C++ bindings which
wrap the C API in a safe and modern interface.

## Features

- Inline docs
- Templated for safe context usage
- Yoga C API parity
- Layout manager
- Modern C++20 design using concepts and templates
- Ranges for child iteration

## Requirements
- C++20 or later
- CMake 3.15+

## Install

CMake is officially supported:

```cmake
include(FetchContent)

FetchContent_Declare(yoga_cpp
    GIT_REPOSITORY https://github.com/detri/yoga-cpp.git
    GIT_TAG v2.0.0
)

FetchContent_MakeAvailable(yoga_cpp)

target_link_libraries(MyTarget PRIVATE yoga_cpp)
```

The project will download and link Yoga for you, so use yoga-cpp as a drop-in replacement and not an add-on.

## Barebones Example
```c++
#include <yoga-cpp/yogav1.hpp>
#include <iostream>
#include <format>

struct Empty {};
// define a layout type with no context
using Layout = Yoga::Layout<Empty>;

int main()
{
    Layout layout;
    auto root = layout.createNode();
    root.setWidthPercent(100.f);
    root.setHeightPercent(100.f);
    
    auto node = root.createChild();
    node.setWidthPercent(50.f);
    node.setHeightPercent(50.f);
    root.calculateLayout(100.f, 100.f);
    auto dimensions = std::format(
        "X: {}, Y: {}, W: {}, H: {}",
        node.getLayoutLeft(),
        node.getLayoutTop(),
        node.getLayoutWidth(),
        node.getLayoutHeight());
    // X: 0, Y: 0, W: 50, H: 50
    std::cout << dimensions << '\n';
    return 0;
}
```

## Layout\<Ctx\>
The main feature is the `Layout<Ctx>` template class.
Yoga is just a tree of nodes with no concept of ownership or external storage.

This class provides context storage for any constructible type and acts as a factory
that ties Yoga layout nodes and user data together.

In other words, it acts as the source of truth for Yoga node and context lifetimes.
```c++
Layout<std::monostate> layout; // Creates a layout with "empty" context
```

In the spirit of being a manager class, Layouts are not copyable or moveable.
If you need to model ownership of a Layout, construct and use it as a smart pointer.

You can create a node and context together like this:
```c++
Layout<std::string> layout;
auto node = layout.createNode("I am a context argument");
```

Node handles are aware of their owning layouts for ergonomics:
```c++
Layout<std::monostate> layout;
auto node = layout.createNode();

// Adding a child through the handle like this:
auto child = node.createChild();
// is equivalent to this:
auto child = layout.createNode();
node.insertChild(child, node.getChildCount());
```

### Traversing a Layout Tree

Children can be accessed from a node via their `getChildren()` method:

```c++
for (auto child : node.getChildren()) {
  // WARNING: avoid removing nodes while iterating
  child.getContext().doStuff();
}
```

This allows for straightforward recursion. You can store the resulting references in order and reversely iterate over it if you need to walk the tree in reverse implicit Z-order, such as for hit-testing.

### Important: Node Lifetime

A `Node<Ctx>` is a non-owning reference to a node that is managed by a `Layout`. The node's memory is freed when the `Layout` object is destroyed or when you explicitly call `layout.destroyNode(node)`.

Be careful to discard any `Node<Ctx>` objects you have stored elsewhere after removing them from the layout, as they will become invalid. You can check for validity of a specific reference by using `node.valid()`.
Note that in the case of removal, only the reference used to remove the node will become invalid. The rest will become dangling.

## Context
Once you have a node from a layout, its context can be accessed with `getContext()`.
The previous version of this library returned an optional wrapped reference. As of v2.0.0, contexts
are tightly coupled with Nodes within a Layout, so they are no longer optional.
```c++
MyLayoutCtx& ctx = node.getContext(); 
```
