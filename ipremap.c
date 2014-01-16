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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char *argv[]) {
  int ret = EXIT_FAILURE, res, fd;
  struct sockaddr_un sa;
  struct in_addr orig_addr, nat_addr;
  char buf[INET_ADDRSTRLEN];

  if (argc < 2) {
    fprintf(stderr, "usage: %s <address>\n", argv[0]);
    goto out1;
  }
  res = inet_pton(AF_INET, argv[1], &orig_addr);
  if (res < 0) {
    perror("inet_pton()");
    goto out1;
  } else if (res == 0) {
    fprintf(stderr, "invalid address: %s\n", argv[1]);
    goto out1;
  }
  fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (fd < 0) {
    perror("socket()");
    goto out1;
  }
  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strcpy(sa.sun_path, "ipremap.sock");
  if (connect(fd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
    perror("connect()");
    goto out2;
  }
  if (write(fd, &orig_addr, sizeof(orig_addr)) != sizeof(orig_addr)) {
    perror("write()");
    goto out2;
  }
  if (read(fd, &nat_addr, sizeof(nat_addr)) != sizeof(nat_addr)) {
    perror("read()");
    goto out2;
  }
  if (inet_ntop(AF_INET, &nat_addr, buf, INET_ADDRSTRLEN) == NULL) {
    perror("inet_ntop()");
    goto out2;
  }
  printf("%s\n", buf);
  ret = EXIT_SUCCESS;

out2:
  close(fd);
out1:
  return ret;
}
