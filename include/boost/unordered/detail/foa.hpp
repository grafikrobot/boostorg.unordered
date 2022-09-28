/* Fast open-addressing hash table.
 *
 * Copyright 2022 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://www.boost.org/libs/unordered for library home page.
 */

#ifndef BOOST_UNORDERED_DETAIL_FOA_HPP
#define BOOST_UNORDERED_DETAIL_FOA_HPP

#include <boost/assert.hpp>
#include <boost/config.hpp>
#include <boost/core/bit.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/core/pointer_traits.hpp>
#include <boost/cstdint.hpp>
#include <boost/predef.h>
#include <boost/type_traits/is_nothrow_swappable.hpp>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#if defined(__SSE2__)||\
    defined(_M_X64)||(defined(_M_IX86_FP)&&_M_IX86_FP>=2)
#define BOOST_UNORDERED_SSE2
#include <emmintrin.h>
#elif defined(__ARM_NEON)&&!defined(__ARM_BIG_ENDIAN)
#define BOOST_UNORDERED_LITTLE_ENDIAN_NEON
#include <arm_neon.h>
#endif

#ifdef __has_builtin
#define BOOST_UNORDERED_HAS_BUILTIN(x) __has_builtin(x)
#else
#define BOOST_UNORDERED_HAS_BUILTIN(x) 0
#endif

#if !defined(NDEBUG)
#define BOOST_UNORDERED_ASSUME(cond) BOOST_ASSERT(cond)
#elif BOOST_UNORDERED_HAS_BUILTIN(__builtin_assume)
#define BOOST_UNORDERED_ASSUME(cond) __builtin_assume(cond)
#elif defined(__GNUC__) || BOOST_UNORDERED_HAS_BUILTIN(__builtin_unreachable)
#define BOOST_UNORDERED_ASSUME(cond)    \
  do{                                   \
    if(!(cond))__builtin_unreachable(); \
  }while(0)
#elif defined(_MSC_VER)
#define BOOST_UNORDERED_ASSUME(cond) __assume(cond)
#else
#define BOOST_UNORDERED_ASSUME(cond)  \
  do{                                 \
    static_cast<void>(false&&(cond)); \
  }while(0)
#endif

namespace boost{
namespace unordered{
namespace detail{
namespace foa{

/* TODO: description */

#ifdef BOOST_UNORDERED_SSE2

struct group15
{
  static constexpr int N=15;

  struct dummy_group_type
  {
    alignas(16) unsigned char storage[N+1]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0};
  };

  inline void set(std::size_t pos,std::size_t hash)
  {
    BOOST_ASSERT(pos<N);
    at(pos)=adjust_hash(hash);
  }

  inline void set_sentinel()
  {
    at(N-1)=sentinel_;
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    BOOST_ASSERT(pos<N);
    return at(pos)==sentinel_;
  }

  inline void reset(std::size_t pos)
  {
    BOOST_ASSERT(pos<N);
    at(pos)=available_;
  }

  static inline void reset(unsigned char* pc)
  {
    *pc=available_;
  }

  inline int match(std::size_t hash)const
  {
    return _mm_movemask_epi8(
      _mm_cmpeq_epi8(m,_mm_set1_epi32(match_word(hash))))&0x7FFF;
  }

  inline bool is_not_overflowed(std::size_t hash)const
  {
    static constexpr unsigned char shift[]={1,2,4,8,16,32,64,128};

    return !(overflow()&shift[hash%8]);
  }

  inline void mark_overflow(std::size_t hash)
  {
    overflow()|=static_cast<unsigned char>(1<<(hash%8));
  }

  inline int match_available()const
  {
    return _mm_movemask_epi8(
      _mm_cmpeq_epi8(m,_mm_setzero_si128()))&0x7FFF;
  }

  inline int match_occupied()const
  {
    return (~match_available())&0x7FFF;
  }

  inline int match_really_occupied()const /* excluding sentinel */
  {
    return at(N-1)==sentinel_?match_occupied()&0x3FFF:match_occupied();
  }

private:
  static constexpr unsigned char available_=0,
                                 sentinel_=1;

