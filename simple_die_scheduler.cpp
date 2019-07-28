#include "simple_die_scheduler.h"

#include <iostream>
#include <cassert>

#include "systemc.h"

using namespace std;
using namespace sc_core;

simple_die_scheduler::simple_die_scheduler(sc_module_name module_name, gyc_bus_pkt_mm* global_mm) :
	sc_module(module_name), ch_port("ch_port")
{

	assert(global_mm);
	m_mm = global_mm;

	_ftl = NULL;
	_callback = NULL;
	_gc_priority = BACKGROUND_GC;

	number_of_io_page_read = 0;
	number_of_io_page_write = 0;
	number_of_io_slc_page_write = 0;
	number_of_benefited_page_write = 0;
	number_of_truely_benefited_page_write = 0;
	number_of_io_mlc_page_write = 0;
	number_of_gc_page_read = 0;
	number_of_gc_page_write = 0;
	number_of_gc_block_erase = 0;
	number_of_gc_slc_block_erase = 0;
	number_of_gc_mlc_block_erase = 0;

#ifdef GYC_PAPER_SLC_THROTTLING
	last_epoch_number_of_io_page_read = 0;
	last_epoch_number_of_io_page_write = 0;
	last_epoch_number_of_io_slc_page_write = 0;
	last_epoch_number_of_io_mlc_page_write = 0;
	last_epoch_number_of_gc_page_read = 0;
	last_epoch_number_of_gc_page_write = 0;
	last_epoch_number_of_gc_block_erase = 0;
#endif

#ifdef DEADLINE_AWARE_SLC_POLICY
	_deadline = (msec_t) 0.0;
#endif

	m_bus_busy = false;
	m_recv_buf = NULL;
}

