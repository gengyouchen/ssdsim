#ifndef __SIMPLE_DIE_SCHEDULER_H__
#define __SIMPLE_DIE_SCHEDULER_H__

#include <cstdlib>
#include <iostream>
#include <queue>

#include "systemc.h"

#include "ssd_typedef.h"
#include "gyc_bus.h"

#include "ssd_die_ftl_interface.h"
#include "ssd_die_scheduler_interface.h"
#include "ssd_die_scheduler_callback.h"

class simple_die_scheduler :
	public sc_core::sc_module,
	public ssd_die_scheduler_interface,
	public gyc_bus_dev_if
{

	private:
		gyc_bus_pkt_mm* m_mm;
		std::deque<gyc_bus_pkt *> queue_of_pending_io_page_read;
		std::deque<gyc_bus_pkt *> queue_of_pending_io_page_write;
		std::deque<gyc_bus_pkt *> queue_of_pending_gc_page_read;
		std::deque<gyc_bus_pkt *> queue_of_pending_gc_page_write;
		std::deque<gyc_bus_pkt *> queue_of_pending_gc_block_erase;

		ssd_die_ftl_interface* _ftl;
		ssd_die_scheduler_callback *_callback;
		gc_priority_t _gc_priority;

		gyc_bus_pkt* peek_next_io_rd_op() const;
		gyc_bus_pkt* peek_next_io_wr_op() const;
		gyc_bus_pkt* peek_next_gc_rd_op() const;
		gyc_bus_pkt* peek_next_gc_wr_op() const;
		gyc_bus_pkt* peek_next_gc_er_op() const;
		gyc_bus_pkt* peek_next_op() const;

		gyc_bus_pkt* apply_next_io_rd_op();
		gyc_bus_pkt* apply_next_io_wr_op();
		gyc_bus_pkt* apply_next_gc_rd_op();
		gyc_bus_pkt* apply_next_gc_wr_op();
		gyc_bus_pkt* apply_next_gc_er_op();
		gyc_bus_pkt* apply_next_op();

#ifdef DEADLINE_AWARE_SLC_POLICY
		msec_t _deadline;
#endif

	public:
		SC_HAS_PROCESS(simple_die_scheduler);
		simple_die_scheduler(sc_core::sc_module_name module_name, gyc_bus_pkt_mm* global_mm);
		sc_port<gyc_bus_dev_cb> ch_port;

		ppn_t number_of_io_page_read;
		ppn_t number_of_io_page_write;
		ppn_t number_of_io_slc_page_write;
		ppn_t number_of_benefited_page_write;
		ppn_t number_of_truely_benefited_page_write;
		ppn_t number_of_io_mlc_page_write;
		ppn_t number_of_gc_page_read;
		ppn_t number_of_gc_page_write;
		pbn_t number_of_gc_block_erase;
		pbn_t number_of_gc_slc_block_erase;
		pbn_t number_of_gc_mlc_block_erase;

#ifdef GYC_PAPER_SLC_THROTTLING
		ppn_t last_epoch_number_of_io_page_read;
		ppn_t last_epoch_number_of_io_page_write;
		ppn_t last_epoch_number_of_io_slc_page_write;
		ppn_t last_epoch_number_of_io_mlc_page_write;
		ppn_t last_epoch_number_of_gc_page_read;
		ppn_t last_epoch_number_of_gc_page_write;
		pbn_t last_epoch_number_of_gc_block_erase;
#endif

#ifdef DEADLINE_AWARE_SLC_POLICY
		ppn_t count_miss_deadline_pending_io_page_write() const;
		bool can_all_pending_io_page_write_meet_deadline() const;
#endif

		/* ============================== */
		/* implement ssd_die_ftl_callback */
		/* ============================== */

		gc_priority_t get_gc_priority() const;
		void set_gc_priority(gc_priority_t priority);

		void schedule_gc_page_copy(ppn_t physical_page_to_be_copid, lpn_t logical_page);
		void schedule_gc_block_erase(pbn_t physical_block_to_be_erased, bool use_slc_wearout);

		size_t get_number_of_pending_io_page_read() const;
		size_t get_number_of_pending_io_page_write() const;
		size_t get_number_of_pending_gc_page_read() const;
		size_t get_number_of_pending_gc_page_write() const;
		size_t get_number_of_pending_gc_block_erase() const;

#ifdef GYC_PAPER_SLC_THROTTLING
		long double get_slc_throttle_rate() const { return _callback->get_slc_throttle_rate(); }
#endif

#ifdef GYC_PAPER_D_MONITOR
		void record_big_d(long double big_d) { _callback->record_big_d(big_d); }
		void record_small_d(long double small_d) { _callback->record_small_d(small_d); }
#endif
		/* ===================================== */
		/* implement ssd_die_scheduler_interface */
		/* ===================================== */

		ssd_die_ftl_interface* get_ftl() const;
		void set_ftl(ssd_die_ftl_interface* ftl);
		ssd_die_scheduler_callback* get_callback() const;
		void set_callback(ssd_die_scheduler_callback* callback);
		void schedule_io_page_read(gyc_bus_pkt* parent_req, lpn_t lpn);
		void schedule_io_page_write(gyc_bus_pkt* parent_req, lpn_t lpn);

#ifdef DEADLINE_AWARE_SLC_POLICY
		msec_t get_deadline() const;
		void set_deadline(msec_t deadline);
#endif

private:
		int m_ch_id;
		int m_ch_dev_id;
public:
		void set_ch_id(int ch_id) { m_ch_id = ch_id; }
		int get_ch_id() const { return m_ch_id; }
		void set_ch_dev_id(int ch_dev_id) { m_ch_dev_id = ch_dev_id; }
		int get_ch_dev_id() const { return m_ch_dev_id; }

		/* ======================== */
		/* implement gyc_bus_dev_if */
		/* ======================== */
private:
		bool m_bus_busy;
		gyc_bus_pkt* m_recv_buf;

public:
		gyc_bus_pkt* peek_next_pkt_to_send(int bus_id) const;
		void on_send_started(int bus_id);
		void on_send_completed(int bus_id);
		bool can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt_to_recv) const;
		void recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt_to_recv);
		void on_recv_completed(int bus_id);

};

#endif

