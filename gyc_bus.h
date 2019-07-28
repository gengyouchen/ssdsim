#ifndef __GYC_BUS_H__
#define __GYC_BUS_H__

#define GYC_BUS_PKT_SSD_REQ_EXT
#define GYC_BUS_PKT_SSD_OP_EXT

#include <cassert>

#include "systemc.h"

#ifdef GYC_BUS_PKT_SSD_REQ_EXT

#include "ssd_typedef.h"

typedef enum ssd_req_type {
	SSD_REQ_TYP_RD,
	SSD_REQ_TYP_WR
} ssd_req_type_t;

#endif

#ifdef GYC_BUS_PKT_SSD_OP_EXT

#include "ssd_typedef.h"

typedef enum ssd_op_type {
	SSD_OP_TYP_RD,
	SSD_OP_TYP_WR,
	SSD_OP_TYP_ER
} ssd_op_type_t;

#endif

class gyc_bus_pkt;
struct gyc_bus_pkt_mm_if {
	virtual void free(gyc_bus_pkt* pkt) = 0;
};

class gyc_bus_pkt {
	private:
		gyc_bus_pkt_mm_if* m_bus_pkt_mm;
		int m_bus_pkt_prio;
		int m_bus_pkt_dest;
		sc_time m_bus_pkt_delay;
	public:
		gyc_bus_pkt(gyc_bus_pkt_mm_if* bus_pkt_mm) : m_bus_pkt_mm(bus_pkt_mm) { assert(m_bus_pkt_mm); }
		void free() { assert(m_bus_pkt_mm); m_bus_pkt_mm->free(this); }

		void set_bus_pkt_prio(int bus_pkt_prio) { m_bus_pkt_prio = bus_pkt_prio; }
		int get_bus_pkt_prio() const { return m_bus_pkt_prio; }

		void set_bus_pkt_dest(int bus_pkt_dest) { m_bus_pkt_dest = bus_pkt_dest; }
		int get_bus_pkt_dest() const { return m_bus_pkt_dest; }

