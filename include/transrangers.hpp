/* Transrangers: an efficient, composable design pattern for range processing.
 *
 * Copyright 2021 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See https://github.com/joaquintides/transrangers for project home page.
 */
 
#ifndef JOAQUINTIDES_TRANSRANGERS_HPP
#define JOAQUINTIDES_TRANSRANGERS_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>

namespace transrangers{

template<typename Cursor,typename F> 
struct ranger_class:F
{
  using cursor=Cursor;
};
    
template<typename Cursor,typename F>
auto ranger(F f)
{
  return ranger_class<Cursor,F>{f};
}
    
template<typename Range>
auto all(Range&& rng)
{
  using std::begin;
  using std::end;
  using cursor=decltype(begin(rng));
  
  return ranger<cursor>([first=begin(rng),last=end(rng)](auto dst)mutable{
    auto it=first;
    while(it!=last)if(!dst(it++)){first=it;return false;}
    first=it;
    return true;
  });
}

template<typename Range>
struct all_copy
{
  using ranger=decltype(all(std::declval<Range&>()));
  using cursor=typename ranger::cursor;
  
  auto operator()(auto p){return rgr(p);}

  Range  rng;
  ranger rgr=all(rng);
};

template<typename Range>
auto all(Range&& rng) requires(std::is_rvalue_reference_v<Range&&>)
{
  return all_copy<Range>{std::move(rng)};
}

template<typename Pred,typename Ranger>
auto filter(Pred pred,Ranger rgr)
{
  using cursor=typename Ranger::cursor;
    
  return ranger<cursor>([=](auto dst)mutable{
    return rgr([&](auto p){
      return pred(*p)?dst(p):true;
    });
  });
}

template<typename Cursor,typename F,typename=void>
struct deref_fun
{
  decltype(auto) operator*()const{return (*pf)(*p);} 
    
  Cursor p;
  F*     pf;
};

template<typename Cursor,typename F>
struct deref_fun<
  Cursor,F,
  std::enable_if_t<
    std::is_trivially_default_constructible_v<F>&&std::is_empty_v<F>
  >
>
{
  deref_fun(Cursor p,F*):p{p}{}

  decltype(auto) operator*()const{return F()(*p);} 
    
  Cursor p;
};
    
template<typename F,typename Ranger>
auto transform(F f,Ranger rgr)
{
  using cursor=deref_fun<typename Ranger::cursor,F>;
    
  return ranger<cursor>([=](auto dst)mutable{
    return rgr([&](auto p){return dst(cursor{p,&f});});
  });
}

template<typename Ranger>
auto take(int n,Ranger rgr)
{
  using cursor=typename Ranger::cursor;
    
  return ranger<cursor>([=](auto dst)mutable{
    if(n)return rgr([&](auto p){
      --n;
      return dst(p)&&(n!=0);
    })||(n==0);
    else return true;
  });
}

auto concat()
{
  return [](auto&&){return true;};
}

template<typename Ranger,typename... Rangers>
auto concat(Ranger rgr,Rangers... rgrs)
{
  using cursor=typename Ranger::cursor;
    
  return ranger<cursor>(
    [=,cont=false,next=concat(rgrs...)](auto dst)mutable{
      if(!cont){
        if(!(cont=rgr(dst)))return false;
      }
      return next(dst);
    }
  );
}
    
template<typename Ranger>
auto unique(Ranger rgr)
{
  using cursor=typename Ranger::cursor;
    
  return ranger<cursor>([=,start=true,p=cursor{}](auto dst)mutable{
    if(start){
      start=false;
      bool cont=false;
      if(rgr([&](auto q){
        p=q;
        cont=dst(q);
        return false;
      }))return true;
      if(!cont)return false;
    }
    return rgr([&](auto q){
      if(*p==*q){p=q;return true;}
      else{
        p=q;
        return dst(q);
      }
    });
  });
}

template<typename Ranger>
auto join(Ranger rgr)
{
  using cursor=typename Ranger::cursor;
  using subranger=std::remove_cvref_t<decltype(*std::declval<cursor>())>; 
  using subranger_cursor=typename subranger::cursor;
    
  return ranger<subranger_cursor>(
    [=,osrgr=std::optional<subranger>{}](auto dst)mutable{
      if(osrgr){
        if(!(*osrgr)(dst))return false;
      }
      return(rgr([&](auto p){
        auto cont=(*p)(dst);
        if(!cont)osrgr.emplace(*p);
        return cont;
      }));
  });    
}

template<typename Ranger>
auto ranger_join(Ranger rgr)
{
  auto all_adaptor=[](auto&& srng){
    return all(std::forward<decltype(srng)>(srng));
  };

  return join(transform(all_adaptor,rgr));
}

} /* transrangers */

#endif
