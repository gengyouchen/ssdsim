#ifndef __GYC_FTL_ASSERT_H__
#define __GYC_FTL_ASSERT_H__

#include <assert.h>
#include <stdio.h>

#include "gyc_ftl_config.h"
#include "gyc_ftl_typedef.h"
#include "gyc_ftl_p_meta_table.h"

#define ASSERT_PASS { \
	return 1; \
}

#define ASSERT_CHECK(TRUE_EXPRESSION) { \
	if (!(TRUE_EXPRESSION)) { \
		fputs("ASSERT_CHECK(" #TRUE_EXPRESSION ")\n", stderr); \
		return 0; \
	} \
}

#define ASSERT_IS_VALID_PBN(P_META_TABLE, PBN) { \
	assert(_assert_is_valid_pbn(P_META_TABLE, PBN)); \
}

static inline int _assert_is_valid_pbn(const p_meta_table_t *p_meta_table, pbn_t pbn) {

	ASSERT_CHECK(pbn >= 0);
	ASSERT_CHECK(pbn < p_meta_table->num_of_all_pbs);

	ASSERT_PASS;
}

#define ASSERT_IS_VALID_PPN(P_META_TABLE, PPN) { \
	assert(_assert_is_valid_ppn(P_META_TABLE, PPN)); \
}

static inline int _assert_is_valid_ppn(const p_meta_table_t *p_meta_table, ppn_t ppn) {

	ASSERT_CHECK(ppn >= 0);
	ASSERT_CHECK(ppn < p_meta_table->num_of_all_pps);

	ASSERT_PASS;
}

#define ASSERT_IS_PP_IN_PB(P_META_TABLE, PBN, PPN) { \
	assert(_assert_is_pp_in_pb(P_META_TABLE, PBN, PPN)); \
}

static inline int _assert_is_pp_in_pb(const p_meta_table_t *p_meta_table, pbn_t pbn, ppn_t ppn) {

	ASSERT_CHECK(ppn >= p_meta_table->pb_meta_array[pbn].ptr_to_first_page);
	ASSERT_CHECK(ppn <= p_meta_table->pb_meta_array[pbn].ptr_to_last_page);

	ASSERT_PASS;
}

#define ASSERT_IS_VALID_P_META_TABLE(P_META_TABLE) { \
	assert(_assert_is_valid_p_meta_table(P_META_TABLE)); \
}

static inline int _assert_is_valid_p_meta_table(const p_meta_table_t *p_meta_table) {

	ASSERT_CHECK(p_meta_table);
	ASSERT_CHECK(p_meta_table->pb_meta_array);
	ASSERT_CHECK(p_meta_table->pp_meta_array);

	/* check each variable's range */
	ASSERT_CHECK(p_meta_table->num_of_all_pps > 0);
	ASSERT_CHECK(p_meta_table->num_of_all_pbs > 0);
	ASSERT_CHECK(p_meta_table->num_of_pps_per_pb > 0);
	ASSERT_CHECK(p_meta_table->num_of_free_pbs >= 0);
	ASSERT_CHECK(p_meta_table->num_of_head_pbs >= 0);
	ASSERT_CHECK(p_meta_table->num_of_body_pbs >= 0);
	ASSERT_CHECK(p_meta_table->num_of_tail_pbs >= 0);

	/* check multi-variable consistency */
	ASSERT_CHECK(p_meta_table->num_of_all_pps == p_meta_table->num_of_pps_per_pb * p_meta_table->num_of_all_pbs);
	ASSERT_CHECK(p_meta_table->num_of_all_pbs == p_meta_table->num_of_free_pbs + p_meta_table->num_of_head_pbs + p_meta_table->num_of_body_pbs + p_meta_table->num_of_tail_pbs);

	ASSERT_PASS;
}

#define ASSERT_IS_VALID_FREE_PB(P_META_TABLE, PBN) { \
	assert(_assert_is_valid_free_pb(P_META_TABLE, PBN)); \
}

static inline int _assert_is_valid_free_pb(const p_meta_table_t *p_meta_table, pbn_t pbn) {

	ASSERT_CHECK(p_meta_table);
	ASSERT_CHECK(p_meta_table->pb_meta_array);
	ASSERT_CHECK(pbn < p_meta_table->num_of_all_pbs);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_first_page == p_meta_table->num_of_pps_per_pb * pbn);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_last_page == p_meta_table->num_of_pps_per_pb * (pbn + 1) - 1);

	/* check each variable's range */
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].state == PB_FREE);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_free_page == p_meta_table->pb_meta_array[pbn].ptr_to_first_page);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_free_pages == p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages == 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_dead_pages == 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].mode == PB_MLC);

	ASSERT_PASS;
}

