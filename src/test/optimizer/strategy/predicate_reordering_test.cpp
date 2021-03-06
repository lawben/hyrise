#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../base_test.hpp"
#include "gtest/gtest.h"

#include "abstract_expression.hpp"
#include "logical_query_plan/join_node.hpp"
#include "logical_query_plan/lqp_column_reference.hpp"
#include "logical_query_plan/predicate_node.hpp"
#include "logical_query_plan/projection_node.hpp"
#include "logical_query_plan/sort_node.hpp"
#include "logical_query_plan/stored_table_node.hpp"
#include "logical_query_plan/union_node.hpp"
#include "optimizer/column_statistics.hpp"
#include "optimizer/strategy/predicate_reordering_rule.hpp"
#include "optimizer/strategy/strategy_base_test.hpp"
#include "optimizer/table_statistics.hpp"
#include "storage/storage_manager.hpp"

#include "utils/assert.hpp"

#include "logical_query_plan/mock_node.hpp"

namespace opossum {

class TableStatisticsMock : public TableStatistics {
 public:
  // we don't need a shared_ptr<Table> for this mock, so just set a nullptr
  TableStatisticsMock() : TableStatistics(std::make_shared<Table>()) { _row_count = 0; }

  explicit TableStatisticsMock(float row_count) : TableStatistics(std::make_shared<Table>()) { _row_count = row_count; }

  std::shared_ptr<TableStatistics> predicate_statistics(const ColumnID column_id,
                                                        const PredicateCondition predicate_condition,
                                                        const AllParameterVariant& value,
                                                        const std::optional<AllTypeVariant>& value2) override {
    if (column_id == ColumnID{0}) {
      return std::make_shared<TableStatisticsMock>(500);
    }
    if (column_id == ColumnID{1}) {
      return std::make_shared<TableStatisticsMock>(200);
    }
    if (column_id == ColumnID{2}) {
      return std::make_shared<TableStatisticsMock>(950);
    }

    Fail("Tried to access TableStatisticsMock with unexpected column");
  }
};

class PredicateReorderingTest : public StrategyBaseTest {
 protected:
  void SetUp() override {
    StorageManager::get().add_table("a", load_table("src/test/tables/int_int_int.tbl", Chunk::MAX_SIZE));
    _rule = std::make_shared<PredicateReorderingRule>();

    std::vector<std::shared_ptr<BaseColumnStatistics>> column_statistics(
        {std::make_shared<ColumnStatistics<int32_t>>(ColumnID{0}, 20, 10, 100),
         std::make_shared<ColumnStatistics<int32_t>>(ColumnID{1}, 5, 50, 60),
         std::make_shared<ColumnStatistics<int32_t>>(ColumnID{2}, 2, 110, 1100)});

    auto table_statistics = std::make_shared<TableStatistics>(100, column_statistics);

    _mock_node = MockNode::make(table_statistics);

    _mock_node_a = LQPColumnReference{_mock_node, ColumnID{0}};
    _mock_node_b = LQPColumnReference{_mock_node, ColumnID{1}};
    _mock_node_c = LQPColumnReference{_mock_node, ColumnID{2}};
  }

  std::shared_ptr<MockNode> _mock_node;
  LQPColumnReference _mock_node_a, _mock_node_b, _mock_node_c;
  std::shared_ptr<PredicateReorderingRule> _rule;
};

TEST_F(PredicateReorderingTest, SimpleReorderingTest) {
  // clang-format off
  const auto input_lqp =
  PredicateNode::make(_mock_node_a, PredicateCondition::GreaterThan, 10,
    PredicateNode::make(_mock_node_a, PredicateCondition::GreaterThan, 50,
      _mock_node));
  // clang-format on

  auto stored_table_node = StoredTableNode::make("a");

  auto statistics_mock = std::make_shared<TableStatisticsMock>();
  stored_table_node->set_statistics(statistics_mock);

  auto predicate_node_0 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{0}}, PredicateCondition::GreaterThan, 10);
  predicate_node_0->set_left_child(stored_table_node);

  auto predicate_node_1 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{1}}, PredicateCondition::GreaterThan, 50);
  predicate_node_1->set_left_child(predicate_node_0);

  auto reordered = StrategyBaseTest::apply_rule(_rule, predicate_node_1);

  EXPECT_EQ(reordered, predicate_node_0);
  EXPECT_EQ(reordered->left_child(), predicate_node_1);
  EXPECT_EQ(reordered->left_child()->left_child(), stored_table_node);
}

