# Yoga C++ (yoga-cpp)
#### version 1.0.0 (Yoga version 3.2.1)

## Background
Yoga is a layout engine by Meta (née Facebook) which is written
in C++ but exposes only a C API.

This project aims to provide stable user-facing C++ bindings which
wrap the C API in a safe and modern interface.

It also includes the optional and more opinionated `Yoga::Layout` class,
which provides a simple way to manage and traverse a Yoga layout tree.

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
3. A reference (`Node<Ctx>`) to the root node used for calculating the layout.

The API is primarily used via the `createNode()` method, which gives you a non-owning reference to a brand new wrapped Yoga node, with which you can do as you please.
You can also access the root node with `getRoot()` i.e. to set a background color that may exist in a custom context.

```c++
struct StyleCtx
{
    Color bgColor;
}
Layout<StyleCtx> layout;
layout.getRoot().getContext().bgColor = Color::Black;
```

Nodes and their contexts can be deleted by using `removeNode(Node<Ctx> node)` on a node reference.
If you track references (i.e. storing Node\<Ctx\> in a map to "name" them), be careful to get rid of any invalid references.
You can compare nodes with the underlying pointer using the `Node<Ctx>::getRef()` method.

The layout can be calculated based on available viewport space using `calculate(width, height, direction = YGDirectionLTR)`.

### Traversing a Layout Tree

Children can be accessed from a node via their `getChildren()` method:

```c++
for (auto child : node.getChildren()) {
  // WARNING: avoid removing nodes while iterating
  child.getContext().doStuff();
}
```
You can also traverse the entire tree starting from the root:

```c++
std::vector<Node<Ctx>> allNodes;
layout.walkTree([&](const auto& node) {
    allNodes.emplace_back(node);
});
```

### Important: Node Lifetime

A `Node<Ctx>` is a non-owning reference to a node that is managed by a `Layout`. The node's memory is freed when the `Layout` object is destroyed or when you explicitly call `layout.removeNode(node)`.

Be careful to discard any `Node<Ctx>` objects you have stored elsewhere after removing them from the layout, as they will become invalid. You can check for validity by using the `if (node)` pattern.

## Context
Once you have a node from a layout, its context can be accessed with `getContext()`.
Note that this returns an optional wrapped reference to your context, i.e. `std::optional<std::reference_wrapper<Ctx>>`.
```c++
auto ctxWrapper = node.getContext() // std::optional<std::reference_wrapper<DoStuffCtx>>
if (ctxWrapper) {
  auto& ctx = ctxWrapper->get(); // DoStuffCtx& (also available as const)
  ctx.doStuff();
}
```

## Usage without Layout
`yoga-cpp` can be used as plain Yoga bindings by directly constructing
OwningNodes yourself and hooking them together. The method names mostly follow
the C API's naming conventions, in pascalCase and without prefixes,
except `Layout` methods which property names with styles are prefixed by `getLayout` or `setLayout`
instead of just `get` or `set`.

```c++
using MyNode = OwningNode<>; // node type with empty/monostate ctx
auto node = MyNode{}; // fresh, blank node that will be destroyed when the scope exits
```