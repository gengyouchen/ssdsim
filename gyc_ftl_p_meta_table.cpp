#include "gyc_ftl_p_meta_table.h"

#include <assert.h>
#include <stdlib.h>

#include "gyc_ftl_typedef.h"
#include "gyc_ftl_assert.h"
#include "gyc_ftl_foreach.h"

p_meta_table_t *create_p_meta_table(pbn_t num_of_all_pbs, ppn_t num_of_pps_per_pb) {

	p_meta_table_t *curr = (p_meta_table_t *) malloc(sizeof(p_meta_table_t));
	assert(curr);

	curr->num_of_all_pps = num_of_pps_per_pb * num_of_all_pbs;
	curr->num_of_all_pbs = num_of_all_pbs;
	curr->num_of_pps_per_pb = num_of_pps_per_pb;

	/* allocate pp_meta_array */
	curr->pp_meta_array = (pp_meta_t *) malloc(sizeof(pp_meta_t) * curr->num_of_all_pps);
	assert(curr->pp_meta_array);

	/* allocate pb_meta_array */
	curr->pb_meta_array = (pb_meta_t *) malloc(sizeof(pb_meta_t) * curr->num_of_all_pbs);
	assert(curr->pb_meta_array);

	FOR_EACH_PP(curr, ppn, pp_meta) {

		/* initially, all physical pages are free pages */
		pp_meta->state = PP_FREE;
		pp_meta->ptr_to_logical_page = NULL_LPN;

	} END_FOR_EACH_PP;

	FOR_EACH_PB(curr, pbn, pb_meta) {

		/* initially, all physical blocks are free blocks */
		pb_meta->state = PB_FREE;
		pb_meta->ptr_to_free_page = curr->num_of_pps_per_pb * pbn;
		pb_meta->ptr_to_first_page = curr->num_of_pps_per_pb * pbn;
		pb_meta->ptr_to_last_page = curr->num_of_pps_per_pb * (pbn + 1) - 1;
		pb_meta->num_of_free_pages = curr->num_of_pps_per_pb;
		pb_meta->num_of_live_pages = 0;
		pb_meta->num_of_dead_pages = 0;
		pb_meta->mode = PB_MLC;
		ASSERT_IS_VALID_FREE_PB(curr, pbn);

		pb_meta->tot_num_of_rd_ops = 0;
		pb_meta->tot_num_of_wr_ops = 0;
		pb_meta->tot_num_of_er_ops = 0;

	} END_FOR_EACH_PB;

	/* fill consistent statistics */
	curr->num_of_free_pbs = curr->num_of_all_pbs;
	curr->num_of_head_pbs = 0;
	curr->num_of_body_pbs = 0;
	curr->num_of_tail_pbs = 0;

	return curr;
}

ppn_t alloc_new_pp_in_pb(p_meta_table_t *curr, pbn_t pb_to_store_new_pp, lpn_t recorded_lpn_in_new_pp) {

	/* from now, we only deal with a head block */
	ASSERT_IS_VALID_HEAD_PB(curr, pb_to_store_new_pp);

	/* write recorded_lpn_in_new_pp to the pp_meta_array[write_pointer] */
	ppn_t write_pointer = curr->pb_meta_array[pb_to_store_new_pp].ptr_to_free_page;
	assert(curr->pp_meta_array[write_pointer].state == PP_FREE);
	assert(curr->pp_meta_array[write_pointer].ptr_to_logical_page == NULL_LPN);

	if (recorded_lpn_in_new_pp != NULL_LPN) {

		/* normal write */
		curr->pp_meta_array[write_pointer].state = PP_LIVE;
		curr->pp_meta_array[write_pointer].ptr_to_logical_page = recorded_lpn_in_new_pp;

		curr->pb_meta_array[pb_to_store_new_pp].num_of_free_pages--;
		curr->pb_meta_array[pb_to_store_new_pp].num_of_live_pages++;
	} else {

		/* dummy write (used in SLC mode) */
		curr->pp_meta_array[write_pointer].state = PP_DEAD;
		curr->pp_meta_array[write_pointer].ptr_to_logical_page = recorded_lpn_in_new_pp;

		curr->pb_meta_array[pb_to_store_new_pp].num_of_free_pages--;
		curr->pb_meta_array[pb_to_store_new_pp].num_of_dead_pages++;
	}

	if (curr->pb_meta_array[pb_to_store_new_pp].num_of_free_pages == 0) {

		/* after allocating one pp, if this pb has no more free pp, mark this pp as body block */
		curr->pb_meta_array[pb_to_store_new_pp].ptr_to_free_page = NULL_PPN;
		mark_as_body_pb(curr, pb_to_store_new_pp);
	} else {

		/* otherwise, move write pointer by one and stay in the head block state */
		curr->pb_meta_array[pb_to_store_new_pp].ptr_to_free_page++;
		ASSERT_IS_VALID_HEAD_PB(curr, pb_to_store_new_pp);
	}

	return write_pointer;
}

