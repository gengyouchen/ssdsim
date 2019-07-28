#include "gyc_ftl_wr_ptr.h"

#include <assert.h>
#include <stdlib.h>

#include "gyc_ftl_config.h"
#include "gyc_ftl_typedef.h"
#include "gyc_ftl_assert.h"
#include "gyc_ftl_foreach.h"
#include "gyc_ftl_p_meta_table.h"

void assign_wr_ptr(p_meta_table_t *p_meta_table, wr_ptr_t *wr_ptr);

wr_ptr_t *create_wr_ptr(p_meta_table_t *p_meta_table, wr_ptr_mode_t mode) {

	wr_ptr_t *wr_ptr = (wr_ptr_t *) malloc(sizeof(wr_ptr_t));
	assert(wr_ptr);

	wr_ptr->ptr_to_head_pb = NULL_PBN;
	wr_ptr->mode = mode;
	wr_ptr->tot_num_of_wr_ops = 0;

	assign_wr_ptr(p_meta_table, wr_ptr);
	return wr_ptr;
}

ppn_t write_lp_to_wr_ptr(p_meta_table *p_meta_table, wr_ptr_t *wr_ptr, lpn_t recorded_lpn_in_new_pp) {

	assert(wr_ptr);

	ppn_t num_of_dummy_pps = 0;

	switch (wr_ptr->mode) {
		case SLC_MODE:
			num_of_dummy_pps = BIT_PER_MLC - 1;
			break;
		case MLC_MODE:
			num_of_dummy_pps = 0;
			break;
		default:
			assert(0);
			break;
	};

	ASSERT_IS_VALID_HEAD_PB(p_meta_table, wr_ptr->ptr_to_head_pb);
	switch (wr_ptr->mode) {
		case SLC_MODE:
			assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode == PB_SLC);
			break;
		case MLC_MODE:
			assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode == PB_MLC);
			break;
		default:
			assert(0);
			break;
	};

	/* currently, we don't allow any unaligned write */
	assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].num_of_free_pages % (1 + num_of_dummy_pps) == 0);

	/* perform one normal write (i.e. write with the given logical page data) */
	assert(recorded_lpn_in_new_pp != NULL_LPN);
	ppn_t new_pp = alloc_new_pp_in_pb(p_meta_table, wr_ptr->ptr_to_head_pb, recorded_lpn_in_new_pp);
	assign_wr_ptr(p_meta_table, wr_ptr);

	/* perform appending dummy writes */
	for (ppn_t i = 0; i < num_of_dummy_pps; i++) {
		ppn_t dummy_pp = alloc_new_pp_in_pb(p_meta_table, wr_ptr->ptr_to_head_pb, NULL_LPN);
		assign_wr_ptr(p_meta_table, wr_ptr);
	}

	wr_ptr->tot_num_of_wr_ops++;

	/* return the PPN of the normal write for caller to update its LPN-to-PPN table */
	return new_pp;
}

void write_dummy_to_wr_ptr(p_meta_table *p_meta_table, wr_ptr_t *wr_ptr) {

	assert(wr_ptr);

	ppn_t num_of_dummy_pps = 0;
	switch (wr_ptr->mode) {
		case SLC_MODE:
			num_of_dummy_pps = BIT_PER_MLC;
			break;
		case MLC_MODE:
			num_of_dummy_pps = 1;
			break;
		default:
			assert(0);
			break;
	};

	ASSERT_IS_VALID_HEAD_PB(p_meta_table, wr_ptr->ptr_to_head_pb);
	switch (wr_ptr->mode) {
		case SLC_MODE:
			assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode == PB_SLC);
			break;
		case MLC_MODE:
			assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode == PB_MLC);
			break;
		default:
			assert(0);
			break;
	};

	/* currently, we don't allow any unaligned write */
	assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].num_of_free_pages % num_of_dummy_pps == 0);

	/* perform dummy writes */
	for (ppn_t i = 0; i < num_of_dummy_pps; i++) {
		ppn_t dummy_pp = alloc_new_pp_in_pb(p_meta_table, wr_ptr->ptr_to_head_pb, NULL_LPN);
		assign_wr_ptr(p_meta_table, wr_ptr);
	}

	wr_ptr->tot_num_of_wr_ops++;
}

void assign_wr_ptr(p_meta_table_t *p_meta_table, wr_ptr_t *wr_ptr) {

	if (wr_ptr->ptr_to_head_pb != NULL_PBN) {
		switch (wr_ptr->mode) {
			case SLC_MODE:
				assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode == PB_SLC);
				break;
			case MLC_MODE:
				assert(p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode == PB_MLC);
				break;
			default:
				assert(0);
				break;
		};
		if (p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].state == PB_HEAD) {
			ASSERT_IS_VALID_HEAD_PB(p_meta_table, wr_ptr->ptr_to_head_pb);
			return;
		}
		ASSERT_IS_VALID_BODY_PB(p_meta_table, wr_ptr->ptr_to_head_pb);
	}

	/* perform dynamic wear-leveling */
	pbn_t next_pb = NULL_PBN;
	pbn_t min_tot_num_of_er_ops = 0xffffffff;
	FOR_EACH_FREE_PB(p_meta_table, pbn, pb_meta) {
		ASSERT_IS_VALID_FREE_PB(p_meta_table, pbn);
		if (pb_meta->tot_num_of_er_ops < min_tot_num_of_er_ops) {
			next_pb = pbn;
			min_tot_num_of_er_ops = pb_meta->tot_num_of_er_ops;
		}
	} END_FOR_EACH_FREE_PB;
	assert(next_pb != NULL_PBN);

	mark_as_head_pb(p_meta_table, next_pb);
	wr_ptr->ptr_to_head_pb = next_pb;
	switch (wr_ptr->mode) {
		case SLC_MODE:
			p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode = PB_SLC;
			break;
		case MLC_MODE:
			p_meta_table->pb_meta_array[wr_ptr->ptr_to_head_pb].mode = PB_MLC;
			break;
		default:
			assert(0);
			break;
	};
}

