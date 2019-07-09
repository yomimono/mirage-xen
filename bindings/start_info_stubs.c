/*
 * Copyright (c) 2012 Citrix Systems Inc
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/bigarray.h>

#include <uk/assert.h>
#include <xen/xen.h>
#include <common/hypervisor.h> //for hvm_get_parameter
#include <uk/print.h> //for printk
#if defined(__X86_64__) || defined(__X86_32__)
#include <xen-x86/mm.h>
#endif
#if defined(__ARM32__) || defined(__ARM_64__)
#include <xen-arm/mm.h>
#endif

#ifndef CONFIG_PARAVIRT
#include <xen/hvm/params.h>
#endif

#ifdef CONFIG_PARAVIRT
#include <xen-x86/setup.h>
CAMLprim value
stub_start_info_get(value unit)
{
  CAMLparam1(unit);
  CAMLlocal2(result, tmp);
  char buf[MAX_GUEST_CMDLINE+1];

  result = caml_alloc_tuple(16);
  memcpy(buf, HYPERVISOR_start_info->magic, sizeof(HYPERVISOR_start_info->magic));
  buf[sizeof(HYPERVISOR_start_info->magic)] = 0;
  tmp = caml_copy_string(buf);
  Store_field(result, 0, tmp);
  Store_field(result, 1, Val_int(HYPERVISOR_start_info->nr_pages));
  Store_field(result, 2, Val_int(HYPERVISOR_start_info->shared_info));
  Store_field(result, 3, Val_int(HYPERVISOR_start_info->flags));
  Store_field(result, 4, Val_int(HYPERVISOR_start_info->store_mfn));
  Store_field(result, 5, Val_int(HYPERVISOR_start_info->store_evtchn));
  Store_field(result, 6, Val_int(HYPERVISOR_start_info->console.domU.mfn));
  Store_field(result, 7, Val_int(HYPERVISOR_start_info->console.domU.evtchn));
  Store_field(result, 8, Val_int(HYPERVISOR_start_info->pt_base));
  Store_field(result, 9, Val_int(HYPERVISOR_start_info->nr_pt_frames));
  Store_field(result, 10, Val_int(HYPERVISOR_start_info->mfn_list));
  Store_field(result, 11, Val_int(HYPERVISOR_start_info->mod_start));
  Store_field(result, 12, Val_int(HYPERVISOR_start_info->mod_len));
  memcpy(buf, HYPERVISOR_start_info->cmd_line, MAX_GUEST_CMDLINE);
  buf[MAX_GUEST_CMDLINE] = 0;
  tmp = caml_copy_string(buf);
  Store_field(result, 13, tmp);
  Store_field(result, 14, Val_int(HYPERVISOR_start_info->first_p2m_pfn));
  Store_field(result, 15, Val_int(HYPERVISOR_start_info->nr_p2m_frames));
  CAMLreturn(result);
}
#endif

CAMLprim value
caml_cmdline(value v_unit)
{
  CAMLparam1(v_unit);
  char buf[MAX_GUEST_CMDLINE+1];
#ifdef CONFIG_PARAVIRT
  memcpy(buf, HYPERVISOR_start_info->cmd_line, MAX_GUEST_CMDLINE);
#endif /* CONFIG_PARAVIRT */
  buf[MAX_GUEST_CMDLINE] = 0;
  CAMLreturn(caml_copy_string(buf));
}

CAMLprim value
caml_console_start_page(value v_unit)
{
  CAMLparam1(v_unit);
#ifdef CONFIG_PARAVIRT
  CAMLreturn(caml_ba_alloc_dims(CAML_BA_UINT8 | CAML_BA_C_LAYOUT,
                                1,
                                mfn_to_virt(HYPERVISOR_start_info->console.domU.mfn),
                                (long)PAGE_SIZE));
#else /* CONFIG_PARAVIRT */
  uint64_t console;
  ///hvm_get_parameter is missing, so construct our own call to HYPERVISOR_hvm_op
  //code copied from hypervisor.c in minios-xen
  struct xen_hvm_param xhv;
  int ret;
  
  xhv.domid = DOMID_SELF; //known from grant_table.h
  xhv.index = HVM_PARAM_CONSOLE_PFN;
  ret = HYPERVISOR_hvm_op(HVMOP_get_param, &xhv);
  UK_ASSERT(ret >= 0);
  console = xhv.value;
  
  CAMLreturn(caml_ba_alloc_dims(CAML_BA_UINT8 | CAML_BA_C_LAYOUT,
                                1,
                                pfn_to_virt(console),
                                (long)PAGE_SIZE));
#endif
}

CAMLprim value caml_xenstore_event_channel(value v_unit)
{
  CAMLparam1(v_unit);
#ifdef CONFIG_PARAVIRT
  CAMLreturn (Val_int(HYPERVISOR_start_info->store_evtchn));
#else
  uint64_t evtchn;
  hvm_get_parameter(HVM_PARAM_STORE_EVTCHN, &evtchn);
  CAMLreturn (Val_int(evtchn));
#endif
}

CAMLprim value
caml_xenstore_start_page(value v_unit)
{
  CAMLparam1(v_unit);
#ifdef CONFIG_PARAVIRT
  CAMLreturn(caml_ba_alloc_dims(CAML_BA_UINT8 | CAML_BA_C_LAYOUT,
                                1,
                                mfn_to_virt(HYPERVISOR_start_info->store_mfn),
                                (long)PAGE_SIZE));
#else /* CONFIG_PARAVIRT */
  uint64_t store;
  store = hvm_get_parameter(HVM_PARAM_STORE_PFN, &store);
  /* this won't work - you need to put this into the page table, which isn't exposed in unikraft API */
  CAMLreturn(caml_ba_alloc_dims(CAML_BA_UINT8 | CAML_BA_C_LAYOUT,
                                1,
                                (void *)pfn_to_virt(store),
                                (long)PAGE_SIZE));
#endif /* CONFIG_PARAVIRT */
}