TEST_F(PredicateReorderingTest, MoreComplexReorderingTest) {
  auto stored_table_node = StoredTableNode::make("a");

  auto statistics_mock = std::make_shared<TableStatisticsMock>();
  stored_table_node->set_statistics(statistics_mock);

  auto predicate_node_0 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{0}}, PredicateCondition::GreaterThan, 5);
  predicate_node_0->set_left_child(stored_table_node);

  auto predicate_node_1 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{1}}, PredicateCondition::GreaterThan, 1);
  predicate_node_1->set_left_child(predicate_node_0);

  auto predicate_node_2 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{2}}, PredicateCondition::GreaterThan, 9);
  predicate_node_2->set_left_child(predicate_node_1);

  auto reordered = StrategyBaseTest::apply_rule(_rule, predicate_node_2);
  EXPECT_EQ(reordered, predicate_node_2);
  EXPECT_EQ(reordered->left_child(), predicate_node_0);
  EXPECT_EQ(reordered->left_child()->left_child(), predicate_node_1);
  EXPECT_EQ(reordered->left_child()->left_child()->left_child(), stored_table_node);
}

TEST_F(PredicateReorderingTest, ComplexReorderingTest) {
  // clang-format off
  const auto input_lqp =
  PredicateNode::make(_mock_node_a, PredicateCondition::Equals, 42,
    PredicateNode::make(_mock_node_b, PredicateCondition::GreaterThan, 50,
      PredicateNode::make(_mock_node_b, PredicateCondition::GreaterThan, 40,
        ProjectionNode::make_pass_through(
          PredicateNode::make(_mock_node_a, PredicateCondition::GreaterThanEquals, 90,
            PredicateNode::make(_mock_node_c, PredicateCondition::LessThan, 500,
              _mock_node))))));


  const auto expected_optimized_lqp =
  PredicateNode::make(_mock_node_b, PredicateCondition::GreaterThan, 40,
    PredicateNode::make(_mock_node_b, PredicateCondition::GreaterThan, 50,
      PredicateNode::make(_mock_node_a, PredicateCondition::Equals, 42,
        ProjectionNode::make_pass_through(
          PredicateNode::make(_mock_node_c, PredicateCondition::LessThan, 500,
            PredicateNode::make(_mock_node_a, PredicateCondition::GreaterThanEquals, 90,
              _mock_node))))));
  // clang-format on

  const auto reordered_input_lqp = StrategyBaseTest::apply_rule(_rule, input_lqp);
  EXPECT_LQP_EQ(reordered_input_lqp, expected_optimized_lqp);
}

