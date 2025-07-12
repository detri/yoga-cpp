#pragma once

#include <algorithm>
#include <cassert>
#include <functional>
#include <optional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>
#include "yoga/Yoga.h"

namespace Yoga
{
    template <typename T>
    concept DefaultConstructible = std::is_default_constructible_v<T>;

    template <DefaultConstructible Ctx>
    class Config
    {
    public:
        Config() : _ygConfig{YGConfigNew()} { setContext(&_context); }
        ~Config()
        {
            if (_ygConfig != nullptr)
                YGConfigFree(_ygConfig);
        }

        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        Config(Config&& other) noexcept : _ygConfig{other._ygConfig}, _context{std::move(other._context)}
        {
            other._ygConfig = nullptr;
            if (this->_ygConfig != nullptr)
            {
                YGConfigSetContext(_ygConfig, &_context);
            }
        }
        Config& operator=(Config&& other) noexcept
        {
            if (this != &other)
            {
                if (_ygConfig != nullptr)
                {
                    YGConfigFree(_ygConfig);
                }
                _ygConfig = other._ygConfig;
                other._ygConfig = nullptr;
                _context = std::move(other._context);
                if (this->_ygConfig != nullptr)
                {
                    YGConfigSetContext(_ygConfig, &_context);
                }
            }
            return *this;
        }

        void setUseWebDefaults(const bool useWebDefaults) { YGConfigSetUseWebDefaults(_ygConfig, useWebDefaults); }
        [[nodiscard]] bool getUseWebDefaults() const { return YGConfigGetUseWebDefaults(_ygConfig); }

        void setPointScaleFactor(const float pointScaleFactor)
        {
            YGConfigSetPointScaleFactor(_ygConfig, pointScaleFactor);
        }
        [[nodiscard]] float getPointScaleFactor() const { return YGConfigGetPointScaleFactor(_ygConfig); }

        void setErrata(const YGErrata errata) { YGConfigSetErrata(_ygConfig, errata); }
        [[nodiscard]] YGErrata getErrata() const { return YGConfigGetErrata(_ygConfig); }

        void setLogger(const YGLogger logger) { YGConfigSetLogger(_ygConfig, logger); }

        void setContext(Ctx* context) { YGConfigSetContext(_ygConfig, context); }

        std::optional<std::reference_wrapper<Ctx>> getContext() noexcept
        {
            auto* ctxPtr = YGConfigGetContext(_ygConfig);
            if (ctxPtr == nullptr)
            {
                return std::nullopt;
            }
            return std::ref(*static_cast<Ctx*>(ctxPtr));
        }

        std::optional<std::reference_wrapper<const Ctx>> getContext() const noexcept
        {
            auto* ctxPtr = YGConfigGetContext(_ygConfig);
            if (ctxPtr == nullptr)
            {
                return std::nullopt;
            }
            return std::ref(*static_cast<const Ctx*>(ctxPtr));
        }

        [[nodiscard]] YGConfigRef getRef() const noexcept { return _ygConfig; }
        [[nodiscard]] YGConfigConstRef getConstRef() const noexcept { return _ygConfig; }

    private:
        YGConfigRef _ygConfig;
        Ctx _context;
    };

    /**
     * Non-owning representation of a Yoga node.
     * Contains most of the Yoga API as methods.
     * You should acquire one from a Yoga::Layout.
     * @tparam Ctx Context type stored per node - must be default constructible
     */
    template <DefaultConstructible Ctx = std::monostate>
    class Node
    {
    public:
        static constexpr bool isOwning = false;

        template <DefaultConstructible LayoutCtx, DefaultConstructible ConfigCtx>
        friend class Layout;

        template <DefaultConstructible OwningCtx>
        friend class OwningNode;

        using context_type = Ctx;

        Node() = delete;

        explicit Node(YGNodeRef ref) noexcept : _ygNode{ref} {}

        ~Node() = default;

        Node(const Node& other) = default;
        Node& operator=(const Node& other) = default;
        Node(Node&& other) noexcept = default;
        Node& operator=(Node&& other) noexcept = default;

        explicit operator bool() const noexcept { return _ygNode != nullptr; }

        /**
         * Iterator implementation for node children
         */
        class ChildIterator
        {
        public:
            ChildIterator() = delete;

            ChildIterator(YGNodeRef node, const size_t index) noexcept : _ygNode{node}, _index{index} {}

            ChildIterator& operator++() noexcept
            {
                ++_index;
                return *this;
            }

