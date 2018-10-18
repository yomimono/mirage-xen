/*
 * Copyright (C) Citrix Systems Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only. with the special
 * exception on linking described in file LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */
#include <stdint.h>
#include <common/hypervisor.h>
#include <xen/sched.h>
#include <caml/mlvalues.h>
#include <caml/memory.h>

#include <uk/print.h> //uk_printk

void unmap_shared_info();
void init_time();
void arch_rebuild_p2m();
void setup_xen_features(void);
void init_events(void);

/* Assembler interface fns in entry.S. */
void hypervisor_callback(void);
void failsafe_callback(void);

static unsigned int reasons[] = {
  SHUTDOWN_poweroff,
  SHUTDOWN_reboot,
  SHUTDOWN_suspend,
  SHUTDOWN_crash
};

CAMLprim value
stub_sched_shutdown(value v_reason)
{
    CAMLparam1(v_reason);
    struct sched_shutdown sched_shutdown = { .reason = reasons[Int_val(v_reason)] };
    HYPERVISOR_sched_op(SCHEDOP_shutdown, &sched_shutdown);
    CAMLreturn(Val_unit);
}

CAMLprim value
stub_hypervisor_suspend(value unit)
{
  CAMLparam0();
  int cancelled;

  uk_pr_warn("WARNING: stub_hypervisor_suspend not yet implemented\n");
  cancelled = 1;
  CAMLreturn(Val_int(cancelled));
}
