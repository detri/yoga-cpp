#include <format>
#include <iostream>
#include <yoga-cpp/yoga.hpp>

using Layout = Yoga::Layout<std::string>;

int main()
{
    Layout layout;
    auto root = layout.createNode("parent");

    for (int i = 0; i < 10; i++)
    {
        root.createChild(std::format("child{}", i));
    }

    for (auto child : root.getChildren())
    {
        std::cout << child.getContext().c_str() << '\n';
    }

    return 0;
}
