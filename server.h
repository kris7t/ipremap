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

#ifndef IPREMAPD_SERVER_H_
#define IPREMAPD_SERVER_H_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <sys/time.h>

#include "mapper.h"

namespace ipremapd {

class Server {
 public:
  Server(const std::shared_ptr<Mapper> &mapper,
         const std::string &socket_path);
  Server(const Server &) = delete;
  Server(Server &&);
  Server &operator=(const Server &) = delete;
  Server &operator=(Server &&);
  ~Server();

  void Select(timeval *timeout);

 private:
  static constexpr int kBacklogSize = 32;

  class Connection {
   public:
    explicit Connection(int fd);
    Connection(const Connection &) = delete;
    Connection(Connection &&);
    Connection &operator=(const Connection &) = delete;
    Connection &operator=(Connection &&);
    ~Connection();

    int fd() const { return fd_; }
    bool write_pending() const { return write_pending_; }

    void HandleReadable(Mapper &mapper);
    void HandleWriteable();

   private:
    void SetResponse(const in_addr &addr);
    void SetInvalidResponse();

    int fd_;
    bool write_pending_;
    in_addr response_;
  };

  void HandleAccept();

  std::shared_ptr<Mapper> mapper_;
  std::string socket_path_;
  int socket_fd_;
  int max_fd_;
  std::vector<Connection> connections_;
};

} // namespace ipremapd

#endif // IPREMAPD_SERVER_H_
