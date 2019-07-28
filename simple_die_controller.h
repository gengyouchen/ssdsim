#ifndef __SIMPLE_DIE_CONTROLLER_H__
#define __SIMPLE_DIE_CONTROLLER_H__

#include "systemc.h"
#include "ssd_typedef.h"
#include "gyc_bus.h"

typedef enum die_op_mode {
	DIE_FREE_MODE,
	DIE_READ_MODE,
	DIE_PROGRAM_MODE,
	DIE_ERASE_MODE
} die_op_mode_t;

class simple_die_controller : public sc_module, public gyc_bus_dev_if {
	private:
		gyc_bus_pkt* m_cache_reg;
		gyc_bus_pkt* m_data_reg;

		bool m_die_busy;
		bool m_bus_busy;

		sc_event m_exec_completed_event;
		void on_exec_completed();

		die_op_mode_t m_op_mode;

	public:
		SC_HAS_PROCESS(simple_die_controller);
		simple_die_controller(sc_module_name module_name);

		sc_port<gyc_bus_dev_cb> belonged_channel;

	private:
		int m_ch_id;
		int m_ch_dev_id;

	public:
		void set_ch_id(int ch_id) { m_ch_id = ch_id; }
		int get_ch_id() const { return m_ch_id; }
		void set_ch_dev_id(int ch_dev_id) { m_ch_dev_id = ch_dev_id; }
		int get_ch_dev_id() const { return m_ch_dev_id; }

		/* ======================= */
		/* implement gyc_bus_in_if */
		/* ======================= */
	public:
		bool can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt) const;
		void recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt);
		void on_recv_completed(int bus_id);

		/* ======================== */
		/* implement gyc_bus_out_if */
		/* ======================== */
	public:
		gyc_bus_pkt* peek_next_pkt_to_send(int bus_id) const;
		void on_send_started(int bus_id);
		void on_send_completed(int bus_id);

		/* ======================== */
		/* disable copy constructor */
		/* ======================== */
	private:
		simple_die_controller(const simple_die_controller& ref_obj) { }
};

#endif