		void set_bus_pkt_delay(const sc_time& bus_pkt_delay) { m_bus_pkt_delay = bus_pkt_delay; }
		sc_time get_bus_pkt_delay() const { return m_bus_pkt_delay; }

#ifdef GYC_BUS_PKT_SSD_REQ_EXT
	private:
		bool m_def_req_ext;
		ssd_req_type m_req_type;
		lpn_t m_req_lpn;
		lpn_t m_req_size;
		lpn_t m_req_remain_size;
		sc_time m_req_arrival_time;
	public:
		bool has_ssd_req_ext() const { return m_def_req_ext; }
		void enable_ssd_req_ext() { assert(!m_def_req_ext); m_def_req_ext = true; }
		void disable_ssd_req_ext() { assert(m_def_req_ext); m_def_req_ext = false; }
		ssd_req_type_t get_req_type() const { assert(m_def_req_ext); return m_req_type; }
		void set_req_type(ssd_req_type_t req_type) { assert(m_def_req_ext); m_req_type = req_type; }
		lpn_t get_req_lpn() const { assert(m_def_req_ext); return m_req_lpn; }
		void set_req_lpn(lpn_t req_lpn) { assert(m_def_req_ext); m_req_lpn = req_lpn; }
		lpn_t get_req_size() const { assert(m_def_req_ext); return m_req_size; }
		void set_req_size(lpn_t req_size) { assert(m_def_req_ext); m_req_size = req_size; }
		lpn_t get_req_remain_size() const { assert(m_def_req_ext); return m_req_remain_size; }
		void set_req_remain_size(lpn_t req_remain_size) { assert(m_def_req_ext); m_req_remain_size = req_remain_size; }
		sc_time get_req_arrival_time() const { assert(m_def_req_ext); return m_req_arrival_time; }
		void set_req_arrival_time(const sc_time& req_arrival_time) { assert(m_def_req_ext); m_req_arrival_time = req_arrival_time; }
#endif

#ifdef GYC_BUS_PKT_SSD_OP_EXT
	private:
		bool m_def_op_ext;
		gyc_bus_pkt* m_op_parent_req;
		ssd_op_type_t m_op_type;
		lpn_t m_op_lpn;
		ppn_t m_op_ppn;
		pbn_t m_op_pbn;
		bool m_op_in_slc;
#ifdef DEADLINE_AWARE_SLC_POLICY
		msec_t m_op_deadline;
#endif
	public:
		bool has_ssd_op_ext() const { return m_def_op_ext; }
		void enable_ssd_op_ext() { assert(!m_def_op_ext); m_def_op_ext = true; }
		void disable_ssd_op_ext() { assert(m_def_op_ext); m_def_op_ext = false; }
		gyc_bus_pkt* get_op_parent_req() const { assert(m_def_op_ext); if (m_op_parent_req) assert(m_op_parent_req->has_ssd_req_ext()); return m_op_parent_req; }
		void set_op_parent_req(gyc_bus_pkt* op_parent_req) { assert(m_def_op_ext); if (op_parent_req) assert(op_parent_req->has_ssd_req_ext()); m_op_parent_req = op_parent_req; }
		ssd_op_type_t get_op_type() const { assert(m_def_op_ext); return m_op_type; }
		void set_op_type(ssd_op_type_t op_type) { assert(m_def_op_ext); m_op_type = op_type; }
		lpn_t get_op_lpn() const { assert(m_def_op_ext); return m_op_lpn; }
		void set_op_lpn(lpn_t op_lpn) { assert(m_def_op_ext); m_op_lpn = op_lpn; }
		ppn_t get_op_ppn() const { assert(m_def_op_ext); return m_op_ppn; }
		void set_op_ppn(ppn_t op_ppn) { assert(m_def_op_ext); m_op_ppn = op_ppn; }
		pbn_t get_op_pbn() const { assert(m_def_op_ext); return m_op_pbn; }
		void set_op_pbn(pbn_t op_pbn) { assert(m_def_op_ext); m_op_pbn = op_pbn; }
		bool get_op_in_slc() const { assert(m_def_op_ext); return m_op_in_slc; }
		void set_op_in_slc(bool op_in_slc) { assert(m_def_op_ext); m_op_in_slc = op_in_slc; }
#ifdef DEADLINE_AWARE_SLC_POLICY
		msec_t get_op_deadline() const { assert(m_def_op_ext); return m_op_deadline; }
		void set_op_deadline(msec_t op_deadline) { assert(m_def_op_ext); m_op_deadline = op_deadline; }
#endif
#endif
};

class gyc_bus_pkt_mm : public gyc_bus_pkt_mm_if {
	private:
		struct gyc_bus_pkt_owner {
			gyc_bus_pkt* pkt;
			gyc_bus_pkt_owner* next;
			gyc_bus_pkt_owner* prev;
		};
		gyc_bus_pkt_owner* m_first_valid_owner;
		gyc_bus_pkt_owner* m_last_invalid_owner;
		int m_count;
	public:
		gyc_bus_pkt_mm() : m_first_valid_owner(NULL), m_last_invalid_owner(NULL), m_count(0) { }
		gyc_bus_pkt* alloc() {
			gyc_bus_pkt* pkt;
			if (m_first_valid_owner) {
				pkt = m_first_valid_owner->pkt;
				m_last_invalid_owner = m_first_valid_owner;
				m_first_valid_owner = m_first_valid_owner->next;
			} else {
				pkt = new gyc_bus_pkt(this);
			}
			m_count++;

#ifdef GYC_BUS_PKT_SSD_REQ_EXT
			if (pkt->has_ssd_req_ext()) {
				pkt->disable_ssd_req_ext();
			}
#endif

#ifdef GYC_BUS_PKT_SSD_OP_EXT
			if (pkt->has_ssd_op_ext()) {
				pkt->disable_ssd_op_ext();
			}
#endif
			return pkt;
		}
		void free(gyc_bus_pkt* pkt) {
			if (!m_last_invalid_owner) {
				m_last_invalid_owner = new gyc_bus_pkt_owner();
				m_last_invalid_owner->next = m_first_valid_owner;
				m_last_invalid_owner->prev = NULL;
				if (m_first_valid_owner) {
					m_first_valid_owner->prev = m_last_invalid_owner;
				}
			}
			m_first_valid_owner = m_last_invalid_owner;
			m_last_invalid_owner = m_last_invalid_owner->prev;
			m_first_valid_owner->pkt = pkt;
			m_count--;

#ifdef GYC_BUS_PKT_SSD_REQ_EXT
			if (pkt->has_ssd_req_ext()) {
				pkt->disable_ssd_req_ext();
			}
#endif

#ifdef GYC_BUS_PKT_SSD_OP_EXT
			if (pkt->has_ssd_op_ext()) {
				pkt->disable_ssd_op_ext();
			}
#endif
		}
		int count() const {
			return m_count;
		}
};