TEST_F(PredicateReorderingTest, TwoReorderings) {
  auto stored_table_node = StoredTableNode::make("a");

  auto statistics_mock = std::make_shared<TableStatisticsMock>();
  stored_table_node->set_statistics(statistics_mock);

  auto predicate_node_0 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{0}}, PredicateCondition::GreaterThan, 10);
  predicate_node_0->set_left_child(stored_table_node);

  auto predicate_node_1 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{1}}, PredicateCondition::GreaterThan, 50);
  predicate_node_1->set_left_child(predicate_node_0);

  auto sort_node = SortNode::make(
      std::vector<OrderByDefinition>{{LQPColumnReference{stored_table_node, ColumnID{0}}, OrderByMode::Ascending}});
  sort_node->set_left_child(predicate_node_1);

  auto predicate_node_2 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{2}}, PredicateCondition::GreaterThan, 90);
  predicate_node_2->set_left_child(sort_node);

  auto predicate_node_3 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{1}}, PredicateCondition::GreaterThan, 50);
  predicate_node_3->set_left_child(predicate_node_2);

  const auto& expressions = LQPExpression::create_columns(
      {LQPColumnReference{stored_table_node, ColumnID{0}}, LQPColumnReference{stored_table_node, ColumnID{1}}});
  const auto projection_node = ProjectionNode::make(expressions);
  projection_node->set_left_child(predicate_node_3);

  auto reordered = StrategyBaseTest::apply_rule(_rule, projection_node);

  EXPECT_EQ(reordered, projection_node);
  EXPECT_EQ(reordered->left_child(), predicate_node_2);
  EXPECT_EQ(reordered->left_child()->left_child(), predicate_node_3);
  EXPECT_EQ(reordered->left_child()->left_child()->left_child(), sort_node);
  EXPECT_EQ(reordered->left_child()->left_child()->left_child()->left_child(), predicate_node_0);
  EXPECT_EQ(reordered->left_child()->left_child()->left_child()->left_child()->left_child(), predicate_node_1);
  EXPECT_EQ(reordered->left_child()->left_child()->left_child()->left_child()->left_child()->left_child(),
            stored_table_node);
}

TEST_F(PredicateReorderingTest, SameOrderingForStoredTable) {
  std::shared_ptr<Table> table_a = load_table("src/test/tables/int_float4.tbl", 2);
  StorageManager::get().add_table("table_a", std::move(table_a));

  auto stored_table_node = StoredTableNode::make("table_a");

  // Setup first LQP
  // predicate_node_1 -> predicate_node_0 -> stored_table_node
  auto predicate_node_0 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{0}}, PredicateCondition::LessThan, 20);
  predicate_node_0->set_left_child(stored_table_node);

  auto predicate_node_1 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{0}}, PredicateCondition::LessThan, 40);
  predicate_node_1->set_left_child(predicate_node_0);

  predicate_node_1->get_statistics();

  auto reordered = StrategyBaseTest::apply_rule(_rule, predicate_node_1);

  // Setup second LQP
  // predicate_node_3 -> predicate_node_2 -> stored_table_node
  auto predicate_node_2 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{0}}, PredicateCondition::LessThan, 40);
  predicate_node_2->set_left_child(stored_table_node);

  auto predicate_node_3 =
      PredicateNode::make(LQPColumnReference{stored_table_node, ColumnID{0}}, PredicateCondition::LessThan, 20);
  predicate_node_3->set_left_child(predicate_node_2);

  auto reordered_1 = StrategyBaseTest::apply_rule(_rule, predicate_node_3);

  EXPECT_EQ(reordered, predicate_node_1);
  EXPECT_EQ(reordered->left_child(), predicate_node_0);
  EXPECT_EQ(reordered_1, predicate_node_2);
  EXPECT_EQ(reordered_1->left_child(), predicate_node_3);
}