void delete_pp_in_pb(p_meta_table_t *p_meta_table, pbn_t pbn, ppn_t ppn) {

	ASSERT_IS_VALID_PBN(p_meta_table, pbn);
	ASSERT_IS_VALID_PPN(p_meta_table, ppn);
	ASSERT_IS_PP_IN_PB(p_meta_table, pbn, ppn);

	assert(p_meta_table->pp_meta_array[ppn].state == PP_LIVE);
	p_meta_table->pp_meta_array[ppn].state = PP_DEAD;

	assert(p_meta_table->pp_meta_array[ppn].ptr_to_logical_page != NULL_LPN);
	p_meta_table->pp_meta_array[ppn].ptr_to_logical_page = NULL_LPN;

	assert(p_meta_table->pb_meta_array[pbn].num_of_dead_pages >= 0);
	assert(p_meta_table->pb_meta_array[pbn].num_of_dead_pages < p_meta_table->num_of_pps_per_pb);
	p_meta_table->pb_meta_array[pbn].num_of_dead_pages++;

	assert(p_meta_table->pb_meta_array[pbn].num_of_live_pages > 0);
	assert(p_meta_table->pb_meta_array[pbn].num_of_live_pages <= p_meta_table->num_of_pps_per_pb);
	p_meta_table->pb_meta_array[pbn].num_of_live_pages--;
}

void erase_pb(p_meta_table_t *p_meta_table, pbn_t pbn) {

	ASSERT_IS_VALID_PBN(p_meta_table, pbn);
	ASSERT_IS_VALID_TAIL_PB(p_meta_table, pbn);

	FOR_EACH_PP_IN_PB(p_meta_table, pbn, ppn, pp_meta) {
		assert(pp_meta->state == PP_DEAD);
		assert(pp_meta->ptr_to_logical_page == NULL_LPN);
		pp_meta->state = PP_FREE;
	} END_FOR_EACH_PP_IN_PB;

	p_meta_table->pb_meta_array[pbn].num_of_free_pages = p_meta_table->num_of_pps_per_pb;
	p_meta_table->pb_meta_array[pbn].num_of_live_pages = 0;
	p_meta_table->pb_meta_array[pbn].num_of_dead_pages = 0;
	p_meta_table->pb_meta_array[pbn].ptr_to_free_page = p_meta_table->pb_meta_array[pbn].ptr_to_first_page;
	p_meta_table->pb_meta_array[pbn].tot_num_of_er_ops++;
	p_meta_table->pb_meta_array[pbn].mode = PB_MLC;

	mark_as_free_pb(p_meta_table, pbn);
}

void delete_pp_recorded_given_lpn(p_meta_table_t *p_meta_table, lpn_t lpn, ppn_t pp_recorded_given_lpn) {

	/* assert the correct inverse mapping */
	assert(find_lpn_by_ppn(p_meta_table, pp_recorded_given_lpn) == lpn);

	/* find target physical block */
	pbn_t target_pb = find_pbn_by_ppn(p_meta_table, pp_recorded_given_lpn);

	/* forward deletion operation to the target physical block */
	delete_pp_in_pb(p_meta_table, target_pb, pp_recorded_given_lpn);
}

void mark_as_head_pb(p_meta_table_t *p_meta_table, pbn_t pbn) {

	assert(p_meta_table->pb_meta_array[pbn].state == PB_FREE);
	p_meta_table->pb_meta_array[pbn].state = PB_HEAD;

	ASSERT_IS_VALID_HEAD_PB(p_meta_table, pbn);

	p_meta_table->num_of_free_pbs--;
	p_meta_table->num_of_head_pbs++;

	ASSERT_IS_VALID_P_META_TABLE(p_meta_table);
}

void mark_as_body_pb(p_meta_table_t *p_meta_table, pbn_t pbn) {

	assert(p_meta_table->pb_meta_array[pbn].state == PB_HEAD);
	p_meta_table->pb_meta_array[pbn].state = PB_BODY;

	ASSERT_IS_VALID_BODY_PB(p_meta_table, pbn);

	p_meta_table->num_of_head_pbs--;
	p_meta_table->num_of_body_pbs++;

	ASSERT_IS_VALID_P_META_TABLE(p_meta_table);
}

void mark_as_tail_pb(p_meta_table_t *p_meta_table, pbn_t pbn) {

	assert(p_meta_table->pb_meta_array[pbn].state == PB_BODY);
	p_meta_table->pb_meta_array[pbn].state = PB_TAIL;

	ASSERT_IS_VALID_TAIL_PB(p_meta_table, pbn);

	p_meta_table->num_of_body_pbs--;
	p_meta_table->num_of_tail_pbs++;

	ASSERT_IS_VALID_P_META_TABLE(p_meta_table);
}

void mark_as_free_pb(p_meta_table_t *p_meta_table, pbn_t pbn) {

	assert(p_meta_table->pb_meta_array[pbn].state == PB_TAIL);
	p_meta_table->pb_meta_array[pbn].state = PB_FREE;

	ASSERT_IS_VALID_FREE_PB(p_meta_table, pbn);

	p_meta_table->num_of_tail_pbs--;
	p_meta_table->num_of_free_pbs++;

	ASSERT_IS_VALID_P_META_TABLE(p_meta_table);
}

lpn_t find_lpn_by_ppn(const p_meta_table_t *p_meta_table, ppn_t ppn) {

	ASSERT_IS_VALID_PPN(p_meta_table, ppn);
	return p_meta_table->pp_meta_array[ppn].ptr_to_logical_page;
}

pbn_t find_pbn_by_ppn(const p_meta_table_t *p_meta_table, ppn_t ppn) {

	ASSERT_IS_VALID_PPN(p_meta_table, ppn);

	pbn_t pbn = ppn / p_meta_table->num_of_pps_per_pb;

	ASSERT_IS_VALID_PBN(p_meta_table, pbn);
	ASSERT_IS_PP_IN_PB(p_meta_table, pbn, ppn);

	return pbn;
}

