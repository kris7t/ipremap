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

#include "remap_chain.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <initializer_list>
#include <vector>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>

namespace ipremapd {

static const char *kIptablesPath = "/sbin/iptables";

static void SetDevNullOrDie() {
  int devnull;

  devnull = open("/dev/null", O_RDONLY);
  if (devnull < 0) {
    _Exit(EXIT_FAILURE);
  }
  if (dup2(devnull, STDOUT_FILENO) < 0
      || dup2(devnull, STDERR_FILENO) < 0) {
    close(devnull);
    _Exit(EXIT_FAILURE);
  }
  close(devnull);

  devnull = open("/dev/null", O_WRONLY);
  if (devnull < 0) {
    _Exit(EXIT_FAILURE);
  }
  if (dup2(devnull, STDIN_FILENO) < 0) {
    close(devnull);
    _Exit(EXIT_FAILURE);
  }
  close(devnull); 
}

static std::vector<char *> BuildArgc(
    const char *path, std::initializer_list<const char *> args) {
  std::vector<char *> argc;
  argc.reserve(args.size() + 2);
  argc.push_back(strdup(path));
  for (const char *item : args) {
    argc.push_back(strdup(item));
  }
  argc.push_back(nullptr);
  return argc;
}

static void Iptables(std::initializer_list<const char *> args) {
  pid_t pid = fork();
  if (pid > 0) {
    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
      throw std::runtime_error("iptables command failed.");
    }
  } else if (pid == 0) {
    SetDevNullOrDie();
    std::vector<char *> argc = BuildArgc(kIptablesPath, args);
    execv(kIptablesPath, argc.data());
    _Exit(EXIT_FAILURE);
  } else {
    throw std::runtime_error("fork failed.");
  }
}

static std::string AddressToString(in_addr addr) {
  char buf[INET_ADDRSTRLEN];
  const char *res = inet_ntop(AF_INET, &addr, buf, INET_ADDRSTRLEN);
  if (res == nullptr) {
    throw std::runtime_error("inet_ntop failed.");
  }
  return std::string(res);
}

RemapChain::RemapChain(const std::string &name) : name_(name) {
}

void RemapChain::Flush() {
  Iptables({"-t", "nat", "-F", name_.c_str()});
}

void RemapChain::AddRule(in_addr orig, in_addr nat) {
  RuleAction(Action::kAdd, orig, nat);
}

void RemapChain::DeleteRule(in_addr orig, in_addr nat) {
  RuleAction(Action::kDelete, orig, nat);
}

void RemapChain::RuleAction(Action action, in_addr orig, in_addr nat) {
  const char *action_arg;
  switch (action) {
    case Action::kAdd:
      action_arg = "-A";
      break;
    case Action::kDelete:
      action_arg = "-D";
      break;
  }
  const std::string orig_str = AddressToString(orig);
  const std::string nat_str = AddressToString(nat);
  Iptables({"-t", "nat", action_arg, name_.c_str(), "--dst", nat_str.c_str(),
          "-j", "DNAT", "--to", orig_str.c_str()});
}

} // namespace ipremapd
