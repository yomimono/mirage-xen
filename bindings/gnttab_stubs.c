/*
 * Copyright (c) 2010-2011 Anil Madhavapeddy <anil@recoil.org>
 * Copyright (c) 2006 Steven Smith <sos22@cam.ac.uk>
 * Copyright (c) 2006 Grzegorz Milos <gm281@cam.ac.uk>
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

#if defined(__X86_64__) || defined(__X86_32__)
#include <xen-x86/mm.h>
#endif
#if defined(__ARM_64__) || defined(__ARM_32__)
#include <xen-arm/mm.h>
#endif

#include <uk/alloc.h>

#include <common/gnttab.h>
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/bigarray.h>
#include <caml/alloc.h>
#include <caml/fail.h>

#include <xen/grant_table.h>

#include <uk/print.h>

struct gntmap *map = NULL;

/* Defined in unikraft gnttab.c: */
extern grant_entry_t *gnttab_table;

/* Note in particular EXTERNAL rather than MANAGED: we don't want anyone to
   call free() on a grant mapping -- nothing good can come of that. To quote
   mini-os: "If instead you choose to disregard this message, I insist that
   you keep an eye out for raptors."
 */
#define XC_GNTTAB_BIGARRAY (CAML_BA_UINT8 | CAML_BA_C_LAYOUT | CAML_BA_EXTERNAL)

//these definitions were in minios-xen include/gnttab.h but aren't exposed in unikraft
#define NR_GRANT_FRAMES 4
#define NR_GRANT_ENTRIES (NR_GRANT_FRAMES * PAGE_SIZE / sizeof(grant_entry_t))

void gnttab_init(void)
{
	CAMLparam1(unit);
	CAMLlocal1(result);
	if (!map) {
                //get the allocator from unikraft
                struct uk_alloc *allocator;
                allocator = uk_alloc_get_default();
		map = (struct gntmap*) uk_malloc(allocator, sizeof(struct gntmap));
		gntmap_init(map);
		uk_pr_crit("initialised mini-os^Wunikraft gntmap\n");
	}
	result = Val_unit;
	CAMLreturn(result);
}

CAMLprim value stub_gnttab_interface_close(value unit)
{
	CAMLparam1(unit);
	CAMLreturn(Val_unit);
}

CAMLprim value stub_gnttab_allocates(void)
{
	CAMLparam0();
	/* The mini-os API is now the same as the userspace Linux one: it
	   returns fresh granted pages rather than expecting us to pass in
	   an existing heap page. FIXME: remove the 'map_onto' API completely */
	CAMLreturn(Val_bool(1));
}

CAMLprim value stub_gntshr_allocates(void)
{
	CAMLparam0();
	/* We still manage our grant references from the OCaml code */
	CAMLreturn(Val_bool(0));
}

static void *
base_page_of(value v_iopage)
{
    /* The grant API takes page-aligned addresses. */
    struct caml_ba_array *a = (struct caml_ba_array *)Caml_ba_array_val(v_iopage);
    unsigned long page_aligned_view = (unsigned long)a->data & ~(PAGE_SIZE - 1);
    return (void*) page_aligned_view;
}

/* The 'type grant_handle' is opaque and contains all the information we need
   to be able to unmap the grant. The mini-os API needs the 'start_address'
   and 'count' */

CAMLprim value
stub_gnttab_unmap(value i, value v_handle)
{
  CAMLparam2(i, v_handle);
  unsigned long start_address = Int_val(Field(v_handle, 0));
  int count = Int_val(Field(v_handle, 1));
  if (gntmap_munmap(map, start_address, count)) {
    caml_failwith("gntmap_munmap failed");
  }

  CAMLreturn(Val_unit);
}

CAMLprim value stub_gnttab_map_fresh(value i, value r, value d, value w)
{
    CAMLparam4(i, r, d, w);
    CAMLlocal3(contents, pair, handle);
    void *mapping;
    uint32_t domids[1];
    uint32_t refs[1];
    domids[0] = Int_val(d);
    refs[0] = Int_val(r);
    mapping = gntmap_map_grant_refs(map, 1, domids, 1, refs, Bool_val(w));
    if (mapping==NULL) {
            caml_failwith("stub_gnttab_map_fresh: failed to map grant ref");
    }

    handle = caml_alloc_tuple(2);
    Store_field(handle, 0, Val_int(mapping)); /* FIXME: this is an unsigned long */
    Store_field(handle, 1, Val_int(1));       /* count */
    contents = caml_ba_alloc_dims(XC_GNTTAB_BIGARRAY, 1, mapping, 1 * PAGE_SIZE);
    pair = caml_alloc_tuple(2);
    Store_field(pair, 0, handle);             /* grant_handle */
    Store_field(pair, 1, contents);           /* Io_page.t */
    CAMLreturn(pair);
}

CAMLprim value stub_gnttab_mapv_batched(value xgh, value array, value writable)
{
    CAMLparam3(xgh, array, writable);
    CAMLlocal3(contents, pair, handle);
    int count = Wosize_val(array) / 2;
    uint32_t domids[count];
    uint32_t refs[count];
    int i;

    for (i = 0; i < count; i++){
            domids[i] = Int_val(Field(array, i * 2 + 0));
            refs[i] = Int_val(Field(array, i * 2 + 1));
    }
    void *mapping = gntmap_map_grant_refs(map, count, domids, 1, refs, Bool_val(writable));
    if(mapping==NULL) {
            caml_failwith("stub_gnttab_mapv_batched: failed to map grant ref");
    }
    handle = caml_alloc_tuple(2);
    Store_field(handle, 0, Val_int(mapping)); /* FIXME: this is an unsigned long */
    Store_field(handle, 1, Val_int(count));

    contents = caml_ba_alloc_dims(XC_GNTTAB_BIGARRAY, 1, mapping, count * PAGE_SIZE);
    pair = caml_alloc_tuple(2);
    Store_field(pair, 0, handle); /* grant_handle */
    Store_field(pair, 1, contents); /* Io_page.t */
    CAMLreturn(pair);
}

/* Return the number of reserved grant entries at the start */
CAMLprim value
stub_gnttab_reserved(value unit)
{
    return Val_int(GNTTAB_NR_RESERVED_ENTRIES);
}

CAMLprim value
stub_gnttab_nr_entries(value unit)
{
    return Val_int(NR_GRANT_ENTRIES);
}

/* Exporting (sharing) pages */

/* FIXME: we should use the mini-OS functions directly for these */

static void
gntshr_grant_access(grant_ref_t ref, void *page, int domid, int ro)
{
    gnttab_table[ref].frame = virt_to_mfn(page);
    gnttab_table[ref].domid = domid;
    wmb();
    gnttab_table[ref].flags = GTF_permit_access | (ro * GTF_readonly);
}

CAMLprim value
stub_gntshr_grant_access(value v_ref, value v_iopage, value v_domid, value v_writable)
{
    grant_ref_t ref = Int_val(v_ref);
    void *page = base_page_of(v_iopage);
    if (Caml_ba_data_val(v_iopage) != page)
       caml_failwith("gntshr_grant_access: shared page not page-aligned");
    gntshr_grant_access(ref, page, Int_val(v_domid), !Bool_val(v_writable));

    return Val_unit;
}

CAMLprim value
stub_gntshr_try_end_access(value v_ref)
{
    CAMLparam1(v_ref);
    grant_ref_t ref = Int_val(v_ref);
    CAMLreturn(Val_bool(gnttab_end_access(ref)));
}