  inline static int match_word(std::size_t hash)
  {
    static constexpr boost::uint32_t word[]=
    {
      0x02020202u,0x03030303u,0x02020202u,0x03030303u,0x04040404u,0x05050505u,0x06060606u,0x07070707u,
      0x08080808u,0x09090909u,0x0A0A0A0Au,0x0B0B0B0Bu,0x0C0C0C0Cu,0x0D0D0D0Du,0x0E0E0E0Eu,0x0F0F0F0Fu,
      0x10101010u,0x11111111u,0x12121212u,0x13131313u,0x14141414u,0x15151515u,0x16161616u,0x17171717u,
      0x18181818u,0x19191919u,0x1A1A1A1Au,0x1B1B1B1Bu,0x1C1C1C1Cu,0x1D1D1D1Du,0x1E1E1E1Eu,0x1F1F1F1Fu,
      0x20202020u,0x21212121u,0x22222222u,0x23232323u,0x24242424u,0x25252525u,0x26262626u,0x27272727u,
      0x28282828u,0x29292929u,0x2A2A2A2Au,0x2B2B2B2Bu,0x2C2C2C2Cu,0x2D2D2D2Du,0x2E2E2E2Eu,0x2F2F2F2Fu,
      0x30303030u,0x31313131u,0x32323232u,0x33333333u,0x34343434u,0x35353535u,0x36363636u,0x37373737u,
      0x38383838u,0x39393939u,0x3A3A3A3Au,0x3B3B3B3Bu,0x3C3C3C3Cu,0x3D3D3D3Du,0x3E3E3E3Eu,0x3F3F3F3Fu,
      0x40404040u,0x41414141u,0x42424242u,0x43434343u,0x44444444u,0x45454545u,0x46464646u,0x47474747u,
      0x48484848u,0x49494949u,0x4A4A4A4Au,0x4B4B4B4Bu,0x4C4C4C4Cu,0x4D4D4D4Du,0x4E4E4E4Eu,0x4F4F4F4Fu,
      0x50505050u,0x51515151u,0x52525252u,0x53535353u,0x54545454u,0x55555555u,0x56565656u,0x57575757u,
      0x58585858u,0x59595959u,0x5A5A5A5Au,0x5B5B5B5Bu,0x5C5C5C5Cu,0x5D5D5D5Du,0x5E5E5E5Eu,0x5F5F5F5Fu,
      0x60606060u,0x61616161u,0x62626262u,0x63636363u,0x64646464u,0x65656565u,0x66666666u,0x67676767u,
      0x68686868u,0x69696969u,0x6A6A6A6Au,0x6B6B6B6Bu,0x6C6C6C6Cu,0x6D6D6D6Du,0x6E6E6E6Eu,0x6F6F6F6Fu,
      0x70707070u,0x71717171u,0x72727272u,0x73737373u,0x74747474u,0x75757575u,0x76767676u,0x77777777u,
      0x78787878u,0x79797979u,0x7A7A7A7Au,0x7B7B7B7Bu,0x7C7C7C7Cu,0x7D7D7D7Du,0x7E7E7E7Eu,0x7F7F7F7Fu,
      0x80808080u,0x81818181u,0x82828282u,0x83838383u,0x84848484u,0x85858585u,0x86868686u,0x87878787u,
      0x88888888u,0x89898989u,0x8A8A8A8Au,0x8B8B8B8Bu,0x8C8C8C8Cu,0x8D8D8D8Du,0x8E8E8E8Eu,0x8F8F8F8Fu,
      0x90909090u,0x91919191u,0x92929292u,0x93939393u,0x94949494u,0x95959595u,0x96969696u,0x97979797u,
      0x98989898u,0x99999999u,0x9A9A9A9Au,0x9B9B9B9Bu,0x9C9C9C9Cu,0x9D9D9D9Du,0x9E9E9E9Eu,0x9F9F9F9Fu,
      0xA0A0A0A0u,0xA1A1A1A1u,0xA2A2A2A2u,0xA3A3A3A3u,0xA4A4A4A4u,0xA5A5A5A5u,0xA6A6A6A6u,0xA7A7A7A7u,
      0xA8A8A8A8u,0xA9A9A9A9u,0xAAAAAAAAu,0xABABABABu,0xACACACACu,0xADADADADu,0xAEAEAEAEu,0xAFAFAFAFu,
      0xB0B0B0B0u,0xB1B1B1B1u,0xB2B2B2B2u,0xB3B3B3B3u,0xB4B4B4B4u,0xB5B5B5B5u,0xB6B6B6B6u,0xB7B7B7B7u,
      0xB8B8B8B8u,0xB9B9B9B9u,0xBABABABAu,0xBBBBBBBBu,0xBCBCBCBCu,0xBDBDBDBDu,0xBEBEBEBEu,0xBFBFBFBFu,
      0xC0C0C0C0u,0xC1C1C1C1u,0xC2C2C2C2u,0xC3C3C3C3u,0xC4C4C4C4u,0xC5C5C5C5u,0xC6C6C6C6u,0xC7C7C7C7u,
      0xC8C8C8C8u,0xC9C9C9C9u,0xCACACACAu,0xCBCBCBCBu,0xCCCCCCCCu,0xCDCDCDCDu,0xCECECECEu,0xCFCFCFCFu,
      0xD0D0D0D0u,0xD1D1D1D1u,0xD2D2D2D2u,0xD3D3D3D3u,0xD4D4D4D4u,0xD5D5D5D5u,0xD6D6D6D6u,0xD7D7D7D7u,
      0xD8D8D8D8u,0xD9D9D9D9u,0xDADADADAu,0xDBDBDBDBu,0xDCDCDCDCu,0xDDDDDDDDu,0xDEDEDEDEu,0xDFDFDFDFu,
      0xE0E0E0E0u,0xE1E1E1E1u,0xE2E2E2E2u,0xE3E3E3E3u,0xE4E4E4E4u,0xE5E5E5E5u,0xE6E6E6E6u,0xE7E7E7E7u,
      0xE8E8E8E8u,0xE9E9E9E9u,0xEAEAEAEAu,0xEBEBEBEBu,0xECECECECu,0xEDEDEDEDu,0xEEEEEEEEu,0xEFEFEFEFu,
      0xF0F0F0F0u,0xF1F1F1F1u,0xF2F2F2F2u,0xF3F3F3F3u,0xF4F4F4F4u,0xF5F5F5F5u,0xF6F6F6F6u,0xF7F7F7F7u,
      0xF8F8F8F8u,0xF9F9F9F9u,0xFAFAFAFAu,0xFBFBFBFBu,0xFCFCFCFCu,0xFDFDFDFDu,0xFEFEFEFEu,0xFFFFFFFFu,
    };

    return (int)word[(unsigned char)hash];
  }

