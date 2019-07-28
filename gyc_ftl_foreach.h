#ifndef __GYC_FTL_FOREACH_H__
#define __GYC_FTL_FOREACH_H__

#include "gyc_ftl_typedef.h"

#define FOR_EACH_PB(P_META_TABLE, PBN, PB_META) { \
	pbn_t PBN; \
	for (PBN = 0; PBN < P_META_TABLE->num_of_all_pbs; PBN++) { \
		pb_meta_t *PB_META = &P_META_TABLE->pb_meta_array[PBN];

#define END_FOR_EACH_PB \
	} \
}

#define FOR_EACH_PP(P_META_TABLE, PPN, PP_META) { \
	ppn_t PPN; \
	for (PPN = 0; PPN < P_META_TABLE->num_of_all_pps; PPN++) { \
		pp_meta_t *PP_META = &P_META_TABLE->pp_meta_array[PPN];

#define END_FOR_EACH_PP \
	} \
}

#define FOR_EACH_PP_IN_PB(P_META_TABLE, PBN, PPN, PP_META) { \
	ppn_t PPN; \
	for (PPN = P_META_TABLE->pb_meta_array[PBN].ptr_to_first_page; PPN <= P_META_TABLE->pb_meta_array[PBN].ptr_to_last_page; PPN++) { \
		pp_meta_t *PP_META = &P_META_TABLE->pp_meta_array[PPN];

#define END_FOR_EACH_PP_IN_PB \
	} \
}

#define FOR_EACH_FREE_PB(P_META_TABLE, PBN, PB_META) { \
	FOR_EACH_PB(P_META_TABLE, PBN, PB_META) { \
		if (PB_META->state == PB_FREE) {

#define END_FOR_EACH_FREE_PB \
		} \
	} END_FOR_EACH_PB; \
}

#define FOR_EACH_HEAD_PB(P_META_TABLE, PBN, PB_META) { \
	FOR_EACH_PB(P_META_TABLE, PBN, PB_META) { \
		if (PB_META->state == PB_HEAD) {

#define END_FOR_EACH_HEAD_PB \
		} \
	} END_FOR_EACH_PB; \
}

#define FOR_EACH_BODY_PB(P_META_TABLE, PBN, PB_META) { \
	FOR_EACH_PB(P_META_TABLE, PBN, PB_META) { \
		if (PB_META->state == PB_BODY) {

#define END_FOR_EACH_BODY_PB \
		} \
	} END_FOR_EACH_PB; \
}

#define FOR_EACH_TAIL_PB(P_META_TABLE, PBN, PB_META) { \
	FOR_EACH_PB(P_META_TABLE, PBN, PB_META) { \
		if (PB_META->state == PB_TAIL) {

#define END_FOR_EACH_TAIL_PB \
		} \
	} END_FOR_EACH_PB; \
}

#endif

