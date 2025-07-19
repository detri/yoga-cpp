#pragma once

#include <cassert>
#include <iterator>
#include <memory>
#include <ranges>
#include <unordered_map>


#include "yoga/Yoga.h"

namespace Yoga
{
    template <typename Node>
    class ChildIterator
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type = Node;
        using difference_type = std::ptrdiff_t;
        using pointer = Node*;
        using reference = Node;

        ChildIterator() : _index{0}, _size{0} {}

        explicit ChildIterator(const Node& parent) : _parent{parent}, _index{0}, _size{parent.getChildCount()} {}

        explicit ChildIterator(const Node& parent, const difference_type index, const size_t size) :
            _parent{parent}, _index(index), _size(size)
        {
        }

        reference operator*() const
        {
            assert(_index >= 0 && static_cast<size_t>(_index) < _size && "Iterator out of bounds");
            return _parent.getChild(_index);
        }

        pointer operator->()
        {
            assert(_index >= 0 && static_cast<size_t>(_index) < _size && "Iterator out of bounds");
            _proxyNode = _parent.getChild(_index);
            return &_proxyNode;
        }

        reference operator[](const difference_type offset) const
        {
            const auto newIndex = _index + offset;
            assert(newIndex >= 0 && static_cast<size_t>(newIndex) < _size && "Iterator out of bounds");
            return _parent.getChild(newIndex);
        }

        ChildIterator& operator++()
        {
            ++_index;
            return *this;
        }

        ChildIterator operator++(int)
        {
            ChildIterator temp = *this;
            ++(*this);
            return temp;
        }

        ChildIterator& operator--()
        {
            --_index;
            return *this;
        }

        ChildIterator operator--(int)
        {
            ChildIterator temp = *this;
            --(*this);
            return temp;
        }

        ChildIterator& operator+=(const difference_type offset)
        {
            _index += offset;
            return *this;
        }

        ChildIterator& operator-=(const difference_type offset)
        {
            _index -= offset;
            return *this;
        }

        friend ChildIterator operator+(const ChildIterator& it, difference_type offset)
        {
            return ChildIterator{it._parent, it._index + offset, it._size};
        }

        friend ChildIterator operator+(difference_type offset, const ChildIterator& it) { return it + offset; }

        friend ChildIterator operator-(const ChildIterator& it, difference_type offset)
        {
            return ChildIterator{it._parent, it._index - offset, it._size};
        }

        difference_type operator-(const ChildIterator& other) const { return _index - other._index; }

        bool operator==(const ChildIterator& other) const { return _index == other._index; }
        bool operator!=(const ChildIterator& other) const { return _index != other._index; }
        bool operator<(const ChildIterator& other) const { return _index < other._index; }
        bool operator>(const ChildIterator& other) const { return _index > other._index; }
        bool operator<=(const ChildIterator& other) const { return _index <= other._index; }
        bool operator>=(const ChildIterator& other) const { return _index >= other._index; }

