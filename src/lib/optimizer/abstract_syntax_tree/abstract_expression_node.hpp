#pragma once

#include <memory>
#include <string>
#include <vector>

#include "types.hpp"

namespace opossum {

class AbstractExpressionNode : public std::enable_shared_from_this<AbstractExpressionNode> {
 public:
  explicit AbstractExpressionNode(ExpressionType type);

  const std::weak_ptr<AbstractExpressionNode> &parent() const;
  void set_parent(const std::weak_ptr<AbstractExpressionNode> &parent);

  const std::shared_ptr<AbstractExpressionNode> &left() const;
  void set_left(const std::shared_ptr<AbstractExpressionNode> &left);

  const std::shared_ptr<AbstractExpressionNode> &right() const;
  void set_right(const std::shared_ptr<AbstractExpressionNode> &right);

  const ExpressionType type() const;

  void print(const uint8_t indent = 0) const;
  virtual std::string description() const = 0;

 protected:
  ExpressionType _type;

 private:
  std::weak_ptr<AbstractExpressionNode> _parent;
  std::shared_ptr<AbstractExpressionNode> _left;
  std::shared_ptr<AbstractExpressionNode> _right;
};

}  // namespace opossum
