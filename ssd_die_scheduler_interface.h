#ifndef __SSD_DIE_SCHEDULER_INTERFACE_H__
#define __SSD_DIE_SCHEDULER_INTERFACE_H__

#include "ssd_typedef.h"
#include "gyc_bus.h"
#include "ssd_die_ftl_callback.h"
#include "ssd_die_ftl_interface.h"
#include "ssd_die_scheduler_callback.h"

struct ssd_die_scheduler_interface : public ssd_die_ftl_callback {

	virtual ssd_die_ftl_interface* get_ftl() const = 0;
	virtual void set_ftl(ssd_die_ftl_interface* ftl) = 0;
	virtual ssd_die_scheduler_callback* get_callback() const = 0;
	virtual void set_callback(ssd_die_scheduler_callback* callback) = 0;
	virtual void schedule_io_page_read(gyc_bus_pkt* parent_req, lpn_t lpn) = 0;
	virtual void schedule_io_page_write(gyc_bus_pkt* parent_req, lpn_t lpn) = 0;

#ifdef DEADLINE_AWARE_SLC_POLICY
	virtual void set_deadline(msec_t deadline) = 0;
#endif

};

#endif

