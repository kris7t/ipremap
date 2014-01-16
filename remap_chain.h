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

#ifndef IPREMAPD_REMAP_CHAIN_H_
#define IPREMAPD_REMAP_CHAIN_H_

#include <string>

#include <arpa/inet.h>

namespace ipremapd {

class RemapChain {
 public:
  explicit RemapChain(const std::string &name);
  
  void Flush();
  void AddRule(in_addr orig, in_addr nat);
  void DeleteRule(in_addr orig, in_addr nat);

 private:
  enum class Action {
    kAdd, kDelete
  };

  void RuleAction(Action action, in_addr orig, in_addr nat);

  std::string name_;
};

} // namespace ipremapd

#endif // IPREMAPD_REMAP_CHAIN_H_
