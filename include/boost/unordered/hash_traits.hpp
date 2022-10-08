/* Hash function characterization.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/unordered for library home page.
 */

#ifndef BOOST_UNORDERED_HASH_TRAITS_HPP
#define BOOST_UNORDERED_HASH_TRAITS_HPP

#include <boost/type_traits/make_void.hpp>
#include <type_traits>

namespace boost{
namespace unordered{

namespace detail{

template<typename Hash,typename=void>
struct hash_is_avalanching
{
  using type=std::false_type;
};

template<typename Hash>
struct hash_is_avalanching<Hash,void_t<typename Hash::is_avalanching>>
{
  using type=std::true_type;
};

} /* namespace detail */

/* Partially specializable by users for concrete hash functions when
 * actual characterization differs from default.
 */

template<typename Hash>
struct hash_traits
{
  /* std::true_type if the type Hash::is_avalanching is present,
   * std::false_type otherwise.
   */
  using is_avalanching=typename detail::hash_is_avalanching<Hash>::type;
};

} /* namespace unordered */
} /* namespace boost */

#endif
