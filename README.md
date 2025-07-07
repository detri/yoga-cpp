# Yoga C++ (yoga-cpp)
#### version 1.0.0 (Yoga version 3.2.1)

## Background
Yoga is a layout engine by Meta (née Facebook) which is written
in C++ but exposes only a C API.

This project aims to provide stable user-facing C++ bindings which
mirror the C API almost directly.

## Features

- Templated for safe context usage
- Yoga C API parity
- Layout manager

## Requirements
- C++20 or later
- CMake 3.15+

## Install

CMake is officially supported:

```cmake
include(FetchContent)

FetchContent_Declare(yoga_cpp
    GIT_REPOSITORY https://github.com/detri/yoga-cpp.git
    GIT_TAG v1.0.0
)

FetchContent_MakeAvailable(yoga_cpp)

target_link_libraries(MyTarget PRIVATE yoga_cpp)
```

The project will download and link Yoga for you, so use yoga-cpp as a drop-in replacement and not an add-on.



## Barebones Example
```c++
#include <yoga-cpp/yoga.hpp>
#include <iostream>
#include <format>

struct Empty {};
// define a layout type with no context
using Layout = Yoga::Layout<Empty>;

int main()
{
    Layout layout;
    auto node = layout.createNode();
    layout.addToRoot(node);
    
    node.setWidthPercent(50.f);
    node.setHeightPercent(50.f);
    layout.calculate(100.f, 100.f);
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
**Layout\<Ctx\>** is a templated class that manages a layout and its contexts for you.

The lifetimes of the nodes in a layout are tied to the layout object's lifetime.

It has one requirement, which is that the context must be default-constructible.

The internals consist of:

1. A Yoga::Config which is an owning wrapper around YGConfig.
2. Flat node storage to keep layout elements alive (you work with copyable references to the tree). This is just `std::vector<OwningNode<Ctx>>`.
3. An unordered map to store layout contexts by their internal Yoga node pointers. Backed by `std::unordered_map<YGNodeRef, Ctx>`.
4. A reference (`Node<Ctx>`) to the root node used for calculating the layout.

The API is primarily used via the `createNode()` method, which gives you a non-owning reference to a wrapped Yoga node, with which you can do as you please.
You can also access the root node with `getRoot()` i.e. to set a background color that may exist in a custom context.

```c++
struct StyleCtx
{
    Color bgColor;
}
Layout<StyleCtx> layout;
layout.getRoot().getContext().bgColor = Color::Black;
```

Nodes and their contexts can be deleted by using `removeNode(Node<Ctx> node)` on a reference.
If you track references (i.e. storing Node\<Ctx\> in a map to "name" them), be careful to get rid of any invalid references.
You can compare nodes with the underlying pointer using the `Node<Ctx>::getRef()` method.

Nodes are bool-convertible, so you can write an `if (node)` block to check for validity.

The layout can be calculated based on available app or screen width using `calculate(width, height, direction = YGDirectionLTR)`.

## Context
Once you have a node from a layout, its context can be accessed with `getContext()`.
Note that this returns a pointer and that contexts are optional in Yoga,
so please do check it for nullness. It can be accessed mutably or immutably.