  inline static unsigned char adjust_hash(std::size_t hash)
  {
    return (unsigned char)match_word(hash);
  }

  inline unsigned char& at(std::size_t pos)
  {
    return reinterpret_cast<unsigned char*>(&m)[pos];
  }

  inline unsigned char at(std::size_t pos)const
  {
    return reinterpret_cast<const unsigned char*>(&m)[pos];
  }

  inline unsigned char& overflow()
  {
    return at(N);
  }

  inline unsigned char overflow()const
  {
    return at(N);
  }

  __m128i m;
};

#elif BOOST_UNORDERED_LITTLE_ENDIAN_NEON

struct group15
{
  static constexpr int N=15;

  struct dummy_group_type
  {
    alignas(16) unsigned char storage[N+1]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0};
  };

  inline void set(std::size_t pos,std::size_t hash)
  {
    BOOST_ASSERT(pos<N);
    at(pos)=adjust_hash(hash);
  }

  inline void set_sentinel()
  {
    at(N-1)=sentinel_;
  }

  inline bool is_sentinel(std::size_t pos)const
  {
    BOOST_ASSERT(pos<N);
    return at(pos)==sentinel_;
  }

  inline void reset(std::size_t pos)
  {
    BOOST_ASSERT(pos<N);
    at(pos)=available_;
  }

  static inline void reset(unsigned char* pc)
  {
    *pc=available_;
  }

  inline int match(std::size_t hash)const
  {
    return simde_mm_movemask_epi8(
      vceqq_s8(m,vdupq_n_s8(adjust_hash(hash))))&0x7FFF;
  }

  inline bool is_not_overflowed(std::size_t hash)const
  {
    static constexpr unsigned char shift[]={1,2,4,8,16,32,64,128};

    return !(overflow()&shift[hash%8]);
  }

  inline void mark_overflow(std::size_t hash)
  {
    overflow()|=static_cast<unsigned char>(1<<(hash%8));
  }

  inline int match_available()const
  {
    return simde_mm_movemask_epi8(vceqq_s8(m,vdupq_n_s8(0)))&0x7FFF;
  }

  inline int match_occupied()const
  {
    return simde_mm_movemask_epi8(
      vcgtq_u8(vreinterpretq_u8_s8(m),vdupq_n_u8(0)))&0x7FFF;
  }

  inline int match_really_occupied()const /* excluding sentinel */
  {
    return at(N-1)==sentinel_?match_occupied()&0x3FFF:match_occupied();
  }

private:
  static constexpr unsigned char available_=0,
                                 sentinel_=1;

  inline static unsigned char adjust_hash(unsigned char hash)
  {
    static constexpr unsigned char table[]={
      2,3,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
      16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
      32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,
      48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,
      64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
      80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
      96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
      112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
      128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
      144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
      160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
      176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
      192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
      208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
      224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
      240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
    };
    
    return table[hash];
  }

  /* copied from https://github.com/simd-everywhere/simde/blob/master/simde/x86/sse2.h#L3763 */

  static inline int simde_mm_movemask_epi8(uint8x16_t a)
  {
    static const uint8_t md[16]={
      1 << 0, 1 << 1, 1 << 2, 1 << 3,
      1 << 4, 1 << 5, 1 << 6, 1 << 7,
      1 << 0, 1 << 1, 1 << 2, 1 << 3,
      1 << 4, 1 << 5, 1 << 6, 1 << 7,
    };

    uint8x16_t  masked=vandq_u8(vld1q_u8(md),a);
    uint8x8x2_t tmp=vzip_u8(vget_low_u8(masked),vget_high_u8(masked));
    uint16x8_t  x=vreinterpretq_u16_u8(vcombine_u8(tmp.val[0],tmp.val[1]));
    return vaddvq_u16(x);
  }

