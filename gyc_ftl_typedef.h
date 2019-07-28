#ifndef __GYC_FTL_TYPEDEF_H__
#define __GYC_FTL_TYPEDEF_H__

typedef long long int lpn_t;
typedef long long int pbn_t;
typedef long long int ppn_t;

#define NULL_LPN ((lpn_t) -1)
#define NULL_PPN ((ppn_t) -1)
#define NULL_PBN ((pbn_t) -1)

typedef struct lp_meta {
	ppn_t ptr_to_physical_page;
} lp_meta_t;

typedef enum pp_state {
	PP_FREE,
	PP_LIVE,
	PP_DEAD
} pp_state_t;

typedef struct pp_meta {
	pp_state state;
	lpn_t ptr_to_logical_page;
} pp_meta_t;

typedef enum pb_state {
	PB_FREE,
	PB_HEAD,
	PB_BODY,
	PB_TAIL
} pb_state_t;

typedef enum pb_mode {
	PB_SLC,
	PB_MLC
} pb_mode_t;

typedef struct pb_meta {
	pb_state_t state;
	pb_mode_t mode;
	ppn_t ptr_to_free_page;
	ppn_t ptr_to_first_page;
	ppn_t ptr_to_last_page;
	ppn_t num_of_free_pages;
	ppn_t num_of_live_pages;
	ppn_t num_of_dead_pages;
	ppn_t tot_num_of_rd_ops;
	ppn_t tot_num_of_wr_ops;
	pbn_t tot_num_of_er_ops;
} pb_meta_t;

#endif

