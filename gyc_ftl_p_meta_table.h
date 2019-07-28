#ifndef __GYC_FTL_P_META_TABLE_H__
#define __GYC_FTL_P_META_TABLE_H__

#include "gyc_ftl_typedef.h"

typedef struct p_meta_table {
	ppn_t num_of_all_pps;
	pbn_t num_of_all_pbs;
	ppn_t num_of_pps_per_pb;

	pb_meta_t *pb_meta_array;
	pp_meta_t *pp_meta_array;

	pbn_t num_of_free_pbs;
	pbn_t num_of_head_pbs;
	pbn_t num_of_body_pbs;
	pbn_t num_of_tail_pbs;
} p_meta_table_t;

p_meta_table_t *create_p_meta_table(pbn_t num_of_all_pbs, ppn_t num_of_pps_per_pb);

/* =========================================== */
/* 3 Native APIs in Physical Block Granularity */
/* =========================================== */

ppn_t alloc_new_pp_in_pb(p_meta_table_t *curr, pbn_t pb_to_store_new_pp, lpn_t recorded_lpn_in_new_pp);
void delete_pp_in_pb(p_meta_table_t *p_meta_table, pbn_t pbn, ppn_t pp_to_be_deleted);
void erase_pb(p_meta_table_t *p_meta_table, pbn_t pbn);

/* extended version API of delete_pp_in_pb */
void delete_pp_recorded_given_lpn(p_meta_table_t *p_meta_table, lpn_t lpn, ppn_t pp_recorded_given_lpn);

/* secret API: in the next version, these functions will be moved to private */
void mark_as_head_pb(p_meta_table_t *p_meta_table, pbn_t pbn);
void mark_as_body_pb(p_meta_table_t *p_meta_table, pbn_t pbn);
void mark_as_tail_pb(p_meta_table_t *p_meta_table, pbn_t pbn);
void mark_as_free_pb(p_meta_table_t *p_meta_table, pbn_t pbn);

/* Some short-cut method */
lpn_t find_lpn_by_ppn(const p_meta_table_t *p_meta_table, ppn_t ppn);
pbn_t find_pbn_by_ppn(const p_meta_table_t *p_meta_table, ppn_t ppn);

#endif

