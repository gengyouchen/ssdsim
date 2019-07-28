#ifndef __SSD_DIE_FTL_INTERFACE_H__
#define __SSD_DIE_FTL_INTERFACE_H__

#include "ssd_typedef.h"
#include "ssd_die_ftl_callback.h"

struct ssd_die_ftl_interface {

	virtual ssd_die_ftl_callback* get_callback() const = 0;
	virtual void set_callback(ssd_die_ftl_callback* callback) = 0;
	virtual void reset_data_layout() = 0;

	virtual bool check_mapping_validity(lpn_t logical_page, ppn_t physical_page) const = 0;

	virtual msec_t issue_page_read(lpn_t logical_page, bool generated_by_gc) = 0;
	virtual msec_t issue_page_write(lpn_t logical_page, bool generated_by_gc) = 0;
	virtual msec_t issue_block_erase(pbn_t physical_block) = 0;

};

#endif

