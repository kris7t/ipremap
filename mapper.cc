/*
 * Copyright (C) 2014 Kristof Marussy <kris7topher@gmail.com>
 *
 * This file is part of Ipremap.
 *
 * Ipremap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Ipremap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Ipremap.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mapper.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <type_traits>

// XXX struct in_addr.s_addr is arguably unportable.

namespace ipremapd {

static std::size_t GetRangeSize(const in_addr &mask) {
  std::size_t size = 1;
  for (std::size_t i = 0; i < 32; ++i) {
    if ((mask.s_addr & (1U << i)) == 0) {
      size *= 2;
    }
  }
  return size;
}

Mapper::Mapper(RemapChain chain, const in_addr &range, const in_addr &mask,
               std::chrono::system_clock::duration ttl)
    : Mapper(std::move(chain), range, mask, GetRangeSize(mask) / 2, ttl) {
}

Mapper::Mapper(RemapChain chain, const in_addr &range, const in_addr &mask,
               std::size_t max_size, std::chrono::system_clock::duration ttl)
    : flush_on_destroy_(true), chain_(std::move(chain)), range_(range),
      mask_(mask), max_size_(max_size), ttl_(ttl) {
  if (max_size == 0) {
    throw std::invalid_argument("the must be space for at least one mapping.");
  }
  if (GetRangeSize(mask) < max_size) {
    throw std::invalid_argument("cannot fit all mappings into the range.");
  }
  chain_.Flush();
}

Mapper::Mapper(Mapper &&o)
    : flush_on_destroy_(std::move(o.flush_on_destroy_)),
      chain_(std::move(o.chain_)), map_(std::move(o.map_)),
      in_use_(std::move(o.in_use_)), range_(std::move(o.range_)),
      mask_(std::move(o.mask_)), max_size_(std::move(o.max_size_)),
      ttl_(std::move(o.ttl_)), dist_(std::move(o.dist_)) {
  o.flush_on_destroy_ = false;
}

Mapper &Mapper::operator=(Mapper &&o) {
  if (this != &o) {
    flush_on_destroy_ = std::move(o.flush_on_destroy_);
    chain_ = std::move(o.chain_);
    map_ = std::move(o.map_);
    in_use_ = std::move(o.in_use_);
    range_ = std::move(o.range_);
    mask_ = std::move(o.mask_);
    max_size_ = std::move(o.max_size_);
    dist_ = std::move(o.dist_);
    o.flush_on_destroy_ = false;
  }
  return *this;
}

Mapper::~Mapper() {
  if (flush_on_destroy_) {
    chain_.Flush();
  }
}

in_addr Mapper::Map(const in_addr &orig_addr) {
  auto it = map_.find(orig_addr);
  if (it == map_.end()) {
    return ReallyMap(orig_addr);
  } else {
    it->second.Touch();
    return it->second.nat_addr();
  }
}

void Mapper::Idle() {
  for (auto it = map_.cbegin(); it != map_.cend(); ) {
    if (it->second.IsExpired(ttl_)) {
      it = Unmap(it);
    } else {
      ++it;
    }
  }
}

in_addr Mapper::ReallyMap(const in_addr &orig_addr) {
  if (is_full()) {
    UnmapOne();
    assert(!is_full());
  }
  const in_addr nat_addr = NextAddress();
  chain_.AddRule(orig_addr, nat_addr);
  map_.emplace(std::piecewise_construct,
               std::forward_as_tuple(orig_addr),
               std::forward_as_tuple(nat_addr));
  in_use_.emplace(nat_addr);
  return nat_addr;
}

auto Mapper::Unmap(unordered_mapping_map::const_iterator it)
    -> unordered_mapping_map::iterator {
  chain_.DeleteRule(it->first, it->second.nat_addr());
  in_use_.erase(it->second.nat_addr());
  return map_.erase(it);
}

void Mapper::UnmapOne() {
  typedef unordered_mapping_map::const_reference cref;
  auto lru = std::min_element(
      map_.cbegin(), map_.cend(), [](cref a, cref b) {
      return a.second.last_access() < b.second.last_access();
    });
  Unmap(lru);
}

in_addr Mapper::NextAddress() {
  in_addr addr;
  do {
    addr = RandomAddress();
  } while (in_use_.find(addr) != in_use_.cend());
  return addr;
}

in_addr Mapper::RandomAddress() {
  in_addr addr;
  std::uint32_t r = dist_(rand_);
  addr.s_addr = (range_.s_addr & mask_.s_addr) | (r & ~mask_.s_addr);
  return addr;
}

Mapper::Mapping::Mapping(in_addr nat_addr)
    : nat_addr_(nat_addr), last_access_(std::chrono::system_clock::now()) {
}

void Mapper::Mapping::Touch() {
  last_access_ = std::chrono::system_clock::now();
}

bool Mapper::Mapping::IsExpired(
    std::chrono::system_clock::duration ttl) const {
  if (ttl == std::chrono::system_clock::duration::zero()) {
    // Zero TTL means forever.
    return false;
  } else {
    auto age = std::chrono::system_clock::now() - last_access_;
    return age > ttl;
  }
}

std::size_t Mapper::in_addr_hash::operator()(const in_addr &a) const {
  std::hash<decltype(a.s_addr)> addr_hash;
  return addr_hash(a.s_addr);
}

bool Mapper::in_addr_equal_to::operator()(const in_addr &a,
                                          const in_addr &b) const {
  std::equal_to<decltype(a.s_addr)> addr_equal_to;
  return addr_equal_to(a.s_addr, b.s_addr);
}

} // namespace ipremapd