  inline unsigned char& at(std::size_t pos)
  {
    return reinterpret_cast<unsigned char*>(&m)[pos];
  }

  inline unsigned char at(std::size_t pos)const
  {
    return reinterpret_cast<const unsigned char*>(&m)[pos];
  }

  inline unsigned char& overflow()
  {
    return at(N);
  }

  inline unsigned char overflow()const
  {
    return at(N);
  }

  int8x16_t m;
};

#endif

inline unsigned int unchecked_countr_zero(int x)
{
#if defined(BOOST_MSVC)
  unsigned long r;
  _BitScanForward(&r,(unsigned long)x);
  return (unsigned int)r;
#else
  BOOST_UNORDERED_ASSUME(x);
  return (unsigned int)boost::core::countr_zero((unsigned int)x);
#endif
}

inline void prefetch(const void* p)
{
#if defined(BOOST_GCC)||defined(BOOST_CLANG)
  __builtin_prefetch((const char*)p);
#elif defined(BOOST_UNORDERED_SSE2)
  _mm_prefetch((const char*)p,_MM_HINT_T0);
#endif    
}

struct pow2_size_policy
{
  static inline std::size_t size_index(std::size_t n)
  {
    // TODO: min size is 2, see if we can bring it down to 1 without loss
    // of performance

    return sizeof(std::size_t)*CHAR_BIT-
      (n<=2?1:((std::size_t)(boost::core::bit_width(n-1))));
  }

  static inline std::size_t size(std::size_t size_index)
  {
     return std::size_t(1)<<(sizeof(std::size_t)*CHAR_BIT-size_index);  
  }
    
  static constexpr std::size_t min_size(){return 2;}

  static inline std::size_t position(std::size_t hash,std::size_t size_index)
  {
    return hash>>size_index;
  }
};

struct pow2_quadratic_prober
{
  pow2_quadratic_prober(std::size_t pos_):pos{pos_}{}

  inline std::size_t get()const{return pos;}

  inline bool next(std::size_t mask)
  {
    step+=1;
    pos=(pos+step)&mask;
    return step<=mask;
  }

private:
  std::size_t pos,step=0;
};

template<typename,typename,typename,typename>
class table;

template<typename Value,typename Group,bool Const>
class table_iterator
{
public:
  using difference_type=std::ptrdiff_t;
  using value_type=typename std::conditional<Const,const Value,Value>::type;
  using pointer=value_type*;
  using reference=value_type&;
  using iterator_category=std::forward_iterator_tag;

  table_iterator()=default;
  template<bool Const2,typename std::enable_if<!Const2>::type* =nullptr>
  table_iterator(const table_iterator<Value,Group,Const2>& x):
    pc{x.pc},p{x.p}{}

  inline reference operator*()const noexcept{return *p;}
  inline pointer operator->()const noexcept{return p;}
  inline table_iterator& operator++()noexcept{increment();return *this;}
  inline table_iterator operator++(int)noexcept
    {auto x=*this;increment();return x;}
  friend inline bool operator==(
    const table_iterator& x,const table_iterator& y)
    {return x.p==y.p;}
  friend inline bool operator!=(
    const table_iterator& x,const table_iterator& y)
    {return !(x==y);}

private:
  template<typename,typename,bool> friend class table_iterator;
  template<typename,typename,typename,typename> friend class table;

  table_iterator(Group* pg,std::size_t n,const Value* p_):
    pc{reinterpret_cast<unsigned char*>(const_cast<Group*>(pg))+n},
    p{const_cast<Value*>(p_)}
    {}

  inline std::size_t rebase() noexcept
  {
    std::size_t off=reinterpret_cast<uintptr_t>(pc)%sizeof(Group);
    pc-=off;
    return off;
  }

  inline void increment()noexcept
  {
    std::size_t n0=rebase();

    int mask=(reinterpret_cast<Group*>(pc)->match_occupied()>>(n0+1))<<(n0+1);
    if(!mask){
      do{
        pc+=sizeof(Group);
        p+=Group::N;
      }
      while(!(mask=reinterpret_cast<Group*>(pc)->match_occupied()));
    }

    auto n=unchecked_countr_zero(mask);
    if(BOOST_UNLIKELY(reinterpret_cast<Group*>(pc)->is_sentinel(n))){
      p=nullptr;
    }
    else{
      pc+=n;
      p-=n0;
      p+=n;
    }
  }

