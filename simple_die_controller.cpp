#include <cassert>

#include "systemc.h"
#include "gyc_bus.h"
#include "simple_die_controller.h"

bool simple_die_controller::can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt) const {
	assert(bus_id == m_ch_id);

	assert(next_pkt);
	assert(next_pkt->has_ssd_op_ext());
	if (m_bus_busy) {
		return false;
	}

	switch (m_op_mode) {
		case DIE_FREE_MODE:
			return true;

		case DIE_READ_MODE:
			if (next_pkt->get_op_type() != SSD_OP_TYP_RD) {
				return false;
			}
			return !m_data_reg;

		case DIE_PROGRAM_MODE:
			if (next_pkt->get_op_type() != SSD_OP_TYP_WR) {
				return false;
			}
			return !m_cache_reg;

		case DIE_ERASE_MODE:
			if (next_pkt->get_op_type() != SSD_OP_TYP_ER) {
				return false;
			}
			/* erase operation doesn't support pipeline */
			return !m_cache_reg && !m_data_reg;

		default:
			/* unknown die operation mode */
			assert(0);
			break;
	}
	return false;
}

void simple_die_controller::recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt) {
	assert(bus_id == m_ch_id);

	assert(can_recv_next_pkt(m_ch_id, next_pkt));

	assert(!m_bus_busy);
	m_bus_busy = true;

	switch (next_pkt->get_op_type()) {
		case SSD_OP_TYP_RD:
			assert(m_op_mode == DIE_FREE_MODE || m_op_mode == DIE_READ_MODE);
			assert(!m_data_reg);
			m_data_reg = next_pkt;
			if (m_op_mode == DIE_FREE_MODE) {
				m_op_mode = DIE_READ_MODE;
			}

			/* prepare reversed bus path for the future time when this operation is finished processing */
			assert(next_pkt->get_bus_pkt_dest() == m_ch_dev_id);
			next_pkt->set_bus_pkt_prio(1);
			next_pkt->set_bus_pkt_dest(m_ch_dev_id - 1);
			next_pkt->set_bus_pkt_delay(sc_time(CHANNEL_READ_OUTPUT_LATENCY, SC_MS));
			break;

		case SSD_OP_TYP_WR:
			assert(m_op_mode == DIE_FREE_MODE || m_op_mode == DIE_PROGRAM_MODE);
			assert(!m_cache_reg);
			m_cache_reg = next_pkt;
			if (m_op_mode == DIE_FREE_MODE) {
				m_op_mode = DIE_PROGRAM_MODE;
			}

			/* prepare reversed bus path for the future time when this operation is finished processing */
			assert(next_pkt->get_bus_pkt_dest() == m_ch_dev_id);
			next_pkt->set_bus_pkt_prio(0);
			next_pkt->set_bus_pkt_dest(m_ch_dev_id - 1);
			next_pkt->set_bus_pkt_delay(sc_time(CHANNEL_PROGRAM_OUTPUT_LATENCY, SC_MS));
			break;

		case SSD_OP_TYP_ER:
			assert(m_op_mode == DIE_FREE_MODE || m_op_mode == DIE_ERASE_MODE);
			assert(!m_cache_reg); /* erase operation doesn't support pipeline */
			assert(!m_data_reg);
			m_data_reg = next_pkt;
			if (m_op_mode == DIE_FREE_MODE) {
				m_op_mode = DIE_ERASE_MODE;
			}

			/* prepare reversed bus path for the future time when this operation is finished processing */
			assert(next_pkt->get_bus_pkt_dest() == m_ch_dev_id);
			next_pkt->set_bus_pkt_prio(0);
			next_pkt->set_bus_pkt_dest(m_ch_dev_id - 1);
			next_pkt->set_bus_pkt_delay(sc_time(CHANNEL_ERASE_OUTPUT_LATENCY, SC_MS));
			break;

		default:
			/* unknown operation type */
			assert(0);
			break;
	}

}