struct gyc_bus_dev_if : virtual public sc_interface {
	virtual gyc_bus_pkt* peek_next_pkt_to_send(int bus_id) const = 0;
	virtual void on_send_started(int bus_id) = 0;
	virtual void on_send_completed(int bus_id) = 0;
	virtual bool can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt_to_recv) const = 0;
	virtual void recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt_to_recv) = 0;
	virtual void on_recv_completed(int bus_id) = 0;
};

struct gyc_bus_dev_cb : virtual public sc_interface {
	virtual void require_send_next_pkt() = 0;
	virtual void require_recv_next_pkt() = 0;
};

class gyc_bus : public sc_module, public gyc_bus_dev_cb {
	private:
		int m_bus_id;
		bool m_busy;
		gyc_bus_pkt* m_pkt;
		int m_src_id;
		int m_dest_id;
		sc_event m_require_sched;
		sc_event m_transfer_done_event;
		int m_rr_src_id;

		void sched() {
			assert(!m_busy && !m_pkt && m_src_id == -1 && m_dest_id == -1);
			for (int i = 0; i < dev.size(); i++) {
				int src_id = (m_rr_src_id + i) % dev.size();
				gyc_bus_pkt* pkt = dev[src_id]->peek_next_pkt_to_send(m_bus_id);
				if (pkt) {
					int dest_id = pkt->get_bus_pkt_dest();
					assert(dest_id >= 0 && dest_id < dev.size() && dest_id != src_id);
					if (dev[dest_id]->can_recv_next_pkt(m_bus_id, pkt)) {
						if (!m_pkt || pkt->get_bus_pkt_prio() > m_pkt->get_bus_pkt_prio()) {
							m_pkt = pkt;
							m_src_id = src_id;
							m_dest_id = dest_id;
						}
					}
				}
			}
			if (m_pkt) {
				m_rr_src_id = (m_src_id + 1) % dev.size();
				m_busy = true;
				m_transfer_done_event.notify(m_pkt->get_bus_pkt_delay());
				dev[m_src_id]->on_send_started(m_bus_id);
				dev[m_dest_id]->recv_next_pkt(m_bus_id, m_pkt);
			} else {
				//cout << "At " << sc_time_stamp().to_seconds() << ", " << name() << " has nothing to transfer" << endl;
			}
		}
		void on_transfer_done() {
			assert(m_busy && m_pkt && m_src_id != -1 && m_dest_id != -1);
			dev[m_src_id]->on_send_completed(m_bus_id);
			dev[m_dest_id]->on_recv_completed(m_bus_id);

			m_busy = false;
			m_pkt = NULL;
			m_src_id= -1;
			m_dest_id= -1;
			m_require_sched.notify(SC_ZERO_TIME);
		}
	public:
		sc_port<gyc_bus_dev_if, 0> dev;
		SC_CTOR(gyc_bus) {
			m_bus_id = -1;
			m_busy = false;
			m_pkt = NULL;
			m_src_id = -1;
			m_dest_id = -1;
			m_rr_src_id = 0;

			SC_METHOD(sched);
			sensitive << m_require_sched;
			dont_initialize();

			SC_METHOD(on_transfer_done);
			sensitive << m_transfer_done_event;
			dont_initialize();
		}
		void set_bus_id(int bus_id) { m_bus_id = bus_id; }
		int get_bus_id() const { return m_bus_id; }
		void require_send_next_pkt() { if (!m_busy) m_require_sched.notify(SC_ZERO_TIME); }
		void require_recv_next_pkt() { if (!m_busy) m_require_sched.notify(SC_ZERO_TIME); }
};

#endif

