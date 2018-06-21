/*
 * Copyright (c) 2011 Anil Madhavapeddy <anil@recoil.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef __X86_64__
#include <xen-x86/os.h>
#endif
#ifdef __ARM32__
#include <xen-arm/os.h>
#endif
#include <xen/sched.h>
#include <common/console.h>
#include <common/events.h>
#include <common/gnttab.h>
#include <uk/plat/console.h>
#include <uk/plat/time.h>
#include <uk/plat/bootstrap.h>
#include <time.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/callback.h>

#include <uk/print.h>

void _exit(int);
int errno;
static char *argv[] = { "mirage", NULL };
static unsigned long irqflags;

typedef int64_t s_time_t; //TODO this is copied from minios-xen time.t; unikraft uses s_time_t only internally and only then for arm

s_time_t not_running_time = 0;

CAMLprim value
caml_block_domain(value v_until)
{
  CAMLparam1(v_until);
  s_time_t t0, t1;
  t0 = ukplat_monotonic_clock();
  block_domain((s_time_t)(Int64_val(v_until)));
  t1 = ukplat_monotonic_clock();
  not_running_time += t1 - t0;
  CAMLreturn(Val_unit);
}

void app_main(void *unused)
{
  local_irq_save(irqflags);
  caml_startup(argv);
  _exit(0);
}

void start_kernel(void* nonsense)
{
  /* Set up events. */
  init_events();

  /* Enable event delivery. This is disabled at start of day. */
  local_irq_enable();

  //setup_xen_features();

  /* MCP - pretty sure we don't actually need to do our own memory init, and it might
   * be harmful to do so */

  /* Init time and timers. Needed for block_domain. */
  ukplat_time_init();
  not_running_time = ukplat_monotonic_clock();

  /* Init the console driver.
   * We probably do need this if we want printk to send notifications correctly. */
  _libxenplat_init_console();

  /* Init grant tables. */
  gnttab_init();

  /* Call our main function directly, without using Mini-OS threads. */
  app_main(NULL);
}

void _exit(int ret)
{
  ukplat_terminate(UKPLAT_HALT);
}
ssize_t write(int fd, const void* buf, size_t sz) {
  //if (fd == 1 || fd == 2) {
    uk_printd("%s\n", ((char*) buf));
    return ((int) sz);
  //}
  //errno = ENOSYS;
  //return -1;
}
