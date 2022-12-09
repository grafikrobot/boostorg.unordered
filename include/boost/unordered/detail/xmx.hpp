/* 32b/64b xmx mix function.
 *
 * Copyright 2022 Peter Dimov.
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/unordered for library home page.
 */

#ifndef BOOST_UNORDERED_DETAIL_XMX_HPP
#define BOOST_UNORDERED_DETAIL_XMX_HPP

#include <boost/cstdint.hpp>
#include <climits>
#include <cstddef>

namespace boost{
namespace unordered{
namespace detail{

/* Bit mixer for improvement of statistical properties of hash functions.
 * The implementation is different on 64bit and 32bit architectures:
 * 
 *   - 64bit: same as xmx function in
 *     http://jonkagstrom.com/bit-mixer-construction/index.html
 *   - 32bit: generated by Hash Function Prospector
 *     (https://github.com/skeeto/hash-prospector) and selected as the
 *     best overall performer in benchmarks of Boost.Unordered flat containers.
 *     Score assigned by Hash Prospector: 333.7934929677524
 */

#if defined(SIZE_MAX)
#if ((((SIZE_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define BOOST_UNORDERED_64B_ARCHITECTURE /* >64 bits assumed as 64 bits */
#endif
#elif defined(UINTPTR_MAX) /* used as proxy for std::size_t */
#if ((((UINTPTR_MAX >> 16) >> 16) >> 16) >> 15) != 0
#define BOOST_UNORDERED_64B_ARCHITECTURE
#endif
#endif

static inline std::size_t xmx(std::size_t x)noexcept
{
#if defined(BOOST_UNORDERED_64B_ARCHITECTURE)

  boost::uint64_t z=(boost::uint64_t)x;

  z^=z>>23;
  z*=0xff51afd7ed558ccdull;
  z^=z>>23;

  return (std::size_t)z;

#else /* 32 bits assumed */

  x^=x>>18;
  x*=0x56b5aaadu;
  x^=x>>16;

  return x;

#endif
}

// alternative multipliers

static inline std::size_t xmx2( std::size_t x ) noexcept
{
#if defined(BOOST_UNORDERED_64B_ARCHITECTURE)

  boost::uint64_t z=(boost::uint64_t)x;

  z ^= z >> 23;
  z *= 0x9E3779B97F4A7C15ull;
  z ^= z >> 23;

  return (std::size_t)z;

#else /* 32 bits assumed */

  x ^= x >> 18;
  x *= 0x9E3779B9u;
  x ^= x >> 16;

  return x;

#endif
}

#ifdef BOOST_UNORDERED_64B_ARCHITECTURE
#undef BOOST_UNORDERED_64B_ARCHITECTURE
#endif

} /* namespace detail */
} /* namespace unordered */
} /* namespace boost */

#endif
