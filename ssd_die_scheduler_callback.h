#ifndef __SSD_DIE_SCHEDULER_CALLBACK_H__
#define __SSD_DIE_SCHEDULER_CALLBACK_H__

#include "ssd_typedef.h"
#include "gyc_bus.h"

struct ssd_die_scheduler_callback {

	virtual void complete_parent(gyc_bus_pkt* parent_req) = 0;

#ifdef GYC_PAPER_SLC_THROTTLING
	virtual long double get_slc_throttle_rate() const = 0;
#endif

#ifdef GYC_PAPER_D_MONITOR
	virtual void record_big_d(long double big_d) = 0;
	virtual void record_small_d(long double small_d) = 0;
#endif

};

#endif

