/*
 * Copyright (c) 2010 Anil Madhavapeddy <anil@recoil.org>
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
#include <xen/xen.h>
#include <time.h>
#include <sys/time.h>
#include <uk/plat/time.h>
#include <uk/print.h>
#include <common/hypervisor.h>

#include <plat/common/include/x86/cpu.h> //TODO: this will break on ARM

#include <caml/mlvalues.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/fail.h>

int gettimeofday(struct timeval *tv, struct timezone *tz) {
	// the only caller should be OCaml's Random module attempting to seed itself
	// for now, just crash if that's invoked.
	uk_pr_err("gettimeofday invoked");
	abort();
	return -1;
}

void times(void *buf) {
	uk_pr_err("times invoked");
	abort();
}

CAMLprim value
unix_gettimeofday(value v_unit)
{
  struct vcpu_time_info *src;
  unsigned long uptime, tsc;
  CAMLparam1(v_unit);
  /* according to documentation, we need to calculate the uptime then add that to wc_sec and wc_nsec */

  src = &HYPERVISOR_shared_info->vcpu_info[0].time;
  // TODO: this is x86-specific
  tsc = rdtsc();

  uptime = src->system_time +
	  ((((tsc - src->tsc_timestamp) << src->tsc_shift) * src->tsc_to_system_mul) >> 32);

  uk_pr_info("system time is %lu. tsc is %lu , src->tsc_timestamp %lu. uptime appears to be %lu nsec\n", src->system_time, tsc, src->tsc_timestamp, uptime);

  CAMLreturn(caml_copy_double((double) (HYPERVISOR_shared_info->wc_sec)
			  + (double) ((HYPERVISOR_shared_info->wc_nsec + uptime)/ 1e9)));
}

CAMLprim value
caml_get_monotonic_time(value v_unit)
{
  unsigned long time;
  CAMLparam1(v_unit);
  time = ukplat_monotonic_clock();
  CAMLreturn(caml_copy_int64(time));
}
/*
static value alloc_tm(struct tm *tm)
{
  value res;
  res = caml_alloc_small(9, 0);
  Field(res,0) = Val_int(tm->tm_sec);
  Field(res,1) = Val_int(tm->tm_min);
  Field(res,2) = Val_int(tm->tm_hour);
  Field(res,3) = Val_int(tm->tm_mday);
  Field(res,4) = Val_int(tm->tm_mon);
  Field(res,5) = Val_int(tm->tm_year);
  Field(res,6) = Val_int(tm->tm_wday);
  Field(res,7) = Val_int(tm->tm_yday);
  Field(res,8) = tm->tm_isdst ? Val_true : Val_false;
  return res;
}

CAMLprim value
unix_gmtime(value t)
{
  CAMLparam1(t);
  time_t clock;
  struct tm * tm;
  clock = (time_t) Double_val(t);
  tm = gmtime(&clock);
  if (tm == NULL) caml_failwith("gmtime");
  CAMLreturn(alloc_tm(tm));
} */
