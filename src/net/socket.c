/* SPDX-License-Identifier: MIT */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200112L /* getaddrinfo, select under -std=c11 */
#endif
#include "net/socket.h"

#include <stdio.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)

bool zb_sock_supported(void) { return false; }
zb_socket zb_sock_listen(uint16_t port, uint16_t *bound_port) {
  (void)port;
  (void)bound_port;
  return ZB_SOCKET_NONE;
}
zb_socket zb_sock_accept(zb_socket listener) {
  (void)listener;
  return ZB_SOCKET_NONE;
}
zb_socket zb_sock_connect(const char *host, uint16_t port) {
  (void)host;
  (void)port;
  return ZB_SOCKET_NONE;
}
int zb_sock_poll_connect(zb_socket s) {
  (void)s;
  return -1;
}
long zb_sock_send(zb_socket s, const void *data, size_t len) {
  (void)s;
  (void)data;
  (void)len;
  return ZB_SOCK_CLOSED;
}
long zb_sock_recv(zb_socket s, void *buf, size_t cap) {
  (void)s;
  (void)buf;
  (void)cap;
  return ZB_SOCK_CLOSED;
}
void zb_sock_close(zb_socket s) { (void)s; }

#else

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET native_socket;
#define NATIVE_INVALID INVALID_SOCKET
static bool would_block(void) {
  int e = WSAGetLastError();
  return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS;
}
static void close_native(native_socket s) { closesocket(s); }
static bool set_nonblocking(native_socket s) {
  u_long on = 1;
  return ioctlsocket(s, FIONBIO, &on) == 0;
}
static void startup(void) {
  static bool started = false;
  if (!started) {
    WSADATA data;
    started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }
}
#define SEND_FLAGS 0
#define LEN_ARG(x) ((int)(x))
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int native_socket;
#define NATIVE_INVALID (-1)
static bool would_block(void) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS; }
static void close_native(native_socket s) { close(s); }
static bool set_nonblocking(native_socket s) {
  int flags = fcntl(s, F_GETFL, 0);
  return flags >= 0 && fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
}
static void startup(void) {}
#define LEN_ARG(x) (x)
#if defined(MSG_NOSIGNAL)
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif
#endif

static native_socket native(zb_socket s) { return (native_socket)s; }

static void no_delay(native_socket s) {
  int on = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof on);
}

bool zb_sock_supported(void) { return true; }

zb_socket zb_sock_listen(uint16_t port, uint16_t *bound_port) {
  startup();
  native_socket s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == NATIVE_INVALID) {
    return ZB_SOCKET_NONE;
  }
  int on = 1;
#ifdef _WIN32
  /* On Windows SO_REUSEADDR would let a second server take the port; exclusive use refuses it. */
  setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&on, sizeof on);
#else
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof on); /* quick restarts */
#endif
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port);
  if (bind(s, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(s, 8) != 0 || !set_nonblocking(s)) {
    close_native(s);
    return ZB_SOCKET_NONE;
  }
  socklen_t len = sizeof addr;
  if (bound_port && getsockname(s, (struct sockaddr *)&addr, &len) == 0) {
    *bound_port = ntohs(addr.sin_port);
  }
  return (zb_socket)s;
}

zb_socket zb_sock_accept(zb_socket listener) {
  native_socket c = accept(native(listener), NULL, NULL);
  if (c == NATIVE_INVALID) {
    return ZB_SOCKET_NONE;
  }
  if (!set_nonblocking(c)) {
    close_native(c);
    return ZB_SOCKET_NONE;
  }
  no_delay(c);
  return (zb_socket)c;
}

zb_socket zb_sock_connect(const char *host, uint16_t port) {
  startup();
  char service[8];
  snprintf(service, sizeof service, "%u", (unsigned)port);
  struct addrinfo hints, *res = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, service, &hints, &res) != 0 || !res) {
    return ZB_SOCKET_NONE;
  }
  native_socket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (s == NATIVE_INVALID || !set_nonblocking(s)) {
    if (s != NATIVE_INVALID) {
      close_native(s);
    }
    freeaddrinfo(res);
    return ZB_SOCKET_NONE;
  }
  no_delay(s);
  int r = connect(s, res->ai_addr, (socklen_t)res->ai_addrlen);
  freeaddrinfo(res);
  if (r != 0 && !would_block()) {
    close_native(s);
    return ZB_SOCKET_NONE;
  }
  return (zb_socket)s;
}

int zb_sock_poll_connect(zb_socket s) {
  fd_set wr, ex;
  FD_ZERO(&wr);
  FD_ZERO(&ex);
  FD_SET(native(s), &wr);
  FD_SET(native(s), &ex);
  struct timeval tv = {0, 0};
  int n = select((int)native(s) + 1, NULL, &wr, &ex, &tv);
  if (n < 0 || FD_ISSET(native(s), &ex)) {
    return -1;
  }
  if (n == 0) {
    return 0;
  }
  int err = 0;
  socklen_t len = sizeof err;
  if (getsockopt(native(s), SOL_SOCKET, SO_ERROR, (char *)&err, &len) != 0 || err != 0) {
    return -1;
  }
  return 1;
}

long zb_sock_send(zb_socket s, const void *data, size_t len) {
  long n = (long)send(native(s), (const char *)data, LEN_ARG(len), SEND_FLAGS);
  if (n < 0) {
    return would_block() ? ZB_SOCK_WOULD_BLOCK : ZB_SOCK_CLOSED;
  }
  return n;
}

long zb_sock_recv(zb_socket s, void *buf, size_t cap) {
  long n = (long)recv(native(s), (char *)buf, LEN_ARG(cap), 0);
  if (n == 0) {
    return ZB_SOCK_CLOSED;
  }
  if (n < 0) {
    return would_block() ? ZB_SOCK_WOULD_BLOCK : ZB_SOCK_CLOSED;
  }
  return n;
}

void zb_sock_close(zb_socket s) {
  if (s != ZB_SOCKET_NONE) {
    close_native(native(s));
  }
}

#endif