gyc_bus_pkt* simple_die_scheduler::peek_next_io_rd_op() const {
	if (!queue_of_pending_io_page_read.empty()) {
		gyc_bus_pkt* op = queue_of_pending_io_page_read.front();

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_RD);
		assert(op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() == NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		return op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::apply_next_io_rd_op() {
	if (!queue_of_pending_io_page_read.empty()) {
		gyc_bus_pkt* op = queue_of_pending_io_page_read.front();
		queue_of_pending_io_page_read.pop_front();

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_RD);
		assert(op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() == NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		msec_t op_latency = _ftl->issue_page_read(op->get_op_lpn(), false);
		assert(op_latency == MLC_READ_LATENCY);

		number_of_io_page_read++;
#ifdef GYC_PAPER_SLC_THROTTLING
		last_epoch_number_of_io_page_read++;
#endif
		return op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::peek_next_io_wr_op() const {
	if (!queue_of_pending_io_page_write.empty()) {
		gyc_bus_pkt* op = queue_of_pending_io_page_write.front();

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_WR);
		assert(op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() == NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		return op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::apply_next_io_wr_op() {
	if (!queue_of_pending_io_page_write.empty()) {
		gyc_bus_pkt* op = queue_of_pending_io_page_write.front();
		queue_of_pending_io_page_write.pop_front();

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_WR);
		assert(op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() == NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		msec_t op_latency = _ftl->issue_page_write(op->get_op_lpn(), false);
		if (op_latency == SLC_PROGRAM_LATENCY) {
			op->set_op_in_slc(true);
			number_of_io_slc_page_write++;
			number_of_benefited_page_write += ((ppn_t) 1 + queue_of_pending_io_page_write.size());
			number_of_truely_benefited_page_write += ((ppn_t) 0 + count_miss_deadline_pending_io_page_write());
#ifdef GYC_PAPER_SLC_THROTTLING
			last_epoch_number_of_io_slc_page_write++;
#endif
		} else if (op_latency == MLC_PROGRAM_LATENCY) {
			op->set_op_in_slc(false);
			number_of_io_mlc_page_write++;
#ifdef GYC_PAPER_SLC_THROTTLING
			last_epoch_number_of_io_mlc_page_write++;
#endif
		} else {
			assert(0);
		}

		number_of_io_page_write++;
#ifdef GYC_PAPER_SLC_THROTTLING
		last_epoch_number_of_io_page_write++;
#endif
		return op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::peek_next_gc_rd_op() const {
	gyc_bus_pkt* return_op = NULL;
	deque<gyc_bus_pkt *>::const_iterator iter = queue_of_pending_gc_page_read.begin();
	while (iter != queue_of_pending_gc_page_read.end()) {
		gyc_bus_pkt* op = *iter++;

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_RD);
		assert(!op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() != NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		if (_ftl->check_mapping_validity(op->get_op_lpn(), op->get_op_ppn())) {
			return_op = op;
			break;
		}
	}

	if (return_op) {
		return return_op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::apply_next_gc_rd_op() {
	gyc_bus_pkt* return_op = NULL;
	deque<gyc_bus_pkt *>::iterator iter = queue_of_pending_gc_page_read.begin();
	while (iter != queue_of_pending_gc_page_read.end()) {
		gyc_bus_pkt* op = *iter++;

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_RD);
		assert(!op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() != NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		if (_ftl->check_mapping_validity(op->get_op_lpn(), op->get_op_ppn())) {
			msec_t op_latency = _ftl->issue_page_read(op->get_op_lpn(), true);
			assert(op_latency == MLC_READ_LATENCY);

			return_op = op;
			break;
		} else {
			op->free();
		}
	}

	queue_of_pending_gc_page_read.erase(queue_of_pending_gc_page_read.begin(), iter);

	if (return_op) {
		number_of_gc_page_read++;
#ifdef GYC_PAPER_SLC_THROTTLING
		last_epoch_number_of_gc_page_read++;
#endif
		return return_op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::peek_next_gc_wr_op() const {
	gyc_bus_pkt* return_op = NULL;
	deque<gyc_bus_pkt *>::const_iterator iter = queue_of_pending_gc_page_write.begin();
	while (iter != queue_of_pending_gc_page_write.end()) {
		gyc_bus_pkt* op = *iter++;

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_WR);
		assert(!op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() != NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		if (_ftl->check_mapping_validity(op->get_op_lpn(), op->get_op_ppn())) {
			return_op = op;
			break;
		}
	}

	if (return_op) {
		return return_op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::apply_next_gc_wr_op() {
	gyc_bus_pkt* return_op = NULL;
	deque<gyc_bus_pkt *>::iterator iter = queue_of_pending_gc_page_write.begin();
	while (iter != queue_of_pending_gc_page_write.end()) {
		gyc_bus_pkt* op = *iter++;

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_WR);
		assert(!op->get_op_parent_req());
		assert(op->get_op_lpn() != NULL_LPN);
		assert(op->get_op_ppn() != NULL_PPN);
		assert(op->get_op_pbn() == NULL_PBN);
		assert(!op->get_op_in_slc());

		if (_ftl->check_mapping_validity(op->get_op_lpn(), op->get_op_ppn())) {
			msec_t op_latency = _ftl->issue_page_write(op->get_op_lpn(), true);
			assert(op_latency == MLC_PROGRAM_LATENCY);

			return_op = op;
			break;
		} else {
			op->free();
		}
	}

	queue_of_pending_gc_page_write.erase(queue_of_pending_gc_page_write.begin(), iter);

	if (return_op) {
		number_of_gc_page_write++;
#ifdef GYC_PAPER_SLC_THROTTLING
		last_epoch_number_of_gc_page_write++;
#endif
		return return_op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::peek_next_gc_er_op() const {
	if (!queue_of_pending_gc_block_erase.empty()) {
		gyc_bus_pkt* op = queue_of_pending_gc_block_erase.front();

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_ER);
		assert(!op->get_op_parent_req());
		assert(op->get_op_lpn() == NULL_LPN);
		assert(op->get_op_ppn() == NULL_PPN);
		assert(op->get_op_pbn() != NULL_PBN);

		return op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::apply_next_gc_er_op() {
	if (!queue_of_pending_gc_block_erase.empty()) {
		gyc_bus_pkt* op = queue_of_pending_gc_block_erase.front();
		queue_of_pending_gc_block_erase.pop_front();

		assert(op->has_ssd_op_ext());
		assert(op->get_op_type() == SSD_OP_TYP_ER);
		assert(!op->get_op_parent_req());
		assert(op->get_op_lpn() == NULL_LPN);
		assert(op->get_op_ppn() == NULL_PPN);
		assert(op->get_op_pbn() != NULL_PBN);

		msec_t op_latency = _ftl->issue_block_erase(op->get_op_pbn());
		assert(op_latency == MLC_ERASE_LATENCY);

		if (op->get_op_in_slc()) {
			number_of_gc_slc_block_erase++;
		} else {
			number_of_gc_mlc_block_erase++;
		}
		number_of_gc_block_erase++;
#ifdef GYC_PAPER_SLC_THROTTLING
		last_epoch_number_of_gc_block_erase++;
#endif
		return op;
	}
	return NULL;
}

gyc_bus_pkt* simple_die_scheduler::peek_next_op() const {
	gyc_bus_pkt* next_op = NULL;
	if (_gc_priority == FOREGROUND_GC) {
		if ((next_op = peek_next_gc_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_gc_wr_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_gc_er_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_io_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_io_wr_op()) != NULL) {
			return next_op;
		} else {
			return NULL;
		}
	} else if (_gc_priority == BACKGROUND_GC) {
		if ((next_op = peek_next_io_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_io_wr_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_gc_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_gc_wr_op()) != NULL) {
			return next_op;
		} else if ((next_op = peek_next_gc_er_op()) != NULL) {
			return next_op;
		} else {
			return NULL;
		}
	} else {
		assert(0);
		return NULL;
	}
}
gyc_bus_pkt* simple_die_scheduler::apply_next_op() {
	gyc_bus_pkt* next_op = NULL;
	if (_gc_priority == FOREGROUND_GC) {
		if ((next_op = apply_next_gc_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_gc_wr_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_gc_er_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_io_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_io_wr_op()) != NULL) {
			return next_op;
		} else {
			return NULL;
		}
	} else if (_gc_priority == BACKGROUND_GC) {
		if ((next_op = apply_next_io_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_io_wr_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_gc_rd_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_gc_wr_op()) != NULL) {
			return next_op;
		} else if ((next_op = apply_next_gc_er_op()) != NULL) {
			return next_op;
		} else {
			return NULL;
		}
	} else {
		assert(0);
		return NULL;
	}
}


/* ============================== */
/* implement ssd_die_ftl_callback */
/* ============================== */

gc_priority_t simple_die_scheduler::get_gc_priority() const {
	return _gc_priority;
}

void simple_die_scheduler::set_gc_priority(gc_priority_t priority) {
	_gc_priority = priority;
}

void simple_die_scheduler::schedule_gc_page_copy(ppn_t physical_page_to_be_copid, lpn_t logical_page) {

	gyc_bus_pkt* gc_rd_op = m_mm->alloc();
	gc_rd_op->set_bus_pkt_prio(1);
	gc_rd_op->set_bus_pkt_dest(m_ch_dev_id + 1);
	gc_rd_op->set_bus_pkt_delay(sc_time(CHANNEL_READ_INPUT_LATENCY, SC_MS));
	gc_rd_op->enable_ssd_op_ext();
	gc_rd_op->set_op_parent_req(NULL);
	gc_rd_op->set_op_type(SSD_OP_TYP_RD);
	gc_rd_op->set_op_lpn(logical_page);
	gc_rd_op->set_op_ppn(physical_page_to_be_copid);
	gc_rd_op->set_op_pbn(NULL_PBN);
	gc_rd_op->set_op_in_slc(false);
#ifdef DEADLINE_AWARE_SLC_POLICY
	gc_rd_op->set_op_deadline(DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE);
#endif
	queue_of_pending_gc_page_read.push_back(gc_rd_op);

	gyc_bus_pkt* gc_wr_op = m_mm->alloc();
	gc_wr_op->set_bus_pkt_prio(0);
	gc_wr_op->set_bus_pkt_dest(m_ch_dev_id + 1);
	gc_wr_op->set_bus_pkt_delay(sc_time(CHANNEL_PROGRAM_INPUT_LATENCY, SC_MS));
	gc_wr_op->enable_ssd_op_ext();
	gc_wr_op->set_op_parent_req(NULL);
	gc_wr_op->set_op_type(SSD_OP_TYP_WR);
	gc_wr_op->set_op_lpn(logical_page);
	gc_wr_op->set_op_ppn(physical_page_to_be_copid);
	gc_wr_op->set_op_pbn(NULL_PBN);
	gc_wr_op->set_op_in_slc(false);
#ifdef DEADLINE_AWARE_SLC_POLICY
	gc_wr_op->set_op_deadline(DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE);
#endif
	queue_of_pending_gc_page_write.push_back(gc_wr_op);

	if (peek_next_pkt_to_send(m_ch_id)) {
		ch_port->require_send_next_pkt();
	}
}

void simple_die_scheduler::schedule_gc_block_erase(pbn_t physical_block_to_be_erased, bool use_slc_wearout) {

	gyc_bus_pkt* gc_er_op = m_mm->alloc();
	gc_er_op->set_bus_pkt_prio(0);
	gc_er_op->set_bus_pkt_dest(m_ch_dev_id + 1);
	gc_er_op->set_bus_pkt_delay(sc_time(CHANNEL_ERASE_INPUT_LATENCY, SC_MS));
	gc_er_op->enable_ssd_op_ext();
	gc_er_op->set_op_parent_req(NULL);
	gc_er_op->set_op_type(SSD_OP_TYP_ER);
	gc_er_op->set_op_lpn(NULL_LPN);
	gc_er_op->set_op_ppn(NULL_PPN);
	gc_er_op->set_op_pbn(physical_block_to_be_erased);
	gc_er_op->set_op_in_slc(use_slc_wearout);
#ifdef DEADLINE_AWARE_SLC_POLICY
	gc_er_op->set_op_deadline(DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE);
#endif
	queue_of_pending_gc_block_erase.push_back(gc_er_op);

	if (peek_next_pkt_to_send(m_ch_id)) {
		ch_port->require_send_next_pkt();
	}
}

size_t simple_die_scheduler::get_number_of_pending_io_page_read() const {
	return queue_of_pending_io_page_read.size();
}

size_t simple_die_scheduler::get_number_of_pending_io_page_write() const {
	return queue_of_pending_io_page_write.size();
}

size_t simple_die_scheduler::get_number_of_pending_gc_page_read() const {
	return queue_of_pending_gc_page_read.size();
}

size_t simple_die_scheduler::get_number_of_pending_gc_page_write() const {
	return queue_of_pending_gc_page_write.size();
}

size_t simple_die_scheduler::get_number_of_pending_gc_block_erase() const {
	return queue_of_pending_gc_block_erase.size();
}

/* ===================================== */
/* implement ssd_die_scheduler_interface */
/* ===================================== */

ssd_die_ftl_interface* simple_die_scheduler::get_ftl() const {
	return _ftl;
}

void simple_die_scheduler::set_ftl(ssd_die_ftl_interface* ftl) {
	_ftl = ftl;
}

ssd_die_scheduler_callback* simple_die_scheduler::get_callback() const {
	return _callback;
}

void simple_die_scheduler::set_callback(ssd_die_scheduler_callback* callback) {
	_callback = callback;
}

void simple_die_scheduler::schedule_io_page_read(gyc_bus_pkt* parent_req, lpn_t lpn) {
	gyc_bus_pkt* io_rd_op = m_mm->alloc();
	io_rd_op->set_bus_pkt_prio(1);
	io_rd_op->set_bus_pkt_dest(m_ch_dev_id + 1);
	io_rd_op->set_bus_pkt_delay(sc_time(CHANNEL_READ_INPUT_LATENCY, SC_MS));
	io_rd_op->enable_ssd_op_ext();
	io_rd_op->set_op_parent_req(parent_req);
	io_rd_op->set_op_type(SSD_OP_TYP_RD);
	io_rd_op->set_op_lpn(lpn);
	io_rd_op->set_op_ppn(NULL_PPN);
	io_rd_op->set_op_pbn(NULL_PBN);
	io_rd_op->set_op_in_slc(false);
#ifdef DEADLINE_AWARE_SLC_POLICY
	io_rd_op->set_op_deadline(DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE);
#endif
	queue_of_pending_io_page_read.push_back(io_rd_op);

	if (peek_next_pkt_to_send(m_ch_id)) {
		ch_port->require_send_next_pkt();
	}
}

void simple_die_scheduler::schedule_io_page_write(gyc_bus_pkt* parent_req, lpn_t lpn) {

	gyc_bus_pkt* io_wr_op = m_mm->alloc();
	io_wr_op->set_bus_pkt_prio(0);
	io_wr_op->set_bus_pkt_dest(m_ch_dev_id + 1);
	io_wr_op->set_bus_pkt_delay(sc_time(CHANNEL_PROGRAM_INPUT_LATENCY, SC_MS));
	io_wr_op->enable_ssd_op_ext();
	io_wr_op->set_op_parent_req(parent_req);
	io_wr_op->set_op_type(SSD_OP_TYP_WR);
	io_wr_op->set_op_lpn(lpn);
	io_wr_op->set_op_ppn(NULL_PPN);
	io_wr_op->set_op_pbn(NULL_PBN);
	io_wr_op->set_op_in_slc(false);
#ifdef DEADLINE_AWARE_SLC_POLICY
	io_wr_op->set_op_deadline(sc_time_stamp().to_seconds() * 1000 + _deadline);
#endif
	queue_of_pending_io_page_write.push_back(io_wr_op);

	if (peek_next_pkt_to_send(m_ch_id)) {
		ch_port->require_send_next_pkt();
	}
}

#ifdef DEADLINE_AWARE_SLC_POLICY
ppn_t simple_die_scheduler::count_miss_deadline_pending_io_page_write() const {
	ppn_t miss_count = 0;
	for (int i = 0; i < queue_of_pending_io_page_write.size(); i++) {
		msec_t expected_latency = sc_time_stamp().to_seconds() * 1000
			+ MLC_READ_LATENCY * queue_of_pending_io_page_read.size()
			+ MLC_PROGRAM_LATENCY /* worst case of pipeline */
			+ MLC_PROGRAM_LATENCY * (i + 1)
			- 0.001; /* prevent floating point precision lost */
		if (expected_latency > queue_of_pending_io_page_write[i]->get_op_deadline()) {
			miss_count++;
		}
	}
	return miss_count;
}
bool simple_die_scheduler::can_all_pending_io_page_write_meet_deadline() const {
	return (count_miss_deadline_pending_io_page_write() == 0);
}

msec_t simple_die_scheduler::get_deadline() const {
	return _deadline;
}

void simple_die_scheduler::set_deadline(msec_t deadline) {
	_deadline = deadline;
}


#endif

/* ======================== */
/* implement gyc_bus_dev_if */
/* ======================== */

gyc_bus_pkt* simple_die_scheduler::peek_next_pkt_to_send(int bus_id) const {
	assert(bus_id == m_ch_id);

	if (m_bus_busy) {
		return NULL;
	}
	return peek_next_op();
}

void simple_die_scheduler::on_send_started(int bus_id) {
	assert(bus_id == m_ch_id);

	assert(!m_bus_busy);
	m_bus_busy = true;
	gyc_bus_pkt* peek_of_next_op = peek_next_op();
	gyc_bus_pkt* next_op = apply_next_op();
	assert(peek_of_next_op);
	assert(next_op == peek_of_next_op);
}

void simple_die_scheduler::on_send_completed(int bus_id) {
	assert(bus_id == m_ch_id);

	assert(m_bus_busy);
	m_bus_busy = false;
}

bool simple_die_scheduler::can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt_to_recv) const {
	assert(bus_id == m_ch_id);

	assert(next_pkt_to_recv);
	assert(next_pkt_to_recv->has_ssd_op_ext());

	if (m_bus_busy) {
		return false;
	}
	if (m_recv_buf) {
		return false;
	}
	return true;
}

void simple_die_scheduler::recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt_to_recv) {
	assert(bus_id == m_ch_id);

	assert(!m_bus_busy);
	m_bus_busy = true;

	assert(!m_recv_buf);
	assert(next_pkt_to_recv);
	assert(next_pkt_to_recv->has_ssd_op_ext());
	assert(next_pkt_to_recv->get_bus_pkt_dest() == m_ch_dev_id);
	m_recv_buf = next_pkt_to_recv;
}

void simple_die_scheduler::on_recv_completed(int bus_id) {
	assert(bus_id == m_ch_id);

	assert(m_bus_busy);
	m_bus_busy = false;

	assert(m_recv_buf);
	assert(m_recv_buf->has_ssd_op_ext());
	gyc_bus_pkt* parent_req = m_recv_buf->get_op_parent_req();
	m_recv_buf->free();
	m_recv_buf = NULL;

	if (parent_req) {
		_callback->complete_parent(parent_req);
	}

	ch_port->require_recv_next_pkt();
}

