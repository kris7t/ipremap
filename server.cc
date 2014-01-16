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

#include "server.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>

#ifndef UNIX_PATH_MAX
// man 7 unix says UNIX_PATH_MAX should be defined, but it isn't.
#define UNIX_PATH_MAX sizeof(std::declval<sockaddr_un>().sun_path)
#endif

namespace ipremapd {

namespace {

// Anonymous namespace silences clang++ -Wweak-vtables

class connection_exception : public std::exception {
};

} // namespace

Server::Server(const std::shared_ptr<Mapper> &mapper,
               const std::string &socket_path)
    : mapper_(std::move(mapper)), socket_path_(socket_path) {
  if (socket_path.length() >= UNIX_PATH_MAX) {
    throw std::invalid_argument("path too long.");
  }

  // XXX unportable code.
  max_fd_ = socket_fd_ = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
  if (socket_fd_ < 0) {
    throw std::runtime_error("socket failed.");
  }
  sockaddr_un sa;
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, socket_path.c_str(), UNIX_PATH_MAX);
  if (bind(socket_fd_, reinterpret_cast<const sockaddr *>(&sa),
           sizeof(sa)) < 0) {
    close(socket_fd_);
    throw std::runtime_error("bind failed.");
  }
  if (listen(socket_fd_, kBacklogSize) < 0) {
    close(socket_fd_);
    unlink(socket_path_.c_str());
    throw std::runtime_error("listen failed.");
  }

  connections_.reserve(kBacklogSize);
}

Server::Server(Server &&o)
    : mapper_(std::move(o.mapper_)), socket_path_(std::move(o.socket_path_)),
      socket_fd_(std::move(o.socket_fd_)), max_fd_(std::move(o.max_fd_)),
      connections_(std::move(o.connections_)) {
  o.socket_fd_ = -1;
}

Server &Server::operator=(Server &&o) {
  if (this != &o) {
    mapper_ = std::move(o.mapper_);
    socket_path_ = std::move(o.socket_path_);
    socket_fd_ = std::move(o.socket_fd_);
    max_fd_ = std::move(o.max_fd_);
    connections_ = std::move(o.connections_);
    o.socket_fd_ = -1;
  }
  return *this;
}

Server::~Server() {
  if (socket_fd_ >= 0) {
    close(socket_fd_);
    unlink(socket_path_.c_str());
  }
}

void Server::Select(timeval *timeout) {
  fd_set readfds, writefds, exceptfds;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);
  FD_SET(socket_fd_, &readfds);
  FD_SET(socket_fd_, &exceptfds);
  for (const auto &conn : connections_) {
    FD_SET(conn.fd(), &exceptfds);
    if (conn.write_pending()) {
      FD_SET(conn.fd(), &writefds);
    } else {
      FD_SET(conn.fd(), &readfds);
    }
  }

  errno = 0;
  if (select(max_fd_ + 1, &readfds, &writefds, &exceptfds, timeout) < 0) {
    if (errno == EINTR) {
      return;
    } else {
      throw std::runtime_error("select failed.");
    }
  }

  if (FD_ISSET(socket_fd_, &exceptfds)) {
    throw std::runtime_error("exception on server socket.");
  }

  for (auto it = connections_.begin(); it != connections_.end(); ) {
    try {
      if (FD_ISSET(it->fd(), &exceptfds)) {
        throw connection_exception();
      } else if (FD_ISSET(it->fd(), &writefds)) {
        it->HandleWriteable();
      } else if (FD_ISSET(it->fd(), &readfds)) {
        it->HandleReadable(*mapper_);
      }
      ++it;
    } catch (connection_exception &) {
      it = connections_.erase(it);
    }
  }

  if (FD_ISSET(socket_fd_, &readfds)) {
    HandleAccept();
  }
}

void Server::HandleAccept() {
  errno = 0;
  // XXX unportable code.
  int fd = accept4(socket_fd_, NULL, NULL, SOCK_NONBLOCK);
  if (fd >= 0) {
    max_fd_ = std::max(max_fd_, fd);
    connections_.emplace_back(fd);
  } else {
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNABORTED) {
      throw std::runtime_error("accept failed.");
    }
  }
}

Server::Connection::Connection(int fd)
    : fd_(fd), write_pending_(false) {
}

Server::Connection::Connection(Connection &&o)
    : fd_(std::move(o.fd_)), write_pending_(std::move(o.write_pending_)),
      response_(std::move(o.response_)) {
  o.fd_ = -1;
}

auto Server::Connection::operator=(Connection &&o) -> Connection & {
  if (this != &o) {
    fd_ = std::move(o.fd_);
    write_pending_ = std::move(o.write_pending_);
    response_ = std::move(o.response_);
    o.fd_ = -1;
  }
  return *this;
}

Server::Connection::~Connection() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

void Server::Connection::HandleReadable(Mapper &mapper) {
  in_addr request;
  errno = 0;
  ssize_t res = read(fd_, &request, sizeof(request));
  if (res == sizeof(request)) {
    in_addr nat_addr = mapper.Map(request);
    SetResponse(nat_addr);
  } else if (res > 0) {
    SetInvalidResponse();
  } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
    throw connection_exception();
  }
}

void Server::Connection::HandleWriteable() {
  assert(write_pending_);
  errno = 0;
  ssize_t ret = write(fd_, &response_, sizeof(response_));
  if (ret == sizeof(response_)) {
    write_pending_ = false;
  } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
    throw connection_exception();
  }
}

void Server::Connection::SetResponse(const in_addr &addr) {
  write_pending_ = true;
  response_ = addr;
}

void Server::Connection::SetInvalidResponse() {
  write_pending_ = true;
  // XXX we assume a zero in_addr doesn't represent any valid address.
  memset(&response_, 0, sizeof(response_));   
}

} // namespace ipremapd
