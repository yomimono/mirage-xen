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
#include <stdint.h>
#include <_time.h> //for time_block_until, unpleasantly
#include <xen/sched.h> //for inlined ukplat_entry_argp
#include <common/console.h>
#include <common/events.h>
#include <common/gnttab.h>
#include <uk/plat/console.h>
#include <uk/plat/time.h>
#include <uk/plat/bootstrap.h>
#include <time.h>

#include <uk/plat/lcpu.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/callback.h>

#include <uk/print.h>

#define HAVE_BOOTENTRY

void exit(int);
void abort(); //TODO: some error output here, if we can do it without invoking too much potentially failing stuff
//also TODO: see whether there's a better exit function from unikraft API to be calling
ssize_t write(int, const void*, size_t);
int errno;
static char *our_argv[] = { "mirage", NULL };
unsigned long irqflags;

typedef int64_t s_time_t; //TODO this is copied from minios-xen time.t; unikraft uses s_time_t only internally and only then for arm

s_time_t not_running_time = 0;

CAMLprim value
caml_block_domain(value v_until)
{
  CAMLparam1(v_until);
  s_time_t t0, t1;
  t0 = ukplat_monotonic_clock();
  time_block_until((s_time_t)(Int64_val(v_until)));
  t1 = ukplat_monotonic_clock();
  not_running_time += t1 - t0;
  CAMLreturn(Val_unit);
}
int main(int argc __unused, char *argv[] __unused)
{
  irqflags = ukplat_lcpu_save_irqf();

  uk_pr_debug("Starting OCaml...\n");

  caml_startup(our_argv);
  exit(0);
}

void exit(int ret)
{
  uk_pr_debug("exiting with code %d\n", ret);
  ukplat_terminate(UKPLAT_HALT);
}

void abort()
{
  uk_pr_err("ABORT\n");
  ukplat_terminate(UKPLAT_HALT);
}

ssize_t write(int fd, const void* buf, size_t sz) {
  if (fd == 1 || fd == 2) {
    ((char *)buf)[sz] = '\0';
    uk_printd("%s\n", ((char*) buf));
    return ((int) sz);
  }
  errno = ENOSYS;
  return -1;
}
