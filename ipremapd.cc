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

#include <stdexcept>
#include <iostream>

#include <signal.h>
#include <arpa/inet.h>

#include "remap_chain.h"
#include "mapper.h"
#include "server.h"

namespace ipremapd {

static volatile bool interrupted = false;

static void HandleSigint(int signo) {
  if (signo == SIGINT) {
    interrupted = true;
  }
}

static in_addr StringToAddress(const std::string &str) {
  in_addr addr;
  if (inet_pton(AF_INET, str.c_str(), &addr) <= 0) {
    throw std::runtime_error("inet_pton failed.");
  }
  return addr;
}

} // namespace ipremapd

int main() {
  using namespace ipremapd;

  try {
    in_addr range = StringToAddress("10.19.0.0");
    in_addr mask = StringToAddress("255.255.0.0");

    auto mapper = std::make_shared<Mapper>(RemapChain("ipremap"),
                                           range, mask, 32,
                                           std::chrono::minutes(5));
    auto server = std::make_shared<Server>(mapper, "ipremap.sock");

    signal(SIGINT, HandleSigint);

    while (!interrupted) {
      timeval timeout = {1, 0};
      server->Select(&timeout);
      mapper->Idle();
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  return EXIT_SUCCESS;
}