  unsigned char *pc=nullptr;
  Value         *p=nullptr;
};

template<typename TypePolicy,typename Hash,typename Pred,typename Allocator>
class table
{
  using type_policy=TypePolicy;
  using group_type=group15;
  static constexpr auto N=group_type::N;
  using size_policy=pow2_size_policy;
  using prober=pow2_quadratic_prober;

public:
  using key_type=typename type_policy::key_type;
  using value_type=typename type_policy::value_type;

private:
  static constexpr bool has_mutable_iterator=
    !std::is_same<key_type,value_type>::value;

public:
  using hasher=Hash;
  using key_equal=Pred;
  using allocator_type=Allocator;
  using pointer=value_type*;
  using const_pointer=const value_type*;
  using reference=value_type&;
  using const_reference=const value_type&;
  using size_type=std::size_t;
  using difference_type=std::ptrdiff_t;
  using const_iterator=table_iterator<value_type,group_type,true>;
  using iterator=typename std::conditional<
    has_mutable_iterator,
    table_iterator<value_type,group_type,false>,
    const_iterator>::type;

  table(
    std::size_t n=0,const Hash& h_=Hash(),const Pred& pred_=Pred(),
    const Allocator& al_=Allocator()):
    h{h_},pred{pred_},al{al_},size_{0},arrays{new_arrays(n)},ml{max_load()}
  {}

  table(const table& x):table(x,x.al){}

  table(table&& x)
    noexcept(
      std::is_nothrow_move_constructible<Hash>::value&&
      std::is_nothrow_move_constructible<Pred>::value&&
      std::is_nothrow_move_constructible<Allocator>::value):
    // TODO verify if we should copy or move copy hash, pred and al
    h{std::move(x.h)},pred{std::move(x.pred)},al{std::move(x.al)},
    size_{x.size_},arrays{x.arrays},ml{x.ml}
  {
    x.size_=0;
    x.arrays=x.new_arrays(0);
    x.ml=x.max_load();
  }

  table(const table& x,const Allocator& al_):
    h{x.h},pred{x.pred},al{al_},size_{0},
    arrays{new_arrays(std::size_t(std::ceil(x.size()/mlf)))},
    ml{max_load()}
  {
    BOOST_TRY{
      x.for_all_elements([this](value_type* p){
        unchecked_insert(*p);
      });
    }
    BOOST_CATCH(...){
      clear();
      delete_arrays(arrays);
    }
    BOOST_CATCH_END
  }

  table(table&& x,const Allocator& al_):
    table{0,std::move(x.h),std::move(x.pred),al_}
  {
    if(al==x.al){
      size_=x.size_;
      arrays=x.arrays;
      ml=x.ml;
      x.size_=0;
      x.arrays=x.new_arrays(0);
      x.ml=x.max_load();
    }
    else{
      reserve(x.size());
      BOOST_TRY{
        /* This works because subsequent x.clear() does not depend on the
         * elements' values.
         */

        x.for_all_elements([this](value_type* p){
          unchecked_insert(std::move(*p));
        });
      }
      BOOST_CATCH(...){
        clear();
        delete_arrays(arrays);
        x.clear();
      }
      BOOST_CATCH_END
      x.clear();
    }
  }

  ~table()noexcept
  {
    clear();
    delete_arrays(arrays);
  }

  table& operator=(const table& x)
  {
    if(this!=&x){
      clear();
      h=x.h;
      pred=x.pred;
      if(alloc_traits::propagate_on_container_copy_assignment::value){
        if(al!=x.al)reserve(0);
        al=x.al;
      }
      // TODO may shrink arrays and miss an opportunity for memory reuse
      reserve(x.size());
      x.for_all_elements([this](value_type* p){
        unchecked_insert(*p);
      });
    }
    return *this;
  }

  table& operator=(table&& x)
    noexcept(
      alloc_traits::is_always_equal::value&&
      std::is_nothrow_move_assignable<Hash>::value&&
      std::is_nothrow_move_assignable<Pred>::value)
  {
    if(this!=&x){
      // TODO explain why not constexpr
      auto pocma=alloc_traits::propagate_on_container_move_assignment::value;
      clear();
      h=std::move(x.h);
      pred=std::move(x.pred);
      if(pocma||al==x.al){
        using std::swap;
        reserve(0);
        swap(arrays,x.arrays);
        swap(ml,x.ml);
        if(pocma)al=std::move(x.al);
      }
      else{
        reserve(x.size());
        BOOST_TRY{
          /* This works because subsequent x.clear() does not depend on the
           * elements' values.
           */

          x.for_all_elements([this](value_type* p){
            unchecked_insert(std::move(*p));
          });
        }
        BOOST_CATCH(...){
          x.clear();
        }
        BOOST_CATCH_END
        x.clear();
      }
    }
    return *this;
  }

