#pragma once

#include <boost/iterator/iterator_facade.hpp>

#include <memory>

#include "ns_type.hpp"
#include "base_ns_decoder.hpp"

#include "types.hpp"

namespace opossum {

class BaseNsVector : private Noncopyable {
 public:
  virtual ~BaseNsVector() = default;

  virtual size_t size() const = 0;
  virtual size_t data_size() const = 0;

  virtual NsType type() const = 0;

  virtual pmr_vector<uint32_t> decode() const = 0;

  virtual std::unique_ptr<BaseNsDecoder> create_base_decoder() const = 0;
};

/**
 * You may use this iterator facade to implement iterators returned by NsVector::cbegin() and NsVector::cend()
 */
template <typename Derived>
using BaseNsIterator = boost::iterator_facade<Derived, uint32_t, boost::forward_traversal_tag, uint32_t>;

/**
 * @brief Implements the non-virtual interface of all vectors
 */
template <typename Derived>
class NsVector : public BaseNsVector {
 public:
  /**
   * @defgroup Virtual interface implementation
   * @{
   */

  size_t size() const final { return _self()._on_size(); }
  size_t data_size() const final { return _self()._on_data_size(); }

  virtual NsType type() const final { return get_ns_type<Derived>(); }

  pmr_vector<uint32_t> decode() const final {
    auto decoded_vector = pmr_vector<uint32_t>{};
    decoded_vector.reserve(size());

    auto it = cbegin();
    const auto end = cend();
    for (; it != end; ++it) {
      decoded_vector.push_back(*it);
    }

    return decoded_vector;
  }

  virtual std::unique_ptr<BaseNsDecoder> create_base_decoder() const final { return _self()._on_create_base_decoder(); }
  /**@}*/

 public:
  /**
   * @defgroup Non-virtual interface
   * @{
   */

  /**
   * @brief Returns a vector specific decoder
   * @return a unique_ptr of subclass of BaseNsDecoder
   */
  auto create_decoder() const { return _self()._on_create_decoder(); }

  /**
   * @brief Returns an iterator to the beginning
   * @return a constant forward iterator returning uint32_t
   */
  auto cbegin() const { return _self()._on_cbegin(); }

  /**
   * @brief Returns an iterator to the end
   * @return a constant forward iterator returning uint32_t
   */
  auto cend() const { return _self()._on_cend(); }
  /**@}*/

 private:
  const Derived& _self() const { return static_cast<const Derived&>(*this); }
};

}  // namespace opossum