    private:
        Node _parent;
        difference_type _index;
        size_t _size;
        Node _proxyNode;
    };

    template <typename Node>
    class ChildRange : public std::ranges::view_interface<ChildRange<Node>>
    {
    public:
        ChildRange() = default;

        explicit ChildRange(const Node& parent) : _parent{parent} {}

        ChildIterator<Node> begin() const
        {
            if (!_parent.valid())
            {
                return ChildIterator<Node>{};
            }

            return ChildIterator<Node>{_parent};
        }

        ChildIterator<Node> end() const
        {
            if (!_parent.valid())
            {
                return ChildIterator<Node>{};
            }

            const auto count = _parent.getChildCount();

            return ChildIterator<Node>{_parent, static_cast<std::ptrdiff_t>(count), count};
        }

    private:
        Node _parent;
    };

    template <typename Ctx>
    class Layout;

    template <typename Ctx>
    class Node;

    template <typename Ctx>
    class Layout
    {
    public:
        using context_type = Ctx;
        using node_type = Node<Ctx>;

        Layout() = default;
        ~Layout()
        {
            for (auto& [nodeRef, context] : _nodeContexts)
            {
                YGNodeFree(nodeRef);
            }
            _nodeContexts.clear();
        }

        // Pin layout in memory since it's a manager.
        Layout(const Layout&) = delete;
        Layout& operator=(const Layout&) = delete;
        Layout(Layout&&) = delete;
        Layout& operator=(Layout&&) = delete;

        template <typename... Args>
        node_type createNode(Args&&... args)
        {
            auto ygNode = YGNodeNew();
            _nodeContexts.try_emplace(ygNode, std::make_unique<context_type>(std::forward<Args>(args)...));
            auto node = node_type{this, ygNode};
            node.setContext(_nodeContexts.at(ygNode).get());
            return node;
        }

        void destroyNode(node_type& node)
        {
            if (node.get() == nullptr)
                return;

            auto it = _nodeContexts.find(node.get());
            assert(it != _nodeContexts.end() && "Layout does not contain this node");
            if (it != _nodeContexts.end())
            {
                YGNodeFree(it->first);
                _nodeContexts.erase(it);
                node.invalidate();
            }
        }

    private:
        std::unordered_map<YGNodeRef, std::unique_ptr<context_type>> _nodeContexts;
    };


    template <typename Ctx>
    class Node
    {
    public:
        using layout_type = Layout<Ctx>;
        using context_type = typename layout_type::context_type;

        Node() : _layout{nullptr}, _node{nullptr} {}

        /**
         * @brief Checks if two Node handles refer to the same underlying Yoga node.
         *
         * This allows Nodes to be used in GTest assertions like EXPECT_EQ.
         *
         * @param other The other node handle to compare against.
         * @return True if both handles point to the same node, false otherwise.
         */
        bool operator==(const Node& other) const noexcept { return get() == other.get(); }

        /**
         * @brief Checks if two Node handles refer to different underlying Yoga nodes.
         * @param other The other node handle to compare against.
         * @return True if the handles point to different nodes, false otherwise.
         */
        bool operator!=(const Node& other) const noexcept { return !(*this == other); }


        [[nodiscard]] bool valid() const noexcept { return _layout != nullptr && _node != nullptr; }

        context_type& getContext() noexcept
        {
            assert_valid();
            return *static_cast<context_type*>(YGNodeGetContext(_node));
        }

        const context_type& getContext() const noexcept
        {
            assert_valid();
            return *YGNodeGetContext(_node);
        }

        void setContext(context_type* ctxPtr) { YGNodeSetContext(_node, ctxPtr); }

        [[nodiscard]] YGNodeRef get() const noexcept { return _node; }

        [[nodiscard]] size_t getChildCount() const noexcept
        {
            assert_valid();
            return YGNodeGetChildCount(_node);
        }

        Node getChild(const size_t index) const
        {
            assert_valid();
            return Node{_layout, YGNodeGetChild(_node, index)};
        }

        ChildRange<Node> getChildren()
        {
            assert_valid();
            return ChildRange<Node>{*this};
        }

        Node getParent()
        {
            assert_valid();
            return Node{_layout, YGNodeGetParent(_node)};
        }

        void insertChild(const Node& child, const size_t index = 0)
        {
            assert_valid();
            child.assert_valid();
            assert(_layout == child._layout && "Nodes must belong to the same layout");
            YGNodeInsertChild(_node, child.get(), index);
        }

        void removeChild(const Node& child)
        {
            assert_valid();
            child.assert_valid();
            YGNodeRemoveChild(_node, child.get());
        }

        template <typename... Args>
        Node createChild(Args&&... args)
        {
            assert_valid();
            auto child = _layout->createNode(std::forward<Args>(args)...);
            insertChild(child, getChildCount());
            return child;
        }

        /**
         * Resets this node to its original state.
         *
         * This clears all data including the context pointer.
         *
         * Use with care.
         */
        void reset() noexcept
        {
            assert_valid();
            YGNodeReset(_node);
        }

        /**
         * Calculates the entire layout tree starting from this node.
         * @param width Available width the layout can take up
         * @param height Available height the layout can take up
         * @param direction Reading direction (left-to-right by default)
         */
        void calculateLayout(const float width, const float height,
                             const YGDirection direction = YGDirectionLTR) noexcept
        {
            assert_valid();
            YGNodeCalculateLayout(_node, width, height, direction);
        }

        /**
         * @return Whether the node has a new layout
         */
        [[nodiscard]] bool hasNewLayout() const noexcept
        {
            assert_valid();
            return YGNodeGetHasNewLayout(_node);
        }

        /**
         * @param hasNewLayout Whether the node has a new layout
         */
        void setHasNewLayout(const bool hasNewLayout) noexcept
        {
            assert_valid();
            YGNodeSetHasNewLayout(_node, hasNewLayout);
        }

        /**
         * @return Whether the node has changes that require recalculating its layout
         */
        [[nodiscard]] bool isDirty() const noexcept { return YGNodeIsDirty(_node); }

        /**
         * Marks a node as requiring a layout recalculation
         */
        void markDirty() noexcept
        {
            assert_valid();
            YGNodeMarkDirty(_node);
        }

        /**
         * @param nodeType The type of node (default or text)
         */
        void setNodeType(const YGNodeType nodeType) noexcept
        {
            assert_valid();
            YGNodeSetNodeType(_node, nodeType);
        }

        /**
         * @return The type of node (default or text)
         */
        [[nodiscard]] YGNodeType getNodeType() const noexcept
        {
            assert_valid();
            return YGNodeGetNodeType(_node);
        }

        /**
         * @return The left edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutLeft() const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetLeft(_node);
        }

        /**
         * @return The top edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutTop() const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetTop(_node);
        }

        /**
         * @return The width of the node after layout calculation
         */
        [[nodiscard]] float getLayoutWidth() const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetWidth(_node);
        }

        /**
         * @return The height of the node after layout calculation
         */
        [[nodiscard]] float getLayoutHeight() const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetHeight(_node);
        }

        /**
         * @return The bottom edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutBottom() const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetBottom(_node);
        }

        /**
         * @return The right edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutRight() const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetRight(_node);
        }

        /**
         * @return The reading direction of the node
         */
        [[nodiscard]] YGDirection getLayoutDirection() const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetDirection(_node);
        }

        /**
         * @param edge The edge to get the margin for
         * @return The margin of the node's edge
         */
        [[nodiscard]] float getLayoutMargin(const YGEdge edge) const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetMargin(_node, edge);
        }

        /**
         * @param edge The edge to get the border for
         * @return The width of the border at that edge
         */
        [[nodiscard]] float getLayoutBorder(const YGEdge edge) const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetBorder(_node, edge);
        }

        /**
         * @param edge The edge to get the padding for
         * @return The padding of the node's edge
         */
        [[nodiscard]] float getLayoutPadding(const YGEdge edge) const noexcept
        {
            assert_valid();
            return YGNodeLayoutGetPadding(_node, edge);
        }

        /**
         * Copies this node's style to another
         * @param other The node to copy this node's style to
         */
        void copyStyle(const Node& other) noexcept
        {
            assert_valid();
            YGNodeCopyStyle(_node, other._node);
        }

        /**
         * Set the reading direction of the node.
         * Can be LTR, RTL, or inherited.
         * @param direction Reading direction of the node
         */
        void setDirection(const YGDirection direction) noexcept
        {
            assert_valid();
            YGNodeStyleSetDirection(_node, direction);
        }

        /**
         * @return Current reading direction the node is set to
         */
        [[nodiscard]] YGDirection getDirection() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetDirection(_node);
        }

        /**
         * @param flexDirection The flex direction to give to the node
         */
        void setFlexDirection(const YGFlexDirection flexDirection) noexcept
        {
            assert_valid();
            YGNodeStyleSetFlexDirection(_node, flexDirection);
        }

        /**
         * @return The currently set flex direction
         */
        [[nodiscard]] YGFlexDirection getFlexDirection() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetFlexDirection(_node);
        }

        /**
         * Sets the justify content property of this node.
         *
         * The justify content property defines the alignment along the main axis.
         *
         * @param justifyContent The justify content value to set for this node.
         */
        void setJustifyContent(const YGJustify justifyContent) noexcept
        {
            assert_valid();
            YGNodeStyleSetJustifyContent(_node, justifyContent);
        }

        /**
         * Retrieves the justify content value currently set for this node.
         *
         * The justify content property defines the alignment along the main axis.
         *
         * @return The justify content setting of this node.
         */
        [[nodiscard]] YGJustify getJustifyContent() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetJustifyContent(_node);
        }

        /**
         * Sets the alignment for the container's content.
         * @param alignContent The new align content value for the node.
         */
        void setAlignContent(const YGAlign alignContent) noexcept
        {
            assert_valid();
            YGNodeStyleSetAlignContent(_node, alignContent);
        }

        /**
         * Retrieves the value for the alignment of content within a container.
         *
         * This value dictates how content is distributed along the cross-axis
         * of a container's layout.
         *
         * @return The current content alignment setting.
         */
        [[nodiscard]] YGAlign getAlignContent() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetAlignContent(_node);
        }

        /**
         * Sets the alignment of items within the container.
         *
         * This determines how the items are aligned along the cross axis.
         *
         * @param alignItems The desired alignment value to apply.
         */
        void setAlignItems(const YGAlign alignItems) noexcept
        {
            assert_valid();
            YGNodeStyleSetAlignItems(_node, alignItems);
        }

        /**
         * Retrieves the alignment setting for the items within a container.
         *
         * This function returns the current alignment configuration that dictates
         * how the items are aligned relative to the container along the cross-axis.
         *
         * @return The alignment setting for the items.
         */
        [[nodiscard]] YGAlign getAlignItems() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetAlignItems(_node);
        }

        /**
         * @param alignSelf The alignment value to set for the node.
         */
        void setAlignSelf(const YGAlign alignSelf) noexcept
        {
            assert_valid();
            YGNodeStyleSetAlignSelf(_node, alignSelf);
        }

        /**
         * @return The alignment value for the node's "align-self" property.
         */
        [[nodiscard]] YGAlign getAlignSelf() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetAlignSelf(_node);
        }

        /**
         * @param positionType The position type to be set.
         */
        void setPositionType(const YGPositionType positionType) noexcept
        {
            assert_valid();
            YGNodeStyleSetPositionType(_node, positionType);
        }

        /**
         * @return The current position setting for this node.
         */
        [[nodiscard]] YGPositionType getPositionType() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetPositionType(_node);
        }

        /**
         * @param flexWrap The new flex wrap setting for this node.
         */
        void setFlexWrap(const YGWrap flexWrap) noexcept
        {
            assert_valid();
            YGNodeStyleSetFlexWrap(_node, flexWrap);
        }

        /**
         * @return The current flex wrap setting for this node.
         */
        [[nodiscard]] YGWrap getFlexWrap() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetFlexWrap(_node);
        }

        /**
         * @param overflow The overflow setting of this node.
         */
        void setOverflow(const YGOverflow overflow) noexcept
        {
            assert_valid();
            YGNodeStyleSetOverflow(_node, overflow);
        }

        /**
         * @return Overflow setting for this node.
         */
        [[nodiscard]] YGOverflow getOverflow() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetOverflow(_node);
        }

        /**
         * @param display The new display mode of the node.
         */
        void setDisplay(const YGDisplay display) noexcept
        {
            assert_valid();
            YGNodeStyleSetDisplay(_node, display);
        }

        /**
         * @return The current display mode of the node.
         */
        [[nodiscard]] YGDisplay getDisplay() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetDisplay(_node);
        }

        /**
         * @param flex The new flex value to be assigned to the node.
         */
        void setFlex(const float flex) noexcept
        {
            assert_valid();
            YGNodeStyleSetFlex(_node, flex);
        }

        /**
         * @return The flex value as a float.
         */
        [[nodiscard]] float getFlex() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetFlex(_node);
        }

        /**
         * @param flexGrow The new flex grow for the node.
         */
        void setFlexGrow(const float flexGrow) noexcept
        {
            assert_valid();
            YGNodeStyleSetFlexGrow(_node, flexGrow);
        }

        /**
         * @return The flex grow factor of the node.
         */
        [[nodiscard]] float getFlexGrow() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetFlexGrow(_node);
        }

        /**
         * @param flexShrink The flex shrink factor. A value of 0 means the node does not shrink.
         */
        void setFlexShrink(const float flexShrink) noexcept
        {
            assert_valid();
            YGNodeStyleSetFlexShrink(_node, flexShrink);
        }

        /**
         * @return The flex shrink factor of this node.
         */
        [[nodiscard]] float getFlexShrink() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetFlexShrink(_node);
        }

        /**
         * @param flexBasis The value to set as the flex-basis for this node, in units (usually pixels).
         */
        void setFlexBasis(const float flexBasis) noexcept
        {
            assert_valid();
            YGNodeStyleSetFlexBasis(_node, flexBasis);
        }

        /**
         * @param flexBasisPercent The percentage value to set as the flex basis.
         */
        void setFlexBasisPercent(const float flexBasisPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetFlexBasisPercent(_node, flexBasisPercent);
        }

        /**
         * Sets the flex basis of this node to auto
         */
        void setFlexBasisAuto() noexcept
        {
            assert_valid();
            YGNodeStyleSetFlexBasisAuto(_node);
        }

        /**
         * @return The flex basis value of the node
         */
        [[nodiscard]] YGValue getFlexBasis() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetFlexBasis(_node);
        }

        /**
         * Sets the position of this node.
         *
         * Updates the position to the specified coordinates.
         *
         * @param edge Which edge the position value is relative to.
         * @param position Position in points (usually pixels).
         */
        void setPosition(const YGEdge edge, const float position) noexcept
        {
            assert_valid();
            YGNodeStyleSetPosition(_node, edge, position);
        }

        /**
         * Sets the position of the object as a percentage value.
         *
         * This determines the object's position relative to its container based on the specified percentage.
         *
         * @param edge Which edge the position value is relative to.
         * @param positionPercent Position percentage (0.0 - 100.0).
         */
        void setPositionPercent(const YGEdge edge, const float positionPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetPositionPercent(_node, edge, positionPercent);
        }

        /**
         * Sets a node's edge to auto-position.
         *
         * @param edge The edge to set to auto-position.
         */
        void setPositionAuto(const YGEdge edge) noexcept
        {
            assert_valid();
            YGNodeStyleSetPositionAuto(_node, edge);
        }

        /**
         * Retrieves the current position.
         *
         * This method returns the current position of an edge.
         *
         * @return The current position.
         */
        [[nodiscard]] YGValue getPosition(const YGEdge edge) const noexcept
        {
            assert_valid();
            return YGNodeStyleGetPosition(_node, edge);
        }

        /**
         * Sets the margin of an edge to the specified value.
         *
         * Adjusts the outer spacing adjacent to the edge.
         *
         * @param edge Which edge to set the margin for
         * @param margin The desired margin value to be applied in units (usually pixels).
         */
        void setMargin(const YGEdge edge, const float margin) noexcept
        {
            assert_valid();
            YGNodeStyleSetMargin(_node, edge, margin);
        }

        /**
         * Sets the margin percentage for the current context.
         * @param edge Which edge to set the margin for
         * @param marginPercent The margin value expressed as a percentage.
         */
        void setMarginPercent(const YGEdge edge, const float marginPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetMarginPercent(_node, edge, marginPercent);
        }

        /**
         * Sets the margin of the element to automatic.
         *
         * This allows the margin to be dynamically determined based on the element's context
         * and available space.
         *
         * @param edge Specifies which edge(s) of the element should have automatic margin.
         */
        void setMarginAuto(const YGEdge edge) noexcept
        {
            assert_valid();
            YGNodeStyleSetMarginAuto(_node, edge);
        }

        /**
         * Retrieves the margin value and unit type for the current context.
         *
         * @param edge The edge for which to retrieve the margin.
         * @return The margin unit and value stored in a YGValue.
         */
        [[nodiscard]] YGValue getMargin(const YGEdge edge) const noexcept
        {
            assert_valid();
            return YGNodeStyleGetMargin(_node, edge);
        }

        /**
         * Sets the padding value for an edge of this node.
         *
         * The padding value determines the amount of space between the element's content
         * and its border.
         *
         * @param edge Which edge to set the padding for
         * @param padding The padding value to be set, typically in pixels or other units.
         */
        void setPadding(const YGEdge edge, const float padding) noexcept
        {
            assert_valid();
            YGNodeStyleSetPadding(_node, edge, padding);
        }

        /**
         * Sets the padding percentage for an edge of this node.
         *
         * This method specifies the padding as a percentage.
         *
         * @param edge Which edge to set the padding for
         * @param paddingPercent A floating-point value representing the percentage of padding to be applied.
         */
        void setPaddingPercent(const YGEdge edge, const float paddingPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetPaddingPercent(_node, edge, paddingPercent);
        }

        /**
         * Retrieves the padding value and unit type of the current object.
         * @param edge The edge for which the padding is to be retrieved.
         * @return The padding value and unit type for the specified side, stored in a YGValue.
         */
        [[nodiscard]] YGValue getPadding(const YGEdge edge) const noexcept
        {
            assert_valid();
            return YGNodeStyleGetPadding(_node, edge);
        }

        /**
         * Sets the border width of a node's edge.
         *
         * @param edge The edge of the border
         * @param border The width of the border in units (usually pixels).
         */
        void setBorder(const YGEdge edge, const float border) noexcept
        {
            assert_valid();
            YGNodeStyleSetBorder(_node, edge, border);
        }

        /**
         * Retrieves the border width for an edge on this node.
         *
         * @param edge The edge for which the border information is to be retrieved.
         * @return The border width as a float.
         */
        [[nodiscard]] float getBorder(const YGEdge edge) const noexcept
        {
            assert_valid();
            return YGNodeStyleGetBorder(_node, edge);
        }

        /**
         * Sets the gap size for this node's column or row gutters (or both).
         * @param gutter Which gutter to resize: column, row, or all (YGGutter)
         * @param length The size of the gap in units (usually pixels).
         */
        void setGap(const YGGutter gutter, const float length) noexcept
        {
            assert_valid();
            YGNodeStyleSetGap(_node, gutter, length);
        }

        /**
         * Sets the gutter gap percentage for the current node.
         * @param gutter Which gutter to resize: column, row, or all (YGGutter)
         * @param lengthPercent The size of the gap as a percentage (0-100).
         */
        void setGapPercent(const YGGutter gutter, const float lengthPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetGapPercent(_node, gutter, lengthPercent);
        }

        /**
         * Gets the gap for the specified gutters on this node.
         * @param gutter Which gutter to get the gap size of
         * @return Gutter size
         */
        [[nodiscard]] float getGap(const YGGutter gutter) const noexcept
        {
            assert_valid();
            return YGNodeStyleGetGap(_node, gutter);
        }

        /**
         * Sets the box-sizing property for an element.
         *
         * This determines how the total width and height of an element
         * are calculated, either including or excluding padding and border.
         *
         * @param boxSizing The box-sizing to use for this node
         */
        void setBoxSizing(const YGBoxSizing boxSizing) noexcept
        {
            assert_valid();
            YGNodeStyleSetBoxSizing(_node, boxSizing);
        }

        /**
         * Retrieves the box-sizing property for the current element.
         *
         * The box-sizing property defines how the total width and height
         * of an element are calculated, either by including or excluding
         * padding and border.
         *
         * @return The box-sizing value of the current element.
         */
        [[nodiscard]] YGBoxSizing getBoxSizing() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetBoxSizing(_node);
        }

        /**
         * Sets the width of the object.
         *
         * This method updates the width property to the given value.
         *
         * @param width The new width to set, typically in pixels or the relevant unit.
         */
        void setWidth(const float width) noexcept
        {
            assert_valid();
            YGNodeStyleSetWidth(_node, width);
        }

        /**
         * Sets the width as a percentage of the parent's width.
         *
         * The percentage value should be between 0 and 100.
         * This allows for relative sizing of the element.
         *
         * @param widthPercent The width percentage to set, where 0 represents no width
         *                and 100 represents the full width of the parent.
         */
        void setWidthPercent(const float widthPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetWidthPercent(_node, widthPercent);
        }

        /**
         * Automatically sets the width of the element.
         *
         * This adjusts the width dynamically based on the content or context,
         * ensuring the optimal size is determined and applied automatically.
         *
         * Designed to replace manual width configuration when flexibility is required.
         */
        void setWidthAuto() noexcept
        {
            assert_valid();
            YGNodeStyleSetWidthAuto(_node);
        }

        /**
         * Retrieves the width of the object.
         *
         * This method returns the current width value, typically representing
         * the horizontal size or extent of the object in units.
         *
         * @return The width of the object.
         */
        [[nodiscard]] YGValue getWidth() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetWidth(_node);
        }

        /**
         * Sets the height of the object to the specified value.
         *
         * This modifies the height property and may trigger other dependent calculations.
         *
         * @param height The new height value to be set.
         */
        void setHeight(const float height) noexcept
        {
            assert_valid();
            YGNodeStyleSetHeight(_node, height);
        }

        /**
         * Sets the height of an element as a percentage of its container's height.
         *
         * This method adjusts the height proportionally based on the provided percentage,
         * influencing its appearance relative to its parent container.
         *
         * @param heightPercent The percentage value to set the height, represented as a floating-point number.
         */
        void setHeightPercent(const float heightPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetHeightPercent(_node, heightPercent);
        }

        /**
         * Automatically adjusts the height of this component based on its content.
         *
         * This method enables dynamic resizing to fit the contained data or layout,
         * ensuring proper presentation without manual height adjustments.
         */
        void setHeightAuto() noexcept
        {
            assert_valid();
            YGNodeStyleSetHeightAuto(_node);
        }

        /**
         * Retrieves the height of the object.
         *
         * This method returns the measured height, which is typically calculated
         * based on the object's structure and content.
         *
         * @return The height of the object as an integer.
         */
        [[nodiscard]] YGValue getHeight() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetHeight(_node);
        }

        /**
         * Sets the minimum width for this element.
         *
         * This method defines the smallest allowable width that the element can have.
         * If the provided width is smaller than allowed, it may be automatically adjusted.
         *
         * @param minWidth The desired minimum width for the element, specified in appropriate units.
         */
        void setMinWidth(const float minWidth) noexcept
        {
            assert_valid();
            YGNodeStyleSetMinWidth(_node, minWidth);
        }

        /**
         * Sets the minimum width as a percentage of the parent container's width.
         *
         * This method ensures the element maintains a minimum width relative to its
         * parent container, specified as a percentage value.
         *
         * @param minWidthPercent The minimum width percentage to set. Must be a value
         *                between 0 and 100, where 0 represents 0% and 100
         *                represents 100% of the parent container's width.
         */
        void setMinWidthPercent(const float minWidthPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetMinWidthPercent(_node, minWidthPercent);
        }

        /**
         * Retrieves the minimum width required.
         *
         * This value typically represents constraints or configurations
         * that define the smallest permissible width.
         *
         * @return The minimal allowable width as an integer.
         */
        [[nodiscard]] YGValue getMinWidth() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetMinWidth(_node);
        }

        /**
         * Sets the minimum height for this element.
         *
         * This ensures that the element will not shrink below the specified height.
         * It can be helpful in maintaining a consistent layout.
         *
         * @param minHeight The minimum height value to set, typically in pixels or another supported unit.
         */
        void setMinHeight(const float minHeight) noexcept
        {
            assert_valid();
            YGNodeStyleSetMinHeight(_node, minHeight);
        }

        /**
         * Sets the minimum height as a percentage of the total allowable height.
         *
         * This defines the constrained minimum height to maintain layout consistency or design requirements.
         *
         * @param minHeightPercent The minimum height percentage to set, represented as a value between 0 and 100.
         */
        void setMinHeightPercent(const float minHeightPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetMinHeightPercent(_node, minHeightPercent);
        }

        /**
         * Retrieves the minimum height value.
         *
         * The method calculates or provides the smallest height applicable
         * based on internal logic or stored data.
         *
         * @return The minimum height as a numeric value.
         */
        [[nodiscard]] YGValue getMinHeight() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetMinHeight(_node);
        }

        /**
         * Sets the maximum width for the element.
         *
         * This determines the maximum horizontal size that the element is allowed
         * to occupy.
         *
         * @param maxWidth The maximum allowable width for the element, specified
         * in appropriate measurement units.
         */
        void setMaxWidth(const float maxWidth) noexcept
        {
            assert_valid();
            YGNodeStyleSetMaxWidth(_node, maxWidth);
        }

        /**
         * Sets the maximum width as a percentage of the parent container's width.
         *
         * Adjusts the layout to restrict the width relative to the parent's size.
         *
         * @param maxWidthPercent The percentage of the parent's width to set as the maximum width.
         *                Accepts values between 0 and 100.
         */
        void setMaxWidthPercent(const float maxWidthPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetMaxWidthPercent(_node, maxWidthPercent);
        }

        /**
         * Retrieves the maximum width value.
         *
         * This method calculates and returns the maximum width available or defined.
         *
         * @return The maximum width as an integer value.
         */
        [[nodiscard]] YGValue getMaxWidth() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetMaxWidth(_node);
        }

        /**
         * Sets the maximum height allowed for the specified element.
         *
         * This defines the upper limit for height and ensures the element
         * does not exceed this value.
         *
         * @param maxHeight The maximum height to be set, specified as an integer.
         */
        void setMaxHeight(const float maxHeight) noexcept
        {
            assert_valid();
            YGNodeStyleSetMaxHeight(_node, maxHeight);
        }

        /**
         * Sets the maximum height as a percentage.
         *
         * This method defines the maximum height limit relative to a percentage-based scale.
         *
         * Use this to constrain the height within a specific range.
         *
         * @param maxHeightPercent The maximum height percentage to set. Should be between 0 and 100.
         */
        void setMaxHeightPercent(const float maxHeightPercent) noexcept
        {
            assert_valid();
            YGNodeStyleSetMaxHeightPercent(_node, maxHeightPercent);
        }

        /**
         * Retrieves the maximum height value.
         *
         * This method calculates and returns the highest value
         * based on the implementation's criteria.
         *
         * @return The maximum height value as determined by the method.
         */
        [[nodiscard]] YGValue getMaxHeight() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetMaxHeight(_node);
        }

        /**
         * Sets the aspect ratio for this object.
         *
         * The aspect ratio defines the proportional relationship
         * between the width and height.
         *
         * @param aspectRatio The aspect ratio to be set, typically expressed
         *              as a float (e.g., 1.33 for 4:3 or 1.78 for 16:9).
         */
        void setAspectRatio(const float aspectRatio) noexcept
        {
            assert_valid();
            YGNodeStyleSetAspectRatio(_node, aspectRatio);
        }

        /**
         * Calculates and returns the aspect ratio of this object.
         *
         * The aspect ratio is defined as the ratio of width to height.
         *
         * @return The aspect ratio as a floating-point value.
         */
        [[nodiscard]] float getAspectRatio() const noexcept
        {
            assert_valid();
            return YGNodeStyleGetAspectRatio(_node);
        }

    private:
        friend class Layout<Ctx>;

        Node(layout_type* layout, const YGNodeRef node) : _layout{layout}, _node{node}
        {
            assert(layout != nullptr && "Layout must not be null");
        }

        // Allows Layout to invalidate a handle after destruction.
        void invalidate()
        {
            _layout = nullptr;
            _node = nullptr;
        }

        void assert_valid() const { assert(valid() && "Node handle is invalid"); }

        layout_type* _layout;
        YGNodeRef _node;
    };
} // namespace Yoga