  allocator_type get_allocator()const noexcept{return al;}

  iterator begin()noexcept
  {
    iterator it{arrays.groups,0,arrays.elements};
    if(!(arrays.groups[0].match_occupied()&0x1))++it;
    return it;
  }

  const_iterator begin()const noexcept
                   {return const_cast<table*>(this)->begin();}
  iterator       end()noexcept{return {};}
  const_iterator end()const noexcept{return const_cast<table*>(this)->end();}
  const_iterator cbegin()const noexcept{return begin();}
  const_iterator cend()const noexcept{return end();}

  bool        empty()const noexcept{return size()!=0;}
  std::size_t size()const noexcept{return size_;}
  std::size_t max_size()const noexcept{return SIZE_MAX;}

  template<typename... Args>
  BOOST_FORCEINLINE std::pair<iterator,bool> emplace(Args&&... args)
  {
    return emplace_impl(value_type(std::forward<Args>(args)...));
  }

  template<typename Key,typename... Args>
  BOOST_FORCEINLINE std::pair<iterator,bool> try_emplace(
    Key&& k,Args&&... args)
  {
    return emplace_impl(
      std::piecewise_construct,
      std::forward_as_tuple(std::forward<Key>(k)),
      std::forward_as_tuple(std::forward<Args>(args)...));
  }

  BOOST_FORCEINLINE std::pair<iterator,bool> insert(const value_type& x)
  {
    return emplace_impl(x);
  }

  BOOST_FORCEINLINE std::pair<iterator,bool> insert(value_type&& x)
  {
    return emplace_impl(std::move(x));
  }

  template<
    bool dependent_value=false,
    typename std::enable_if<
      has_mutable_iterator||dependent_value>::type* =nullptr
  >
  void erase(iterator pos)noexcept{return erase(const_iterator(pos));}

  void erase(const_iterator pos)noexcept
  {
    destroy_element(pos.p);
    group_type::reset(pos.pc);
    --size_;
  }

  template<typename Key>
  std::size_t erase(const Key& x)
  {
    auto it=find(x);
    if(it!=end()){
      erase(it);
      return 1;
    }
    else return 0;
  }

  void swap(table& x)
    noexcept(
      alloc_traits::is_always_equal::value&&
      boost::is_nothrow_swappable<Hash>::value&&
      boost::is_nothrow_swappable<Pred>::value)
  {
    using std::swap;
    swap(h,x.h);
    swap(pred,x.pred);
    if(alloc_traits::propagate_on_container_swap::value)swap(al,x.al);
    else BOOST_ASSERT(al==x.al);
    swap(size_,x.size_);
    swap(arrays,x.arrays);
    swap(ml,x.ml);
  }

  void clear()noexcept
  {
    for_all_elements([this](value_type* p){destroy_element(p);});
    size_=0;
  }

  hasher hash_function()const{return h;}
  key_equal key_eq()const{return pred;}

  template<typename Key>
  BOOST_FORCEINLINE iterator find(const Key& x)
  {
    auto hash=h(x);
    return find_impl(x,position_for(hash),hash);
  }

  template<typename Key>
  BOOST_FORCEINLINE const_iterator find(const Key& x)const
  {
    return const_cast<table*>(this)->find(x);
  }

  std::size_t capacity()const noexcept
  {
    return arrays.elements?(arrays.groups_size_mask+1)*N-1:0;
  }
  
  float load_factor()const noexcept{return float(size())/float(capacity());}
  float max_load_factor()const noexcept{return mlf;}

  void rehash(std::size_t n)
  {
    auto c1=std::size_t(std::ceil(float(size())/mlf));
    auto c2=n?size_policy::size(size_policy::size_index(n/N+1))*N-1:0;
    auto c=c1>c2?c1:c2; /* we avoid std::max to not include <algorithm> */

    if(c!=capacity())unchecked_rehash(c);
  }

  void reserve(std::size_t n)
  {
    rehash(std::size_t(std::ceil(n/mlf)));
  }

private:
  using alloc_traits=std::allocator_traits<Allocator>;
  using group_allocator=
    typename alloc_traits::template rebind_alloc<group_type>;
  using group_alloc_traits=std::allocator_traits<group_allocator>;

  struct arrays_info{
    std::size_t  groups_size_index;
    std::size_t  groups_size_mask;
    group_type  *groups;
    value_type  *elements;
  };

