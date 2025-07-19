#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>

#include "yoga-cpp/yoga.hpp"

// A simple context struct for testing purposes.
struct TestContext {
    int id = 0;
    std::string name;

    TestContext() = default;
    TestContext(int id, std::string name) : id(id), name(std::move(name)) {}
};


// Define types for convenience
using TestLayout = Yoga::Layout<TestContext>;
using TestNode = Yoga::Node<TestContext>;

// Test Suite for Layout and Node Lifetime Management
class LayoutLifetimeTest : public ::testing::Test {
protected:
    TestLayout layout;
};

TEST_F(LayoutLifetimeTest, NodeCreationAndDestruction) {
    ASSERT_NO_THROW({
        TestNode node = layout.createNode();
        EXPECT_TRUE(node.valid());

        layout.destroyNode(node);
        EXPECT_FALSE(node.valid());
    });
}

TEST_F(LayoutLifetimeTest, ContextCreationAndAccess) {
    TestNode node = layout.createNode(42, "MyNode");

    ASSERT_TRUE(node.valid());

    // Check if the context was constructed correctly
    TestContext& context = node.getContext();
    EXPECT_EQ(context.id, 42);
    EXPECT_EQ(context.name, "MyNode");

    // Modify the context and check if it persists
    context.id = 100;
    EXPECT_EQ(node.getContext().id, 100);
}

// Test Suite for Child Management API
class NodeChildManagementTest : public ::testing::Test {
protected:
    TestLayout layout;
    TestNode parent;

    void SetUp() override {
        parent = layout.createNode(1, "Parent");
    }
};

TEST_F(NodeChildManagementTest, InsertAndRemoveChild) {
    TestNode child = layout.createNode(2, "Child");

    EXPECT_EQ(parent.getChildCount(), 0);

    parent.insertChild(child, 0);
    EXPECT_EQ(parent.getChildCount(), 1);
    EXPECT_EQ(parent.getChild(0), child);

    parent.removeChild(child);
    EXPECT_EQ(parent.getChildCount(), 0);

    // The child node should still be valid, just detached
    EXPECT_TRUE(child.valid());
}

TEST_F(NodeChildManagementTest, CreateChildConvenience) {
    EXPECT_EQ(parent.getChildCount(), 0);

    TestNode newChild = parent.createChild(10, "CreatedChild");

    EXPECT_EQ(parent.getChildCount(), 1);
    EXPECT_TRUE(newChild.valid());
    EXPECT_EQ(newChild.getContext().id, 10);
    EXPECT_EQ(newChild.getContext().name, "CreatedChild");
    EXPECT_EQ(parent.getChild(0), newChild);
}

TEST_F(NodeChildManagementTest, ChildIteration) {
    std::vector<TestNode> children;
    children.push_back(parent.createChild(10, "Child1"));
    children.push_back(parent.createChild(20, "Child2"));
    children.push_back(parent.createChild(30, "Child3"));

    EXPECT_EQ(parent.getChildCount(), 3);

    int iteratedCount = 0;
    size_t childIndex = 0;
    for (const auto& iteratedChild : parent.getChildren()) {
        EXPECT_TRUE(iteratedChild.valid());
        EXPECT_EQ(iteratedChild, children[childIndex]);
        iteratedCount++;
        childIndex++;
    }

    EXPECT_EQ(iteratedCount, 3);
}

TEST_F(NodeChildManagementTest, GetParent) {
    TestNode child = parent.createChild();

    ASSERT_TRUE(child.getParent().valid());
    EXPECT_EQ(child.getParent(), parent);
}

class LayoutCalculationTest : public ::testing::Test {
protected:
    TestLayout layout;
};

TEST_F(LayoutCalculationTest, SimpleFlexLayout) {
    TestNode root = layout.createNode();
    root.setFlexDirection(YGFlexDirectionRow);
    root.setWidth(500.f);
    root.setHeight(100.f);

    TestNode child1 = layout.createNode();
    child1.setFlexGrow(1.f);

    TestNode child2 = layout.createNode();
    child2.setFlexGrow(1.f);

    root.insertChild(child1, 0);
    root.insertChild(child2, 1);

    root.calculateLayout(500.f, 100.f);

    EXPECT_FLOAT_EQ(child1.getLayoutLeft(), 0);
    EXPECT_FLOAT_EQ(child1.getLayoutTop(), 0);
    EXPECT_FLOAT_EQ(child1.getLayoutWidth(), 250.f);
    EXPECT_FLOAT_EQ(child1.getLayoutHeight(), 100.f);

    EXPECT_FLOAT_EQ(child2.getLayoutLeft(), 250.f);
    EXPECT_FLOAT_EQ(child2.getLayoutTop(), 0);
    EXPECT_FLOAT_EQ(child2.getLayoutWidth(), 250.f);
    EXPECT_FLOAT_EQ(child2.getLayoutHeight(), 100.f);
}
