#ifndef __SIMPLE_DIE_FTL_H__
#define __SIMPLE_DIE_FTL_H__

#include <cassert>
#include <cstdlib>
#include <cmath>
#include <new>
#include <iostream>

#include "systemc.h"

#include "ssd_typedef.h"
#include "ssd_die_ftl_callback.h"
#include "ssd_die_ftl_interface.h"

#include "gyc_ftl_typedef.h"
#include "gyc_ftl_assert.h"
#include "gyc_ftl_foreach.h"
#include "gyc_ftl_p_meta_table.h"
#include "gyc_ftl_wr_ptr.h"

class simple_die_ftl : public sc_module, public ssd_die_ftl_interface {

	private:
		ssd_die_ftl_callback* _callback;

		p_meta_table_t* p_meta_table;
		lp_meta_t* lp_meta_table;

		wr_ptr_t *wr_ptr_for_short_io_queue;
		wr_ptr_t *wr_ptr_for_long_io_queue;
		wr_ptr_t *wr_ptr_for_gc;

		void schedule_gc_if_needed();
		msec_t _determine_write_latency(const wr_ptr_t *wr_ptr) const;

	public:

		SC_HAS_PROCESS(simple_die_ftl);
		simple_die_ftl(sc_core::sc_module_name module_name);

		/* setting */
		int min_free_blocks_percentage;
		slc_policy_t slc_policy;

		ssd_die_ftl_callback* get_callback() const;
		void set_callback(ssd_die_ftl_callback* callback);
		void reset_data_layout();

		bool check_mapping_validity(lpn_t logical_page, ppn_t physical_page) const;

		msec_t issue_page_read(lpn_t logical_page, bool generated_by_gc);
		msec_t issue_page_write(lpn_t logical_page, bool generated_by_gc);
		msec_t issue_block_erase(pbn_t physical_block);

		void print_status();

#ifdef GYC_PAPER_OVERLOAD_PROTECTION
		ppn_t m_current_busy_period_slc_write_count;
		ppn_t m_current_busy_period_mlc_write_count;
		bool m_current_busy_period_has_record_slc_usage;
		bool m_after_foreground_gc;
		long double m_max_slc_usage;
		long double m_ready_slc_usage;
		long double get_current_busy_period_slc_write_utilization() const;
		bool should_enable_slc() const;
#endif

};

#endif

