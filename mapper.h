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

#ifndef IPREMAPD_MAPPER_H_
#define IPREMAPD_MAPPER_H_

#include <chrono>
#include <cstdint>
#include <random>
#include <unordered_set>
#include <unordered_map>

#include <arpa/inet.h>

#include "remap_chain.h"

namespace ipremapd {

class Mapper {
 public:
  Mapper(RemapChain chain, const in_addr &range, const in_addr &mask,
         std::chrono::system_clock::duration ttl =
         std::chrono::system_clock::duration::zero());
  Mapper(RemapChain chain, const in_addr &range, const in_addr &mask,
         std::size_t max_size, std::chrono::system_clock::duration ttl =
         std::chrono::system_clock::duration::zero());
  Mapper(const Mapper &) = delete;
  Mapper(Mapper &&);
  Mapper &operator=(const Mapper &) = delete;
  Mapper &operator=(Mapper &&);
  ~Mapper();

  in_addr Map(const in_addr &orig_addr);
  void Idle();

  std::size_t mapped_count() const { return map_.size(); }
  bool is_full() const { return mapped_count() == max_size_; }

 private:
  class Mapping {
   public:
    explicit Mapping(in_addr nat_addr);
    
    const in_addr &nat_addr() const { return nat_addr_; }
    const std::chrono::system_clock::time_point last_access() const {
      return last_access_;
    }

    void Touch();
    bool IsExpired(std::chrono::system_clock::duration ttl) const;

   private:
    in_addr nat_addr_;
    std::chrono::system_clock::time_point last_access_;
  };

  struct in_addr_hash {
    std::size_t operator()(const in_addr &a) const;
  };

  struct in_addr_equal_to {
    bool operator()(const in_addr &a, const in_addr &b) const;
  };

  typedef std::unordered_map<in_addr, Mapping, in_addr_hash, in_addr_equal_to>
  unordered_mapping_map;

  typedef std::unordered_set<in_addr, in_addr_hash, in_addr_equal_to>
  unordered_in_addr_set;

  in_addr ReallyMap(const in_addr &orig_addr);
  unordered_mapping_map::iterator Unmap(
      unordered_mapping_map::const_iterator it);
  void UnmapOne();

  in_addr NextAddress();
  in_addr RandomAddress();

  bool flush_on_destroy_;
  RemapChain chain_;
  unordered_mapping_map map_;
  unordered_in_addr_set in_use_;
  in_addr range_;
  in_addr mask_;
  std::size_t max_size_;
  std::chrono::system_clock::duration ttl_;
  std::random_device rand_;
  std::uniform_int_distribution<std::uint32_t> dist_;
};

} // namespace ipremapd

#endif // IPREMAPD_MAPPER_H_