TEST_F(PredicateReorderingTest, PredicatesAsRightChild) {
  /**
   * Check that Reordering predicates works if a predicate chain is both on the left and right side of a node.
   * This is particularly interesting because the PredicateReorderingRule needs to re-attach the ordered chain of
   * predicates to the parent (the cross node in this case). This test checks whether the attachment happens as the
   * correct child.
   *
   *             _______Cross________
   *            /                    \
   *  Predicate_0(a > 80)     Predicate_2(a > 90)
   *           |                     |
   *  Predicate_1(a > 60)     Predicate_3(a > 50)
   *           |                     |
   *        Table_0           Predicate_4(a > 30)
   *                                 |
   *                               Table_1
   */

  /**
   * The mocked table has one column of int32_ts with the value range 0..100
   */
  auto column_statistics = std::make_shared<ColumnStatistics<int32_t>>(ColumnID{0}, 100.0f, 0.0f, 100.0f);
  auto table_statistics =
      std::make_shared<TableStatistics>(100, std::vector<std::shared_ptr<BaseColumnStatistics>>{column_statistics});

  auto table_0 = MockNode::make(table_statistics);
  auto table_1 = MockNode::make(table_statistics);
  auto cross_node = JoinNode::make(JoinMode::Cross);
  auto predicate_0 = PredicateNode::make(LQPColumnReference{table_0, ColumnID{0}}, PredicateCondition::GreaterThan, 80);
  auto predicate_1 = PredicateNode::make(LQPColumnReference{table_0, ColumnID{0}}, PredicateCondition::GreaterThan, 60);
  auto predicate_2 = PredicateNode::make(LQPColumnReference{table_1, ColumnID{0}}, PredicateCondition::GreaterThan, 90);
  auto predicate_3 = PredicateNode::make(LQPColumnReference{table_1, ColumnID{0}}, PredicateCondition::GreaterThan, 50);
  auto predicate_4 = PredicateNode::make(LQPColumnReference{table_1, ColumnID{0}}, PredicateCondition::GreaterThan, 30);

  predicate_1->set_left_child(table_0);
  predicate_0->set_left_child(predicate_1);
  predicate_4->set_left_child(table_1);
  predicate_3->set_left_child(predicate_4);
  predicate_2->set_left_child(predicate_3);
  cross_node->set_left_child(predicate_0);
  cross_node->set_right_child(predicate_2);

  const auto reordered = StrategyBaseTest::apply_rule(_rule, cross_node);

  EXPECT_EQ(reordered, cross_node);
  EXPECT_EQ(reordered->left_child(), predicate_1);
  EXPECT_EQ(reordered->left_child()->left_child(), predicate_0);
  EXPECT_EQ(reordered->left_child()->left_child()->left_child(), table_0);
  EXPECT_EQ(reordered->right_child(), predicate_4);
  EXPECT_EQ(reordered->right_child()->left_child(), predicate_3);
  EXPECT_EQ(reordered->right_child()->left_child()->left_child(), predicate_2);
}

TEST_F(PredicateReorderingTest, PredicatesWithMultipleParents) {
  /**
   * If a PredicateNode has multiple parents, it should not be considered for reordering
   */
  /**
   *      _____Union___
   *    /             /
   * Predicate_a     /
   *    \           /
   *     Predicate_b
   *         |
   *       Table
   *
   * predicate_a should come before predicate_b - but since Predicate_b has two parents, it can't be reordered
   */

  /**
   * The mocked table has one column of int32_ts with the value range 0..100
   */
  auto column_statistics = std::make_shared<ColumnStatistics<int32_t>>(ColumnID{0}, 100.0f, 0.0f, 100.0f);
  auto table_statistics =
      std::make_shared<TableStatistics>(100, std::vector<std::shared_ptr<BaseColumnStatistics>>{column_statistics});

  auto table_node = MockNode::make(table_statistics);
  auto union_node = UnionNode::make(UnionMode::Positions);
  auto predicate_a_node =
      PredicateNode::make(LQPColumnReference{table_node, ColumnID{0}}, PredicateCondition::GreaterThan, 90);
  auto predicate_b_node =
      PredicateNode::make(LQPColumnReference{table_node, ColumnID{0}}, PredicateCondition::GreaterThan, 10);

  union_node->set_left_child(predicate_a_node);
  union_node->set_right_child(predicate_b_node);
  predicate_a_node->set_left_child(predicate_b_node);
  predicate_b_node->set_left_child(table_node);

  const auto reordered = StrategyBaseTest::apply_rule(_rule, union_node);

  EXPECT_EQ(reordered, union_node);
  EXPECT_EQ(reordered->left_child(), predicate_a_node);
  EXPECT_EQ(reordered->right_child(), predicate_b_node);
  EXPECT_EQ(predicate_a_node->left_child(), predicate_b_node);
  EXPECT_EQ(predicate_b_node->left_child(), table_node);
}

}  // namespace opossum
