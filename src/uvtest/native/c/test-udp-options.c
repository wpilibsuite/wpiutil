/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv/uv.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int udp_options_test(const struct sockaddr* addr) {
  static int invalid_ttls[] = { -1, 0, 256 };
  uv_loop_t* loop;
  uv_udp_t h;
  int i, r;

  loop = uv_default_loop();

  r = uv_udp_init(loop, &h);
  ASSERT(r == 0);

  uv_unref((uv_handle_t*)&h); /* don't keep the loop alive */

  r = uv_udp_bind(&h, addr, 0);
  ASSERT(r == 0);

  r = uv_udp_set_broadcast(&h, 1);
  r |= uv_udp_set_broadcast(&h, 1);
  r |= uv_udp_set_broadcast(&h, 0);
  r |= uv_udp_set_broadcast(&h, 0);
  ASSERT(r == 0);

  /* values 1-255 should work */
  for (i = 1; i <= 255; i++) {
    r = uv_udp_set_ttl(&h, i);
#if defined(__MVS__)
    if (addr->sa_family == AF_INET6)
      ASSERT(r == 0);
    else
      ASSERT(r == UV_ENOTSUP);
#else
    ASSERT(r == 0);
#endif
  }

  for (i = 0; i < (int) ARRAY_SIZE(invalid_ttls); i++) {
    r = uv_udp_set_ttl(&h, invalid_ttls[i]);
    ASSERT(r == UV_EINVAL);
  }

  r = uv_udp_set_multicast_loop(&h, 1);
  r |= uv_udp_set_multicast_loop(&h, 1);
  r |= uv_udp_set_multicast_loop(&h, 0);
  r |= uv_udp_set_multicast_loop(&h, 0);
  ASSERT(r == 0);

  /* values 0-255 should work */
  for (i = 0; i <= 255; i++) {
    r = uv_udp_set_multicast_ttl(&h, i);
    ASSERT(r == 0);
  }

  /* anything >255 should fail */
  r = uv_udp_set_multicast_ttl(&h, 256);
  ASSERT(r == UV_EINVAL);
  /* don't test ttl=-1, it's a valid value on some platforms */

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(udp_options) {
  struct sockaddr_in addr;

  ASSERT(0 == uv_ip4_addr("0.0.0.0", TEST_PORT, &addr));
  return udp_options_test((const struct sockaddr*) &addr);
}


TEST_IMPL(udp_options6) {
  struct sockaddr_in6 addr;

  if (!can_ipv6())
    RETURN_SKIP("IPv6 not supported");

  ASSERT(0 == uv_ip6_addr("::", TEST_PORT, &addr));
  return udp_options_test((const struct sockaddr*) &addr);
}


TEST_IMPL(udp_no_autobind) {
  uv_loop_t* loop;
  uv_udp_t h;

  loop = uv_default_loop();

  ASSERT(0 == uv_udp_init(loop, &h));
  ASSERT(UV_EBADF == uv_udp_set_multicast_ttl(&h, 32));
  ASSERT(UV_EBADF == uv_udp_set_broadcast(&h, 1));
#if defined(__MVS__)
  ASSERT(UV_ENOTSUP == uv_udp_set_ttl(&h, 1));
#else
  ASSERT(UV_EBADF == uv_udp_set_ttl(&h, 1));
#endif
  ASSERT(UV_EBADF == uv_udp_set_multicast_loop(&h, 1));
  ASSERT(UV_EBADF == uv_udp_set_multicast_interface(&h, "0.0.0.0"));

  uv_close((uv_handle_t*) &h, NULL);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  MAKE_VALGRIND_HAPPY();
  return 0;
}
