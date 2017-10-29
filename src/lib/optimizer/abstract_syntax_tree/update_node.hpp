#pragma once

#include <memory>
#include <string>
#include <vector>

#include "optimizer/abstract_syntax_tree/abstract_ast_node.hpp"

namespace opossum {

/**
 * Node type to represent updates (i.e., invalidation and inserts) in a table.
 */
class UpdateNode : public AbstractASTNode {
 public:
  explicit UpdateNode(const std::string& table_name,
                      const std::vector<std::shared_ptr<Expression>>& column_expressions);

  std::string description() const override;

  const std::string& table_name() const;

  const std::vector<std::shared_ptr<Expression>>& column_expressions() const;

 protected:
  const std::string _table_name;
  const std::vector<std::shared_ptr<Expression>> _column_expressions;

  void map_column_ids(const ColumnIDMapping &column_id_mapping,
                                   const std::optional<ASTChildSide> &caller_child_side = std::nullopt) override;
};

}  // namespace opossum