void simple_die_controller::on_recv_completed(int bus_id) {
	assert(bus_id == m_ch_id);

	assert(m_bus_busy);
	m_bus_busy = false;

	switch (m_op_mode) {
		case DIE_FREE_MODE:
			/* this should never happens */
			/* since the op mode of die will be changed at the beginning of receiving pkt */
			assert(0);
			break;

		case DIE_READ_MODE:
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_RD);

			assert(!m_die_busy);
			m_die_busy = true;
			m_exec_completed_event.notify(sc_time(MLC_READ_LATENCY, SC_MS));

			break;

		case DIE_PROGRAM_MODE:
			assert(m_cache_reg);
			assert(m_cache_reg->get_op_type() == SSD_OP_TYP_WR);

			/* perform pipeline write */
			if (!m_data_reg) {

				/* copy write operation from cache to data register and start executing */
				assert(!m_die_busy);
				m_data_reg = m_cache_reg;
				m_die_busy = true;
				if (m_data_reg->get_op_in_slc()) {
					m_exec_completed_event.notify(sc_time(SLC_PROGRAM_LATENCY, SC_MS));
				} else {
					m_exec_completed_event.notify(sc_time(MLC_PROGRAM_LATENCY, SC_MS));
				}

				/* clear cache register and be ready for receiving next write operation */
				m_cache_reg = NULL;
				belonged_channel->require_recv_next_pkt();
			}

			break;

		case DIE_ERASE_MODE:
			assert(!m_cache_reg);
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_ER);

			assert(!m_die_busy);
			m_die_busy = true;
			m_exec_completed_event.notify(sc_time(MLC_ERASE_LATENCY, SC_MS));
			break;

		default:
			/* unknown die operation mode */
			assert(0);
	}
}

void simple_die_controller::on_exec_completed() {
	//cout << "At " << sc_time_stamp().to_seconds() << ", " << name() << " on_exec_completed" << endl;
	assert(m_die_busy);
	m_die_busy = false;

	switch (m_op_mode) {
		case DIE_FREE_MODE:
			assert(0);
			break;

		case DIE_READ_MODE:
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_RD);
			if (!m_cache_reg) {
				m_cache_reg = m_data_reg;
				belonged_channel->require_send_next_pkt();
				m_data_reg = NULL;
				belonged_channel->require_recv_next_pkt();
			}
			break;

		case DIE_PROGRAM_MODE:
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_WR);
			belonged_channel->require_send_next_pkt();
			break;

		case DIE_ERASE_MODE:
			assert(!m_cache_reg);
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_ER);
			belonged_channel->require_send_next_pkt();
			break;

		default:
			assert(0);
			break;
	}
}

gyc_bus_pkt* simple_die_controller::peek_next_pkt_to_send(int bus_id) const {
	assert(bus_id == m_ch_id);

	if (m_bus_busy) {
		return NULL;
	}

	switch (m_op_mode) {
		case DIE_FREE_MODE:
			return NULL;

		case DIE_READ_MODE:
			if (m_cache_reg) {
				assert(m_cache_reg->get_op_type() == SSD_OP_TYP_RD);
				return m_cache_reg;
			}
			return NULL;

		case DIE_PROGRAM_MODE:
			if (m_data_reg) {
				assert(m_data_reg->get_op_type() == SSD_OP_TYP_WR);
				if (!m_die_busy) {
					return m_data_reg;
				}
			}
			return NULL;

		case DIE_ERASE_MODE:
			assert(!m_cache_reg);
			if (m_data_reg) {
				assert(m_data_reg->get_op_type() == SSD_OP_TYP_ER);
				if (!m_die_busy) {
					return m_data_reg;
				}
			}
			return NULL;

		default:
			assert(0);
			break;
	}
}

