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
