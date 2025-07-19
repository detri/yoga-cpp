#include <format>
#include <iostream>
#include <yoga-cpp/yoga.hpp>

using Layout = Yoga::Layout<std::monostate>;

int main()
{
    Layout layout;
    auto node = layout.createNode();

    node.setWidthPercent(100.f);
    node.setHeightPercent(100.f);

    node.setWidthPercent(50.f);
    node.setHeightPercent(50.f);

    node.calculateLayout(100.f, 100.f);

    const auto dimensions = std::format(
        "X: {}, Y: {}, W: {}, H: {}",
        node.getLayoutLeft(),
        node.getLayoutTop(),
        node.getLayoutWidth(),
        node.getLayoutHeight());
    // X: 0, Y: 0, W: 50, H: 50
    std::cout << dimensions << '\n';
    return 0;
}
