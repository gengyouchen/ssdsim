#ifndef __SSD_DIE_FTL_CALLBACK_H__ 
#define __SSD_DIE_FTL_CALLBACK_H__ 

#include "ssd_typedef.h"

struct ssd_die_ftl_callback {

	virtual gc_priority_t get_gc_priority() const = 0;
	virtual void set_gc_priority(gc_priority_t priority) = 0;

	virtual void schedule_gc_page_copy(ppn_t physical_page_to_be_copid, lpn_t logical_page) = 0;
	virtual void schedule_gc_block_erase(pbn_t physical_block_to_be_erased, bool use_slc_wearout) = 0;

	virtual size_t get_number_of_pending_io_page_read() const = 0;
	virtual size_t get_number_of_pending_io_page_write() const = 0;
	virtual size_t get_number_of_pending_gc_page_read() const = 0;
	virtual size_t get_number_of_pending_gc_page_write() const = 0;
	virtual size_t get_number_of_pending_gc_block_erase() const = 0;

#ifdef DEADLINE_AWARE_SLC_POLICY
	virtual bool can_all_pending_io_page_write_meet_deadline() const = 0;
#endif

#ifdef GYC_PAPER_SLC_THROTTLING
	virtual long double get_slc_throttle_rate() const = 0;
#endif

#ifdef GYC_PAPER_D_MONITOR
	virtual void record_big_d(long double big_d) = 0;
	virtual void record_small_d(long double small_d) = 0;
#endif

};

#endif

