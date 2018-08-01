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

#include <mini-os/os.h>
#include <mini-os/sched.h>
#include <mini-os/events.h>
#include <mini-os/console.h>
#include <mini-os/gnttab.h>
#include <mini-os/time.h>
#include <time.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/callback.h>

void _exit(int);
int errno;
static char *argv[] = { "mirage", NULL };
static unsigned long irqflags;

s_time_t not_running_time = 0;

/* XXX TODO: keep in sync with mirage-xen-minios */
void setup_xen_features(void);

CAMLprim value
caml_block_domain(value v_until)
{
  CAMLparam1(v_until);
  s_time_t t0, t1;
  t0 = monotonic_clock();
  block_domain((s_time_t)(Int64_val(v_until)));
  t1 = monotonic_clock();
  not_running_time += t1 - t0;
  CAMLreturn(Val_unit);
}

void app_main(void *unused)
{
  printk("saving IRQs\n");
  local_irq_save(irqflags);
  printk("starting OCaml\n");
  caml_startup(argv);
  _exit(0);
}

void minios_show_banner(void)
{
  printk("\x1b[32;1mMirageOS booting...\x1b[0m\n");
}

void start_kernel(void* nonsense)
{
  /* Set up events. */
  init_events();

  /* Enable event delivery. This is disabled at start of day. */
  local_irq_enable();

  setup_xen_features();

  /* Init memory management.
   * Needed for malloc. */
  init_mm();

  printk("memory manager initialized.  There are %lu pages total in the heap, and %lu used\n", minios_heap_pages_total, minios_heap_pages_used);

  /* Init time and timers. Needed for block_domain. */
  init_time();
  not_running_time = monotonic_clock();

  /* Init the console driver.
   * We probably do need this if we want printk to send notifications correctly. */
  init_console();

  /* Init grant tables. */
  init_gnttab();

  /* unlike default minios kernel.c, we don't call init_sched() or init_xenbus(),
   * as we don't want to use the minios scheduler and we have our own OCaml xenbus code
   * that we want to run via the OCaml runtime */

  printk("calling app_main(NULL)\n");

  /* Call our main function directly, without using Mini-OS threads. */
  app_main(NULL);
}

void _exit(int ret)
{
  stop_kernel();
  if (!ret) {
    /* No problem, just shutdown.  */
    struct sched_shutdown sched_shutdown = { .reason = SHUTDOWN_poweroff };
    HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
  }
  do_exit();
}
