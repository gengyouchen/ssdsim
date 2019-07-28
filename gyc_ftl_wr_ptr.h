#ifndef __GYC_FTL_WR_PTR_H__
#define __GYC_FTL_WR_PTR_H__

#include "gyc_ftl_typedef.h"
#include "gyc_ftl_p_meta_table.h"

typedef enum wr_ptr_mode {
	SLC_MODE,
	MLC_MODE
} wr_ptr_mode_t;

typedef struct wr_ptr {
	pbn_t ptr_to_head_pb;
	ppn_t tot_num_of_wr_ops;
	wr_ptr_mode_t mode;
} wr_ptr_t;

wr_ptr_t *create_wr_ptr(p_meta_table_t *p_meta_table, wr_ptr_mode_t mode);
ppn_t write_lp_to_wr_ptr(p_meta_table *p_meta_table, wr_ptr_t *wr_ptr, lpn_t recorded_lpn_in_new_pp);
void write_dummy_to_wr_ptr(p_meta_table *p_meta_table, wr_ptr_t *wr_ptr);

#endif