void simple_die_controller::on_send_started(int bus_id) {
	assert(bus_id == m_ch_id);

	assert(peek_next_pkt_to_send(m_ch_id));

	assert(!m_bus_busy);
	m_bus_busy = true;

	switch (m_op_mode) {
		case DIE_FREE_MODE:
			assert(0);
			break;

		case DIE_READ_MODE:
			assert(m_cache_reg);
			assert(m_cache_reg->get_op_type() == SSD_OP_TYP_RD);
			break;

		case DIE_PROGRAM_MODE:
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_WR);
			assert(!m_die_busy);
			break;

		case DIE_ERASE_MODE:
			assert(!m_cache_reg);
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_ER);
			assert(!m_die_busy);
			break;

		default:
			assert(0);
			break;
	}
}

void simple_die_controller::on_send_completed(int bus_id) {
	assert(bus_id == m_ch_id);

	assert(m_bus_busy);
	m_bus_busy = false;

	switch (m_op_mode) {
		case DIE_FREE_MODE:
			assert(0);
			break;

		case DIE_READ_MODE:

			/* clear cache register since the previous read result has been sent */
			assert(m_cache_reg);
			assert(m_cache_reg->get_op_type() == SSD_OP_TYP_RD);
			m_cache_reg = NULL;

			/* perform pipeline read */
			if (m_data_reg) {

				/* if current read operation is done */
				if (!m_die_busy) {

					/* copy read result from data to cache register and be ready for sending result */
					assert(m_data_reg->get_op_type() == SSD_OP_TYP_RD);
					m_cache_reg = m_data_reg;
					belonged_channel->require_send_next_pkt();

					/* clear data register and be ready for receiving next read operation */
					m_data_reg = NULL;
					belonged_channel->require_recv_next_pkt();
				}

			} else {

				/* end of pipeline read since both cache and data register are empty */
				//cout << "At " << sc_time_stamp().to_seconds() << ", " << name() << " reset from read to free mode" << endl;
				m_op_mode = DIE_FREE_MODE;
			}

			break;

		case DIE_PROGRAM_MODE:

			/* clear data register since the result of its write operation has been sent */
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_WR);
			assert(!m_die_busy);
			m_data_reg = NULL;

			/* perform pipeline write */
			if (m_cache_reg) {

				/* copy write operation from cache to data register and start executing */
				assert(m_cache_reg->get_op_type() == SSD_OP_TYP_WR);
				m_data_reg = m_cache_reg;
				m_die_busy = true;
				if (m_data_reg->get_op_in_slc()) {
					m_exec_completed_event.notify(sc_time(SLC_PROGRAM_LATENCY, SC_MS));
				} else {
					m_exec_completed_event.notify(sc_time(MLC_PROGRAM_LATENCY, SC_MS));
				}

				/* clear cache register and be ready for receiving next write operation */
				//cout << "At " << sc_time_stamp().to_seconds() << ", " << name() << " clear cache reg for next pkt" << endl;
				m_cache_reg = NULL;
				belonged_channel->require_recv_next_pkt();

			} else {

				/* end of pipeline write since both cache and data register are empty */
				//cout << "At " << sc_time_stamp().to_seconds() << ", " << name() << " reset from write to free mode" << endl;
				m_op_mode = DIE_FREE_MODE;
			}

			break;

		case DIE_ERASE_MODE:

			/* clear data register and be ready for receiving next erase operation */
			assert(!m_cache_reg);
			assert(m_data_reg);
			assert(m_data_reg->get_op_type() == SSD_OP_TYP_ER);
			assert(!m_die_busy);
			m_data_reg = NULL;
			belonged_channel->require_recv_next_pkt();

			/* erase operation doesn't support pipeline */
			//cout << "At " << sc_time_stamp().to_seconds() << ", " << name() << " reset from erase to free mode" << endl;
			m_op_mode = DIE_FREE_MODE;

			break;

		default:
			assert(0);
	}
}

simple_die_controller::simple_die_controller(sc_module_name module_name) : sc_module(module_name) {
	m_cache_reg = NULL;
	m_data_reg = NULL;
	m_die_busy = false;
	m_bus_busy = false;
	m_op_mode = DIE_FREE_MODE;

	SC_METHOD(on_exec_completed);
	sensitive << m_exec_completed_event;
	dont_initialize();
}