  arrays_info new_arrays(std::size_t n)
  {
    auto  groups_size_index=size_policy::size_index(n/N+1);
    auto        groups_size=size_policy::size(groups_size_index);
    arrays_info new_arrays_{
      groups_size_index,
      groups_size-1,
      nullptr,
      nullptr
    };
    if(!n){
      new_arrays_.groups=dummy_groups();
    }
    else{
      group_allocator gal=al;
      new_arrays_.groups=boost::to_address(group_alloc_traits::allocate(gal,groups_size));
      // TODO: explain why memset
      std::memset(
        new_arrays_.groups,0,sizeof(group_type)*groups_size);
      new_arrays_.groups[groups_size-1].set_sentinel();
      BOOST_TRY{
        new_arrays_.elements=boost::to_address(alloc_traits::allocate(al,groups_size*N-1));
      }
      BOOST_CATCH(...){
        group_alloc_traits::deallocate(gal,new_arrays_.groups,groups_size);
        BOOST_RETHROW;
      }
      BOOST_CATCH_END
    }
    return new_arrays_;
  }

  static group_type* dummy_groups()noexcept
  {
    static constexpr group_type::dummy_group_type storage[size_policy::min_size()];
    return reinterpret_cast<group_type*>(
      const_cast<group_type::dummy_group_type*>(storage));
  }

  void delete_arrays(const arrays_info& arrays_)noexcept
  {
    if(arrays_.elements){
      auto groups_size=arrays_.groups_size_mask+1;
      alloc_traits::deallocate(al,arrays_.elements,groups_size*N-1);
      group_allocator gal=al;
      group_alloc_traits::deallocate(gal,arrays_.groups,groups_size);
    }
  }

  template<typename... Args>
  void construct_element(value_type* p,Args&&... args)
  {
    alloc_traits::construct(al,p,std::forward<Args>(args)...);
  }

  void destroy_element(value_type* p)noexcept
  {
    alloc_traits::destroy(al,p);
  }

  std::size_t max_load()const
  {
    float fml=mlf*(float)(capacity());
    auto  res=(std::numeric_limits<std::size_t>::max)();
    if(res>(std::size_t)fml)res=(std::size_t)fml;
    return res;
  }

  static inline auto key_from(const value_type& x)
    ->decltype(type_policy::extract(x))
  {
    return type_policy::extract(x);
  }

  template<typename Key>
  static inline const Key& key_from(const Key& x)
  {
    return x;
  }

  template<typename Arg1,typename Arg2>
  static inline auto key_from(
    std::piecewise_construct_t,const Arg1& k,const Arg2&)
    ->decltype(std::get<0>(k))
  {
    return std::get<0>(k);
  }

  inline std::size_t position_for(std::size_t hash)const
  {
    return position_for(hash,arrays);
  }

  static inline std::size_t position_for(
    std::size_t hash,const arrays_info& arrays_)
  {
    return size_policy::position(hash,arrays_.groups_size_index);
  }

  static inline void prefetch_elements(const value_type* p)
  {
#if BOOST_ARCH_ARM
    constexpr int  cache_line=64; // TODO: get from Boost.Predef?
                                  // TODO: check if this is 128 in current benchmark machine
    const char    *p0=reinterpret_cast<const char*>(p),
                  *p1=p0+sizeof(value_type)*N/2;
    for(auto p=p0;p<p1;p+=cache_line)prefetch(p);
#else
    prefetch(p);
#endif
  }

  template<typename Key>
  BOOST_FORCEINLINE iterator find_impl(
    const Key& x,std::size_t pos0,std::size_t hash)const
  {    
    prober pb(pos0);
    do{
      auto pos=pb.get();
      auto pg=arrays.groups+pos;
      auto mask=pg->match(hash);
      if(mask){
        auto p=arrays.elements+pos*N;
        prefetch_elements(p);
        do{
          auto n=unchecked_countr_zero(mask);
          if(BOOST_LIKELY(pred(x,key_from(p[n])))){
            return {pg,n,p+n};
          }
          mask&=mask-1;
        }while(mask);
      }
      if(BOOST_LIKELY(pg->is_not_overflowed(hash))){
        return {}; // TODO end() does not work (returns const_iterator)
      }
    }
    while(BOOST_LIKELY(pb.next(arrays.groups_size_mask)));
    return {}; // TODO end() does not work (returns const_iterator)
  }

  template<typename... Args>
  BOOST_FORCEINLINE std::pair<iterator,bool> emplace_impl(Args&&... args)
  {
    const auto &k=key_from(std::forward<Args>(args)...);
    auto        hash=h(k);
    auto        pos0=position_for(hash);
    auto        it=find_impl(k,pos0,hash);

    if(it!=end()){
      return {it,false};
    }
    else if(BOOST_UNLIKELY(size_>=ml)){
      unchecked_rehash(std::size_t(std::ceil((size_+1)/mlf)));
      pos0=position_for(hash);
    }
    return {
      unchecked_emplace_at(pos0,hash,std::forward<Args>(args)...),
      true
    };  
  }