            ChildIterator operator++(int) noexcept
            {
                ChildIterator tmp = *this;
                ++_index;
                return tmp;
            }

            constexpr bool operator!=(const ChildIterator& other) const noexcept
            {
                return _ygNode != other._ygNode || _index != other._index;
            }
            constexpr bool operator==(const ChildIterator& other) const noexcept
            {
                return _ygNode == other._ygNode && _index == other._index;
            }

            Node operator*() const noexcept { return Node{YGNodeGetChild(_ygNode, _index)}; }

            constexpr std::ptrdiff_t operator-(const ChildIterator& other) const noexcept
            {
                return static_cast<std::ptrdiff_t>(_index) - static_cast<std::ptrdiff_t>(other._index);
            }

        private:
            YGNodeRef _ygNode;
            size_t _index;
        };

        class ChildView : public std::ranges::view_interface<ChildView>
        {
        public:
            explicit ChildView(const Node& node) noexcept : _ygNode{node.getRef()} {}

            auto begin() const noexcept { return ChildIterator{_ygNode, 0}; }
            auto end() const noexcept { return ChildIterator{_ygNode, YGNodeGetChildCount(_ygNode)}; }

        private:
            YGNodeRef _ygNode;
        };

        /**
         * Resets this node to its original state.
         *
         * This clears all data including the context pointer.
         *
         * Use with care.
         */
        void reset() noexcept
        {
            assert(_ygNode != nullptr && "Reset called on invalid node");
            YGNodeReset(_ygNode);
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
            assert(_ygNode != nullptr && "Calculate layout called on invalid node");
            YGNodeCalculateLayout(_ygNode, width, height, direction);
        }

        /**
         * @return Whether the node has a new layout
         */
        [[nodiscard]] bool hasNewLayout() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeGetHasNewLayout(_ygNode);
        }

