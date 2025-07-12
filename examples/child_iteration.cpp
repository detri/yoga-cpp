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
        auto ctx = child.getContext();
        if (ctx)
        {
            auto& ctxRef = ctx->get();
            ctxRef.append(std::format("child{}", i));
        }
        node.insertChild(child);
    }

    for (auto child : node.getChildren())
    {
        std::cout << child.getContext()->get().c_str() << '\n';
    }

    return 0;
}