  BOOST_NOINLINE void unchecked_rehash(std::size_t n)
  {
    auto        new_arrays_=new_arrays(n);
    std::size_t num_tx=0;
    BOOST_TRY{
      for_all_elements([&,this](value_type* p){
        nosize_transfer_element(p,new_arrays_);
        ++num_tx;
      });
    }
    BOOST_CATCH(...){
      size_-=num_tx;
      if(num_tx){
        auto pg=arrays.groups;
        for(std::size_t pos=0;;++pos,++pg){
          auto mask=pg->match_occupied();
          while(mask){
            auto nz=unchecked_countr_zero(mask);
            pg->reset(nz);
            if(!(--num_tx))goto continue_;
          }
        }
      }
    continue_:
      for_all_elements(new_arrays_,[this](value_type* p){
        destroy_element(p);
      });
      delete_arrays(new_arrays_);
      BOOST_RETHROW;
    }
    BOOST_CATCH_END
    delete_arrays(arrays);
    arrays=new_arrays_;
    ml=max_load();
  }

  template<typename Value>
  void unchecked_insert(Value&& x)
  {
    auto hash=h(key_from(x));
    unchecked_emplace_at(position_for(hash),hash,std::forward<Value>(x));
  }

  void nosize_transfer_element(value_type* p,const arrays_info& arrays_)
  {
    auto hash=h(key_from(*p));
    nosize_unchecked_emplace_at(
      arrays_,position_for(hash,arrays_),hash,std::move(*p));
    destroy_element(p);
  }

  template<typename... Args>
  iterator unchecked_emplace_at(
    std::size_t pos0,std::size_t hash,Args&&... args)
  {
    auto res=nosize_unchecked_emplace_at(
      arrays,pos0,hash,std::forward<Args>(args)...);
    ++size_;
    return res;
  }

#if 0
  template<typename... Args>
  iterator nosize_unchecked_emplace_at(
    const arrays_info& arrays_,std::size_t pos0,std::size_t hash,
    Args&&... args)
  {
    auto  p=insert_position(arrays_,pos0,hash);
    auto &pos=p.first;
    auto &n=p.second;
    auto  pg=arrays_.groups+pos;
    auto  p=arrays_.elements+pos*N+n;
    construct_element(p,std::forward<Args>(args)...);
    pg->set(n,hash);
    return {pg,n,p};
  }

  std::pair<std::size_t,std::size_t>
  static insert_position(
    const arrays_info& arrays_,std::size_t pos0,std::size_t hash)
  {
    for(prober pb(pos0);;pb.next(arrays_.groups_size_mask)){
      auto pos=pb.get();
      auto pg=arrays_.groups+pos;
      auto mask=pg->match_available();
      if(BOOST_LIKELY(mask)){
        return {pos,unchecked_countr_zero(mask)};
      }
      else pg->mark_overflow(hash);
    }
  }
#else
  template<typename... Args>
  iterator nosize_unchecked_emplace_at(
    const arrays_info& arrays_,std::size_t pos0,std::size_t hash,
    Args&&... args)
  {
    for(prober pb(pos0);;pb.next(arrays_.groups_size_mask)){
      auto pos=pb.get();
      auto pg=arrays_.groups+pos;
      auto mask=pg->match_available();
      if(BOOST_LIKELY(mask)){
        auto n=unchecked_countr_zero(mask);
        auto p=arrays_.elements+pos*N+n;
        construct_element(p,std::forward<Args>(args)...);
        pg->set(n,hash);
        return {pg,n,p};
      }
      else pg->mark_overflow(hash);
    }
  }
#endif

  template<typename F>
  void for_all_elements(F f)const
  {
    for_all_elements(arrays,f);
  }

  template<typename F>
  static void for_all_elements(const arrays_info& arrays_,F f)
  {
    auto pg=arrays_.groups;
    auto p=arrays_.elements;
    if(!p){return;}
    for(std::size_t pos=0,last=arrays_.groups_size_mask+1;
        pos!=last;++pos,++pg,p+=N){
      auto mask=pg->match_really_occupied();
      while(mask){
        f(p+unchecked_countr_zero(mask));
        mask&=mask-1;
      }
    }
  }

  Hash                   h;
  Pred                   pred;
  Allocator              al;
  static constexpr float mlf=0.875;
  std::size_t            size_;
  arrays_info            arrays;
  std::size_t            ml;
};

} /* namespace foa */
} /* namespace detail */
} /* namespace unordered */
} /* namespace boost */

#undef BOOST_UNORDERED_ASSUME
#undef BOOST_UNORDERED_HAS_BUILTIN
#ifdef BOOST_UNORDERED_LITTLE_ENDIAN_NEON
#undef BOOST_UNORDERED_LITTLE_ENDIAN_NEON
#endif
#ifdef BOOST_UNORDERED_SSE2
#undef BOOST_UNORDERED_SSE2
#endif
#endif