        /**
         * @param hasNewLayout Whether the node has a new layout
         */
        void setHasNewLayout(const bool hasNewLayout) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeSetHasNewLayout(_ygNode, hasNewLayout);
        }

        /**
         * @return Whether the node has changes that require recalculating its layout
         */
        [[nodiscard]] bool isDirty() const noexcept { return YGNodeIsDirty(_ygNode); }

        /**
         * Marks a node as requiring a layout recalculation
         */
        void markDirty() noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeMarkDirty(_ygNode);
        }

        /**
         * @return The number of children associated with this node
         */
        [[nodiscard]] size_t getChildCount() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeGetChildCount(_ygNode);
        }

        /**
         * @param nodeType The type of node (default or text)
         */
        void setNodeType(const YGNodeType nodeType) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeSetNodeType(_ygNode, nodeType);
        }

        /**
         * @return The type of node (default or text)
         */
        [[nodiscard]] YGNodeType getNodeType() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeGetNodeType(_ygNode);
        }

        /**
         * @return The left edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutLeft() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetLeft(_ygNode);
        }

        /**
         * @return The top edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutTop() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetTop(_ygNode);
        }

        /**
         * @return The width of the node after layout calculation
         */
        [[nodiscard]] float getLayoutWidth() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetWidth(_ygNode);
        }

        /**
         * @return The height of the node after layout calculation
         */
        [[nodiscard]] float getLayoutHeight() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetHeight(_ygNode);
        }

        /**
         * @return The bottom edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutBottom() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetBottom(_ygNode);
        }

        /**
         * @return The right edge of the node after layout calculation
         */
        [[nodiscard]] float getLayoutRight() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetRight(_ygNode);
        }

        /**
         * @return The reading direction of the node
         */
        [[nodiscard]] YGDirection getLayoutDirection() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetDirection(_ygNode);
        }

        /**
         * @param edge The edge to get the margin for
         * @return The margin of the node's edge
         */
        [[nodiscard]] float getLayoutMargin(const YGEdge edge) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetMargin(_ygNode, edge);
        }

        /**
         * @param edge The edge to get the border for
         * @return The width of the border at that edge
         */
        [[nodiscard]] float getLayoutBorder(const YGEdge edge) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetBorder(_ygNode, edge);
        }

        /**
         * @param edge The edge to get the padding for
         * @return The padding of the node's edge
         */
        [[nodiscard]] float getLayoutPadding(const YGEdge edge) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeLayoutGetPadding(_ygNode, edge);
        }

        /**
         * Copies this node's style to another
         * @param other The node to copy this node's style to
         */
        void copyStyle(const Node& other) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeCopyStyle(_ygNode, other._ygNode);
        }

        /**
         * Set the reading direction of the node.
         * Can be LTR, RTL, or inherited.
         * @param direction Reading direction of the node
         */
        void setDirection(const YGDirection direction) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetDirection(_ygNode, direction);
        }

        /**
         * @return Current reading direction the node is set to
         */
        [[nodiscard]] YGDirection getDirection() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetDirection(_ygNode);
        }

        /**
         * @param flexDirection The flex direction to give to the node
         */
        void setFlexDirection(const YGFlexDirection flexDirection) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlexDirection(_ygNode, flexDirection);
        }

        /**
         * @return The currently set flex direction
         */
        [[nodiscard]] YGFlexDirection getFlexDirection() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetFlexDirection(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetJustifyContent(_ygNode, justifyContent);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetJustifyContent(_ygNode);
        }

        /**
         * Sets the alignment for the container's content.
         * @param alignContent The new align content value for the node.
         */
        void setAlignContent(const YGAlign alignContent) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetAlignContent(_ygNode, alignContent);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetAlignContent(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetAlignItems(_ygNode, alignItems);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetAlignItems(_ygNode);
        }

        /**
         * @param alignSelf The alignment value to set for the node.
         */
        void setAlignSelf(const YGAlign alignSelf) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetAlignSelf(_ygNode, alignSelf);
        }

        /**
         * @return The alignment value for the node's "align-self" property.
         */
        [[nodiscard]] YGAlign getAlignSelf() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetAlignSelf(_ygNode);
        }

        /**
         * @param positionType The position type to be set.
         */
        void setPositionType(const YGPositionType positionType) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetPositionType(_ygNode, positionType);
        }

        /**
         * @return The current position setting for this node.
         */
        [[nodiscard]] YGPositionType getPositionType() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetPositionType(_ygNode);
        }

        /**
         * @param flexWrap The new flex wrap setting for this node.
         */
        void setFlexWrap(const YGWrap flexWrap) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlexWrap(_ygNode, flexWrap);
        }

        /**
         * @return The current flex wrap setting for this node.
         */
        [[nodiscard]] YGWrap getFlexWrap() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetFlexWrap(_ygNode);
        }

        /**
         * @param overflow The overflow setting of this node.
         */
        void setOverflow(const YGOverflow overflow) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetOverflow(_ygNode, overflow);
        }

        /**
         * @return Overflow setting for this node.
         */
        [[nodiscard]] YGOverflow getOverflow() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetOverflow(_ygNode);
        }

        /**
         * @param display The new display mode of the node.
         */
        void setDisplay(const YGDisplay display) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetDisplay(_ygNode, display);
        }

        /**
         * @return The current display mode of the node.
         */
        [[nodiscard]] YGDisplay getDisplay() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetDisplay(_ygNode);
        }

        /**
         * @param flex The new flex value to be assigned to the node.
         */
        void setFlex(const float flex) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlex(_ygNode, flex);
        }

        /**
         * @return The flex value as a float.
         */
        [[nodiscard]] float getFlex() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetFlex(_ygNode);
        }

        /**
         * @param flexGrow The new flex grow for the node.
         */
        void setFlexGrow(const float flexGrow) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlexGrow(_ygNode, flexGrow);
        }

        /**
         * @return The flex grow factor of the node.
         */
        [[nodiscard]] float getFlexGrow() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetFlexGrow(_ygNode);
        }

        /**
         * @param flexShrink The flex shrink factor. A value of 0 means the node does not shrink.
         */
        void setFlexShrink(const float flexShrink) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlexShrink(_ygNode, flexShrink);
        }

        /**
         * @return The flex shrink factor of this node.
         */
        [[nodiscard]] float getFlexShrink() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetFlexShrink(_ygNode);
        }

        /**
         * @param flexBasis The value to set as the flex-basis for this node, in units (usually pixels).
         */
        void setFlexBasis(const float flexBasis) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlexBasis(_ygNode, flexBasis);
        }

        /**
         * @param flexBasisPercent The percentage value to set as the flex basis.
         */
        void setFlexBasisPercent(const float flexBasisPercent) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlexBasisPercent(_ygNode, flexBasisPercent);
        }

        /**
         * Sets the flex basis of this node to auto
         */
        void setFlexBasisAuto() noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetFlexBasisAuto(_ygNode);
        }

        /**
         * @return The flex basis value of the node
         */
        [[nodiscard]] YGValue getFlexBasis() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetFlexBasis(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetPosition(_ygNode, edge, position);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetPositionPercent(_ygNode, edge, positionPercent);
        }

        /**
         * Sets a node's edge to auto-position.
         *
         * @param edge The edge to set to auto-position.
         */
        void setPositionAuto(const YGEdge edge) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetPositionAuto(_ygNode, edge);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetPosition(_ygNode, edge);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMargin(_ygNode, edge, margin);
        }

        /**
         * Sets the margin percentage for the current context.
         * @param edge Which edge to set the margin for
         * @param marginPercent The margin value expressed as a percentage.
         */
        void setMarginPercent(const YGEdge edge, const float marginPercent) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMarginPercent(_ygNode, edge, marginPercent);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMarginAuto(_ygNode, edge);
        }

        /**
         * Retrieves the margin value and unit type for the current context.
         *
         * @param edge The edge for which to retrieve the margin.
         * @return The margin unit and value stored in a YGValue.
         */
        [[nodiscard]] YGValue getMargin(const YGEdge edge) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetMargin(_ygNode, edge);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetPadding(_ygNode, edge, padding);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetPaddingPercent(_ygNode, edge, paddingPercent);
        }

        /**
         * Retrieves the padding value and unit type of the current object.
         * @param edge The edge for which the padding is to be retrieved.
         * @return The padding value and unit type for the specified side, stored in a YGValue.
         */
        [[nodiscard]] YGValue getPadding(const YGEdge edge) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetPadding(_ygNode, edge);
        }

        /**
         * Sets the border width of a node's edge.
         *
         * @param edge The edge of the border
         * @param border The width of the border in units (usually pixels).
         */
        void setBorder(const YGEdge edge, const float border) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetBorder(_ygNode, edge, border);
        }

        /**
         * Retrieves the border width for an edge on this node.
         *
         * @param edge The edge for which the border information is to be retrieved.
         * @return The border width as a float.
         */
        [[nodiscard]] float getBorder(const YGEdge edge) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetBorder(_ygNode, edge);
        }

        /**
         * Sets the gap size for this node's column or row gutters (or both).
         * @param gutter Which gutter to resize: column, row, or all (YGGutter)
         * @param length The size of the gap in units (usually pixels).
         */
        void setGap(const YGGutter gutter, const float length) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetGap(_ygNode, gutter, length);
        }

        /**
         * Sets the gutter gap percentage for the current node.
         * @param gutter Which gutter to resize: column, row, or all (YGGutter)
         * @param lengthPercent The size of the gap as a percentage (0-100).
         */
        void setGapPercent(const YGGutter gutter, const float lengthPercent) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetGapPercent(_ygNode, gutter, lengthPercent);
        }

        /**
         * Gets the gap for the specified gutters on this node.
         * @param gutter Which gutter to get the gap size of
         * @return Gutter size
         */
        [[nodiscard]] float getGap(const YGGutter gutter) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetGap(_ygNode, gutter);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetBoxSizing(_ygNode, boxSizing);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetBoxSizing(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetWidth(_ygNode, width);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetWidthPercent(_ygNode, widthPercent);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetWidthAuto(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetWidth(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetHeight(_ygNode, height);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetHeightPercent(_ygNode, heightPercent);
        }

        /**
         * Automatically adjusts the height of this component based on its content.
         *
         * This method enables dynamic resizing to fit the contained data or layout,
         * ensuring proper presentation without manual height adjustments.
         */
        void setHeightAuto() noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetHeightAuto(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetHeight(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMinWidth(_ygNode, minWidth);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMinWidthPercent(_ygNode, minWidthPercent);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetMinWidth(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMinHeight(_ygNode, minHeight);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMinHeightPercent(_ygNode, minHeightPercent);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetMinHeight(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMaxWidth(_ygNode, maxWidth);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMaxWidthPercent(_ygNode, maxWidthPercent);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetMaxWidth(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMaxHeight(_ygNode, maxHeight);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetMaxHeightPercent(_ygNode, maxHeightPercent);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetMaxHeight(_ygNode);
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
            assert(_ygNode && "must be a valid node");
            YGNodeStyleSetAspectRatio(_ygNode, aspectRatio);
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
            assert(_ygNode && "must be a valid node");
            return YGNodeStyleGetAspectRatio(_ygNode);
        }

        /**
         * Retrieves the current context associated with this node.
         *
         * The context provides relevant state information or parameters.
         *
         * @return The current context of this node.
         */
        [[nodiscard]] std::optional<std::reference_wrapper<Ctx>> getContext() noexcept
        {
            assert(_ygNode && "must be a valid node");
            auto* ctxPtr = YGNodeGetContext(_ygNode);
            if (ctxPtr == nullptr)
            {
                return std::nullopt;
            }
            return std::optional<std::reference_wrapper<Ctx>>{std::ref(*static_cast<Ctx*>(ctxPtr))};
        }

        [[nodiscard]] std::optional<std::reference_wrapper<const Ctx>> getContext() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            auto* ctxPtr = YGNodeGetContext(_ygNode);
            if (ctxPtr == nullptr)
            {
                return std::nullopt;
            }
            return std::optional<std::reference_wrapper<const Ctx>>{std::cref(*static_cast<const Ctx*>(ctxPtr))};
        }

        /**
         * Retrieves a view of child nodes associated with this node.
         *
         * @return A view containing the child nodes of this node.
         */
        [[nodiscard]] ChildView getChildren() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return ChildView{*this};
        }

        /**
         * Retrieves the child node at the specified index, or a null (invalid) reference.
         *
         * @param index The position of the child node to retrieve.
         * @return The child node at the specified index. Can be a null reference.
         */
        [[nodiscard]] Node getChild(const size_t index) const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return Node{YGNodeGetChild(_ygNode, index)};
        }

        /**
         * Inserts a child node into the current node.
         *
         * The new child will be added to the list of children for this node.
         *
         * @param child The child node to insert.
         */
        void insertChild(const Node& child) noexcept
        {
            assert(_ygNode && "must be a valid node");
            if (child._ygNode == nullptr)
                return;
            YGNodeInsertChild(_ygNode, child._ygNode, getChildCount());
        }

        /**
         * Inserts a child node into the current node.
         *
         * The new child will be added to the list of children for this node.
         *
         * @param child The child node to insert.
         * @param index Which index to insert the child at
         */
        void insertChild(const Node& child, const size_t index) noexcept
        {
            assert(_ygNode && "must be a valid node");
            if (child._ygNode == nullptr)
                return;
            YGNodeInsertChild(_ygNode, child._ygNode, index);
        }

        /**
         * Removes a child node from this node.
         *
         * The child node specified will be detached from the current node.
         * If the specified node is not a child of this node, no action is taken.
         *
         * @param child The child node to be removed.
         */
        void removeChild(const Node& child) noexcept
        {
            assert(_ygNode && "must be a valid node");
            if (child._ygNode == nullptr)
                return;
            YGNodeRemoveChild(_ygNode, child._ygNode);
        }

        /**
         * Retrieves the parent node of this object, or a null (invalid) node if there is no parent.
         *
         * @return The parent node of this object or a null reference
         */
        Node getParent() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return Node{YGNodeGetParent(_ygNode)};
        }

        /**
         * @return A raw YGNodeRef, which is an opaque pointer.
         */
        [[nodiscard]] YGNodeRef getRef() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return _ygNode;
        }

        /**
         * @return A raw YGConstNodeRef, which is an opaque const pointer.
         */
        [[nodiscard]] YGNodeConstRef getConstRef() const noexcept
        {
            assert(_ygNode && "must be a valid node");
            return _ygNode;
        }

        template <DefaultConstructible ConfigCtx>
        void setConfig(Config<ConfigCtx>& config) noexcept
        {
            assert(_ygNode && "must be a valid node");
            YGNodeSetConfig(_ygNode, config.getRef());
        }

    protected:
        YGNodeRef _ygNode;

        void setContext(Ctx* context) { YGNodeSetContext(_ygNode, context); }
    };

    /**
     * An owning handle to a Yoga node. You probably want Yoga::Layout!
     *
     * Inherits functionality from Yoga::Node.
     *
     * @tparam Ctx Context type stored per node - must be default constructible
     */
    template <DefaultConstructible Ctx = std::monostate>
    class OwningNode final : public Node<Ctx>
    {

        using base = Node<Ctx>;

    public:
        static constexpr bool isOwning = true;

        OwningNode() : base{YGNodeNew()} { this->setContext(&_context); }

        template <DefaultConstructible ConfigCtx>
        explicit OwningNode(const Config<ConfigCtx>& config) : base{YGNodeNewWithConfig(config.getRef())}
        {
            this->setContext(&_context);
        }

        OwningNode(const OwningNode& other) = delete;
        OwningNode& operator=(const OwningNode& other) = delete;

        OwningNode(OwningNode&& other) noexcept : base{other._ygNode}, _context{std::move(other._context)}
        {
            other._ygNode = nullptr;
            if (this->_ygNode != nullptr)
            {
                this->setContext(&_context);
            }
        }

        OwningNode& operator=(OwningNode&& other) noexcept
        {
            if (this != &other)
            {
                if (this->_ygNode != nullptr)
                {
                    YGNodeFree(this->_ygNode);
                }
                this->_ygNode = other._ygNode;
                other._ygNode = nullptr;
                _context = std::move(other._context);
                if (this->_ygNode != nullptr)
                {
                    this->setContext(&_context);
                }
            }
            return *this;
        }

        ~OwningNode()
        {
            if (this->_ygNode != nullptr)
            {
                YGNodeFree(this->_ygNode);
            }
        }

    private:
        Ctx _context;
    };

    /**
     * Layout manager for Yoga nodes with automatic lifetime management.
     *
     * Manages both the underlying YGNode instances and their associated contexts.
     *
     * @tparam Ctx Context type stored per node - must be default constructible
     */
    template <DefaultConstructible Ctx, DefaultConstructible ConfigCtx = std::monostate>
    class Layout
    {
    public:
        /**
         * Creates a new layout with a root node sized to 100% width/height
         */
        Layout() : _root{nullptr}
        {
            _config.setPointScaleFactor(1.f);
            _root = Node<Ctx>{_nodes.emplace_back(_config).getRef()};
            _root.setWidthPercent(100.f);
            _root.setHeightPercent(100.f);
        }

        Layout(const Layout& other) = delete;
        Layout& operator=(const Layout& other) = delete;

        Layout(Layout&&) = default;
        Layout& operator=(Layout&&) = default;

        /**
         * Creates a new node managed by this layout.
         *
         * The node will be automatically cleaned up when the layout is destroyed.
         *
         * @return Non-owning Node handle for the created node
         */
        Node<Ctx> createNode()
        {
            auto& node = _nodes.emplace_back(_config);
            return Node<Ctx>{node.getRef()};
        }

        /**
         * Removes a node and all of its children from this layout.
         *
         * Also removes it from its parent and cleans up associated context.
         *
         * @param node The node to remove
         */
        void removeNode(const Node<Ctx>& node)
        {
            auto nodeRef = node.getRef();

            assert(_nodes.front().getRef() != nodeRef && "Cannot remove the root");

            if (auto parent = node.getParent(); parent)
            {
                parent.removeChild(node);
            }

            for (auto child : node.getChildren())
            {
                removeNode(child);
            }

            std::erase_if(_nodes, [nodeRef](const OwningNode<Ctx>& owned) { return owned.getRef() == nodeRef; });
        }

        /**
         * Add an existing node to the layout root.
         *
         * A node tree will not be part of the layout unless
         * it is parented to the root.
         * @param node The node to add
         */
        void addToRoot(const Node<Ctx>& node) { _root.insertChild(node); }

        Node<Ctx> getRoot() { return Node{_root.getRef()}; }

        [[nodiscard]] Config<ConfigCtx>& getConfig() { return _config; }
        [[nodiscard]] const Config<ConfigCtx>& getConfig() const { return _config; }

        /**
         * Calculates the entire layout tree starting from this node.
         * @param width Available width the layout can take up
         * @param height Available height the layout can take up
         * @param direction Reading direction (left-to-right by default)
         */
        void calculate(const float width, const float height, const YGDirection direction = YGDirectionLTR)
        {
            _root.calculateLayout(width, height, direction);
        }

        /**
         * Walks the layout tree, invoking a visitor for each node
         * in pre-order (parent, then children).
         * @tparam Callable Any callable that accepts a const Node<Ctx>&
         * @param visitor The callable to invoke for each node.
         */
        template<std::invocable<Node<Ctx>&> Callable>
        void walkTree(Callable&& visitor) const
        {
            walkNode(getRoot(), visitor);
        }
    private:
        Config<ConfigCtx> _config;
        std::vector<OwningNode<Ctx>> _nodes;
        Node<Ctx> _root;

        /**
         * Walks the layout tree, invoking a visitor for each node
         * in pre-order (parent, then children).
         * @tparam Callable Any callable that accepts a const Node<Ctx>&
         * @param node The node to start walking from.
         * @param visitor The callable to invoke for each node.
         */
        template<std::invocable<Node<Ctx>&> Callable>
        void walkNode(Node<Ctx>& node, Callable& visitor) const
        {
            visitor(node);

            for (auto child : node.getChildren())
            {
                walkNode(child, visitor);
            }
        }

    };
} // namespace Yoga
