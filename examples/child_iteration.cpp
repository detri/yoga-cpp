#include <yoga-cpp/yoga.hpp>
#include <iostream>
#include <format>

// define a layout type that stores a name as a context
using Layout = Yoga::Layout<std::string>;

int main()
{
    Layout layout;
    auto node = layout.createNode();
    layout.addToRoot(node);

    for (int i = 0; i < 10; i++)
    {
        auto child = layout.createNode();
        child.getContext()->append(std::format("child{}", i));
        node.insertChild(child);
    }

    for (auto child : node.getChildren())
    {
        std::cout << child.getContext()->c_str() << '\n';
    }

    return 0;
}