#define ASSERT_IS_VALID_HEAD_PB(P_META_TABLE, PBN) { \
	assert(_assert_is_valid_head_pb(P_META_TABLE, PBN)); \
}

static inline int _assert_is_valid_head_pb(const p_meta_table_t *p_meta_table, pbn_t pbn) {

	ASSERT_CHECK(p_meta_table);
	ASSERT_CHECK(p_meta_table->pb_meta_array);
	ASSERT_CHECK(pbn < p_meta_table->num_of_all_pbs);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_first_page == p_meta_table->num_of_pps_per_pb * pbn);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_last_page == p_meta_table->num_of_pps_per_pb * (pbn + 1) - 1);

	/* check each variable's range */
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].state == PB_HEAD);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_free_page >= p_meta_table->pb_meta_array[pbn].ptr_to_first_page);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_free_page <= p_meta_table->pb_meta_array[pbn].ptr_to_last_page);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_free_pages <= p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_free_pages > 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages < p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages >= 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_dead_pages < p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_dead_pages >= 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].mode == PB_SLC || p_meta_table->pb_meta_array[pbn].mode == PB_MLC);

	/* check multi-variable consistency */
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_free_pages == p_meta_table->pb_meta_array[pbn].ptr_to_last_page - p_meta_table->pb_meta_array[pbn].ptr_to_free_page + 1);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_free_pages + p_meta_table->pb_meta_array[pbn].num_of_live_pages + p_meta_table->pb_meta_array[pbn].num_of_dead_pages == p_meta_table->num_of_pps_per_pb);

	ASSERT_PASS;
}

#define ASSERT_IS_VALID_BODY_PB(P_META_TABLE, PBN) { \
	assert(_assert_is_valid_body_pb(P_META_TABLE, PBN)); \
}

static inline int _assert_is_valid_body_pb(const p_meta_table_t *p_meta_table, pbn_t pbn) {

	ASSERT_CHECK(p_meta_table);
	ASSERT_CHECK(p_meta_table->pb_meta_array);
	ASSERT_CHECK(pbn < p_meta_table->num_of_all_pbs);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_first_page == p_meta_table->num_of_pps_per_pb * pbn);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_last_page == p_meta_table->num_of_pps_per_pb * (pbn + 1) - 1);

	/* check each variable's range */
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].state == PB_BODY);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_free_page == NULL_PPN);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_free_pages == 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages <= p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages >= 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_dead_pages <= p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_dead_pages >= 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].mode == PB_SLC || p_meta_table->pb_meta_array[pbn].mode == PB_MLC);

	/* check multi-variable consistency */
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages + p_meta_table->pb_meta_array[pbn].num_of_dead_pages == p_meta_table->num_of_pps_per_pb);

	ASSERT_PASS;
}

#define ASSERT_IS_VALID_TAIL_PB(P_META_TABLE, PBN) { \
	assert(_assert_is_valid_tail_pb(P_META_TABLE, PBN)); \
}

static inline int _assert_is_valid_tail_pb(const p_meta_table_t *p_meta_table, pbn_t pbn) {

	ASSERT_CHECK(p_meta_table);
	ASSERT_CHECK(p_meta_table->pb_meta_array);
	ASSERT_CHECK(pbn < p_meta_table->num_of_all_pbs);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_first_page == p_meta_table->num_of_pps_per_pb * pbn);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_last_page == p_meta_table->num_of_pps_per_pb * (pbn + 1) - 1);

	/* check each variable's range */
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].state == PB_TAIL);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].ptr_to_free_page == NULL_PPN);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_free_pages == 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages <= p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages >= 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_dead_pages <= p_meta_table->num_of_pps_per_pb);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_dead_pages >= 0);
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].mode == PB_SLC || p_meta_table->pb_meta_array[pbn].mode == PB_MLC);

	/* check multi-variable consistency */
	ASSERT_CHECK(p_meta_table->pb_meta_array[pbn].num_of_live_pages + p_meta_table->pb_meta_array[pbn].num_of_dead_pages == p_meta_table->num_of_pps_per_pb);

	ASSERT_PASS;
}

#endif

