#include "simple_ssd_controller.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <utility>
#include <vector>
#include <cmath>

#include "systemc.h"

#include "ssd_typedef.h"
#include "gyc_bus.h"

#include "simple_die_scheduler.h"
#include "simple_die_ftl.h"

using namespace std;
using namespace sc_core;

double geometric_mean(const std::vector<long double> &data) {
	long double m = 1.0;
	long long ex = 0;
	long double invN = static_cast<long double>(1.0) / data.size();
	for (int k = 0; k < data.size(); k++) {
		int i;
		double f1 = std::frexp(data[k],&i);
		m*=f1;
		ex+=i;
	}
	return std::pow(std::numeric_limits<long double>::radix,ex * invN) * std::pow(m,invN);
}

simple_ssd_controller::simple_ssd_controller (
		sc_module_name module_name,
		gyc_bus_pkt_mm* global_mm,
		int total_number_of_dice,
		int total_number_of_channels,
		int min_free_blocks_percentage,
		slc_policy_t slc_policy) :
	sc_module(module_name),
	download_ch_port("download_ch_port"),
	upload_ch_port("upload_ch_port")
{

#ifdef GYC_PAPER_D_MONITOR
	m_mlc_clean_amount = 0.0;
	m_slc_clean_amount = 0.0;
#endif
	assert(global_mm);
	m_mm = global_mm;

	m_queue_size = 0;
	m_recv_buf = NULL;
	m_download_ch_id = -1;
	m_upload_ch_id = -1;
	m_download_ch_busy = false;
	m_upload_ch_busy = false;
#ifdef GYC_PAPER_SLC_THROTTLING
	m_slc_throttle_rate = 1.0;
#endif
	m_slc_policy = slc_policy;
	die_schedulers.resize(total_number_of_dice, NULL);
	die_ftls.resize(total_number_of_dice, NULL);
	die_controllers.resize(total_number_of_dice, NULL);

	ch_array.resize(total_number_of_channels, NULL);
	for (int ch_idx = 0; ch_idx < ch_array.size(); ch_idx++) {
		ch_array[ch_idx] = new gyc_bus(sc_gen_unique_name("ch"));
		ch_array[ch_idx]->set_bus_id(ch_idx);
	}

	/* create and initialize all die scheduler & FTL */
	for (int die_idx = 0; die_idx < total_number_of_dice; die_idx++) {

		/* create each die scheduler */
		die_schedulers[die_idx] = new simple_die_scheduler(sc_gen_unique_name("die_sched"), m_mm);

		/* create each die FTL */
		die_ftls[die_idx] = new simple_die_ftl(sc_gen_unique_name("die_ftl"));

		/* create each die controller */
		die_controllers[die_idx] = new simple_die_controller(sc_gen_unique_name("die_ctrl"));

		/* bind each die FTL to each die scheduler */
		die_schedulers[die_idx]->set_ftl(die_ftls[die_idx]);
		die_ftls[die_idx]->set_callback(die_schedulers[die_idx]);

		/* bind each die scheduler to this SSD controller */
		die_schedulers[die_idx]->set_callback(this);

		int ch_idx = die_idx % ch_array.size();
		int sched_dev_idx = (die_idx / ch_array.size()) * 2 + 0;
		int ctrl_dev_idx = (die_idx / ch_array.size()) * 2 + 1;

		/* bind each die scheduler to each die channel */
		die_schedulers[die_idx]->ch_port(*ch_array[ch_idx]);
		ch_array[ch_idx]->dev(*die_schedulers[die_idx]);
		die_schedulers[die_idx]->set_ch_id(ch_idx);
		die_schedulers[die_idx]->set_ch_dev_id(sched_dev_idx);

		/* bind each die controller to each die channel */
		die_controllers[die_idx]->belonged_channel(*ch_array[ch_idx]);
		ch_array[ch_idx]->dev(*die_controllers[die_idx]);
		die_controllers[die_idx]->set_ch_id(ch_idx);
		die_controllers[die_idx]->set_ch_dev_id(ctrl_dev_idx);

		/* configure each die FTL */
		die_ftls[die_idx]->min_free_blocks_percentage = min_free_blocks_percentage;
		die_ftls[die_idx]->slc_policy = slc_policy;
		die_ftls[die_idx]->reset_data_layout();
	}


#ifdef DEADLINE_AWARE_SLC_POLICY_SELF_TUNING
	m_tla_next_targ = DEADLINE_AWARE_SLC_POLICY_DEADLINE;
	m_tla_next_pr99 = NULL_MSEC;
	m_tla_prev_1_targ = 0;
	m_tla_prev_1_pr99 = 0;
	m_tla_prev_2_targ = 999999;
	m_tla_prev_2_pr99 = 999999;
	m_tla_log.open("tla_log.txt");
#endif

	_update_deadline_for_all_die();

#if 0
	/* print debug information */
	cout << "Finish creating and initializing all die scheduler & FTL" << endl;
	cout << "Each die has been applied the same setting as follow:" << endl;
	cout << "\tmin_free_blocks_percentage=" << min_free_blocks_percentage << endl;
	cout << "\tslc_policy=";
	switch (slc_policy) {
		case NO_SLC_POLICY:
			cout << "NO_SLC_POLICY";
			break;
		case QUEUE_AWARE_SLC_POLICY:
			cout << "QUEUE_AWARE_SLC_POLICY";
			break;
		case ALWAYS_SLC_POLICY:
			cout << "ALWAYS_SLC_POLICY";
			break;
		default:
			assert(0);
			break;
	}
	cout << endl;
#endif

}

void simple_ssd_controller::debug_check_all_req_completed() const {

	ppn_t total_number_of_io_page_read = 0;
	ppn_t total_number_of_io_page_write = 0;
	ppn_t total_number_of_io_slc_page_write = 0;
	ppn_t total_number_of_benefited_page_write = 0;
	ppn_t total_number_of_truely_benefited_page_write = 0;
	ppn_t total_number_of_io_mlc_page_write = 0;
	ppn_t total_number_of_gc_page_read = 0;
	ppn_t total_number_of_gc_page_write = 0;
	pbn_t total_number_of_gc_block_erase = 0;
	pbn_t total_number_of_gc_slc_block_erase = 0;
	pbn_t total_number_of_gc_mlc_block_erase = 0;

	pbn_t total_number_of_physical_blocks = 0;

	for (int die_idx = 0; die_idx < die_schedulers.size(); die_idx++) {

#if 0
		cout << "For die " << die_idx << ":" << endl;
/*
		cout << "\tnumber_of_io_page_read: " << die_schedulers[die_idx]->number_of_io_page_read << endl;
		cout << "\tnumber_of_io_page_write: " << die_schedulers[die_idx]->number_of_io_page_write << endl;
		cout << "\tnumber_of_gc_page_read: " << die_schedulers[die_idx]->number_of_gc_page_read << endl;
		cout << "\tnumber_of_gc_page_write: " << die_schedulers[die_idx]->number_of_gc_page_write << endl;
	 	cout << "\tnumber_of_gc_page_erase: " << die_schedulers[die_idx]->number_of_gc_block_erase << endl;
*/
	 	cout << "\tnumber_of_gc_slc_block_erase: " << die_schedulers[die_idx]->number_of_gc_slc_block_erase << endl;
	 	cout << "\tnumber_of_gc_mlc_block_erase: " << die_schedulers[die_idx]->number_of_gc_mlc_block_erase << endl;
#endif

		total_number_of_io_page_read += die_schedulers[die_idx]->number_of_io_page_read;
		total_number_of_io_page_write += die_schedulers[die_idx]->number_of_io_page_write;
		total_number_of_io_slc_page_write += die_schedulers[die_idx]->number_of_io_slc_page_write;
		total_number_of_benefited_page_write += die_schedulers[die_idx]->number_of_benefited_page_write;
		total_number_of_truely_benefited_page_write += die_schedulers[die_idx]->number_of_truely_benefited_page_write;
		total_number_of_io_mlc_page_write += die_schedulers[die_idx]->number_of_io_mlc_page_write;
		total_number_of_gc_page_read += die_schedulers[die_idx]->number_of_gc_page_read;
		total_number_of_gc_page_write += die_schedulers[die_idx]->number_of_gc_page_write;
		total_number_of_gc_block_erase += die_schedulers[die_idx]->number_of_gc_block_erase;
		total_number_of_gc_slc_block_erase += die_schedulers[die_idx]->number_of_gc_slc_block_erase;
		total_number_of_gc_mlc_block_erase += die_schedulers[die_idx]->number_of_gc_mlc_block_erase;

		total_number_of_physical_blocks += BLOCK_PER_DIE;
	}

	cout << "For entire SSD:" << endl;
	cout << "\t# of User Page Read: " << total_number_of_io_page_read << endl;
	cout << "\t# of User Page Write: " << total_number_of_io_page_write << endl;
	cout << "\t\t# of User SLC Page Write: " << total_number_of_io_slc_page_write << " (" << static_cast<long double>(total_number_of_io_slc_page_write) * 100 / total_number_of_io_page_write << "%)" << endl;
	cout << "\t\t# of User MLC Page Write: " << total_number_of_io_mlc_page_write << " (" << static_cast<long double>(total_number_of_io_mlc_page_write) * 100 / total_number_of_io_page_write << "%)" << endl;
	cout << "\t# of GC Page Read: " << total_number_of_gc_page_read << endl;
	cout << "\t# of GC Page Write: " << total_number_of_gc_page_write << endl;
	cout << "\t# of GC Block Erase: " << total_number_of_gc_block_erase << endl;
	cout << "\t\t# of GC SLC Block Erase: " << total_number_of_gc_slc_block_erase << " (" << static_cast<long double>(total_number_of_gc_slc_block_erase) * 100 / total_number_of_gc_block_erase << "%)" << endl;
	cout << "\t\t# of GC MLC Block Erase: " << total_number_of_gc_mlc_block_erase << " (" << static_cast<long double>(total_number_of_gc_mlc_block_erase) * 100 / total_number_of_gc_block_erase << "%)" << endl;

	/* For my master thesis, report some easily understandable statistic */
	cout << "For my master thesis: " << endl;
	cout << dec << fixed;
	cout << "\tPolicy: ";
	switch (m_slc_policy) {
		case NO_SLC_POLICY:
			cout << "NO_SLC_POLICY";
			break;
		case QUEUE_AWARE_SLC_POLICY:
			cout << "QUEUE_AWARE_SLC_POLICY";
			break;
		case ALWAYS_SLC_POLICY:
			cout << "ALWAYS_SLC_POLICY";
			break;
		default:
			assert(0);
			break;
	}
#ifdef DEADLINE_AWARE_SLC_POLICY_SELF_TUNING
		cout << " + SELF_TUNING";
#endif
	cout << endl;
	cout << "\tDeadline: " << DEADLINE_AWARE_SLC_POLICY_DEADLINE << endl;
	cout << "\tDie Number: " << die_schedulers.size() << endl;
	cout << "\tBlock Number: " << total_number_of_physical_blocks << endl;
	cout << "\tUser Write Count: " << total_number_of_io_page_write << endl;
	cout << "\tUser SLC Write Count: " << total_number_of_io_slc_page_write << endl;
	cout << "\tUser SLC Percentage: " << static_cast<long double>(total_number_of_io_slc_page_write) * 100 / total_number_of_io_page_write << endl;
	cout << "\tUser SLC Overall Speedup Efficiency: " << ((total_number_of_io_slc_page_write > 0) ? (static_cast<long double>(total_number_of_benefited_page_write) / total_number_of_io_slc_page_write) : 0) << endl;
	cout << "\tUser SLC SLO-targeted Speedup Efficiency: " << ((total_number_of_io_slc_page_write > 0) ? (static_cast<long double>(total_number_of_truely_benefited_page_write) / total_number_of_io_slc_page_write) : 0) << endl;
	cout << "\tTotal Write Count: " << total_number_of_io_page_write + total_number_of_gc_page_write << endl;
	cout << "\tWrite Amplification: " << static_cast<long double>(total_number_of_io_page_write + total_number_of_gc_page_write) /  total_number_of_io_page_write << endl;
	cout << "\tTotal Erase Count: " << total_number_of_gc_block_erase << endl;
	cout << "\tAverage Erase Count: " << static_cast<long double>(total_number_of_gc_block_erase) / total_number_of_physical_blocks << endl;

#ifdef GYC_PAPER_D_MONITOR
	cout << "\tMLC clean amount=" << m_mlc_clean_amount << endl;
	cout << "\tSLC clean amount=" << m_slc_clean_amount << endl;
#endif

/*
	cout << "For read resposne time:" << endl;
	cout << "\tread_response_time.avg()=" << read_response_time.avg() << endl;
	cout << "\tread_response_time.percentile(0.50)=" << read_response_time.percentile(0.50) << endl;
	cout << "\tread_response_time.percentile(0.90)=" << read_response_time.percentile(0.90) << endl;
	cout << "\tread_response_time.percentile(0.95)=" << read_response_time.percentile(0.95) << endl;
	cout << "\tread_response_time.percentile(0.99)=" << read_response_time.percentile(0.99) << endl;
	cout << "\tread_response_time.percentile(0.999)=" << read_response_time.percentile(0.999) << endl;
	cout << "\tread_response_time.percentile(0.9999)=" << read_response_time.percentile(0.9999) << endl;

	cout << "For write resposne time:" << endl;
	cout << "\twrite_response_time.avg()=" << write_response_time.avg() << endl;
	cout << "\twrite_response_time.percentile(0.50)=" << write_response_time.percentile(0.50) << endl;
	cout << "\twrite_response_time.percentile(0.90)=" << write_response_time.percentile(0.90) << endl;
	cout << "\twrite_response_time.percentile(0.95)=" << write_response_time.percentile(0.95) << endl;
	cout << "\twrite_response_time.percentile(0.99)=" << write_response_time.percentile(0.99) << endl;
	cout << "\twrite_response_time.percentile(0.999)=" << write_response_time.percentile(0.999) << endl;
	cout << "\twrite_response_time.percentile(0.9999)=" << write_response_time.percentile(0.9999) << endl;
*/
}

/* ==================================== */
/* implement ssd_die_scheduler_callback */
/* ==================================== */

void simple_ssd_controller::complete_parent(gyc_bus_pkt* parent_req) {

	assert(parent_req);
	assert(parent_req->get_req_remain_size() > 0);
	parent_req->set_req_remain_size(parent_req->get_req_remain_size() - 1);

	if (parent_req->get_req_remain_size() == 0) {


		assert(sc_time_stamp() > parent_req->get_req_arrival_time());
		msec_t response_time = (sc_time_stamp() - parent_req->get_req_arrival_time()).to_seconds() * 1000;
		io_latency_monitor.record_value(response_time);

		if (io_latency_monitor.count() % 100000 == 0) {

#ifdef DEADLINE_AWARE_SLC_POLICY
#ifdef DEADLINE_AWARE_SLC_POLICY_SELF_TUNING
			assert(m_tla_next_pr99 == NULL_MSEC);
			m_tla_next_pr99 = io_latency_monitor.percentile(0.99);

			assert(m_tla_log.is_open());
			m_tla_log << dec << fixed << sc_time_stamp().to_seconds();
			m_tla_log << '\t' << m_tla_next_targ;
			m_tla_log << '\t' << m_tla_next_pr99;
			m_tla_log << endl;

			cout << "At t = " << sc_time_stamp().to_seconds() << " sec";
			cout << ", next(" << m_tla_next_targ << ", " << m_tla_next_pr99 << ")";
			tla();
			assert(m_tla_next_pr99 == NULL_MSEC);
			cout << ", next(" << m_tla_next_targ << ", ?)";
			_update_deadline_for_all_die();

#endif

#ifdef GYC_PAPER_SLC_THROTTLING
			lpn_t last_epoch_number_of_io_page_write = 0;
			lpn_t last_epoch_number_of_io_slc_page_write = 0;
			for (int die_idx = 0; die_idx < die_schedulers.size(); die_idx++) {
				last_epoch_number_of_io_page_write += die_schedulers[die_idx]->last_epoch_number_of_io_page_write;
				last_epoch_number_of_io_slc_page_write += die_schedulers[die_idx]->last_epoch_number_of_io_slc_page_write;

				die_schedulers[die_idx]->last_epoch_number_of_io_page_read = 0;
				die_schedulers[die_idx]->last_epoch_number_of_io_page_write = 0;
				die_schedulers[die_idx]->last_epoch_number_of_io_slc_page_write = 0;
				die_schedulers[die_idx]->last_epoch_number_of_io_mlc_page_write = 0;
				die_schedulers[die_idx]->last_epoch_number_of_gc_page_read = 0;
				die_schedulers[die_idx]->last_epoch_number_of_gc_page_write = 0;
				die_schedulers[die_idx]->last_epoch_number_of_gc_block_erase = 0;
			}

			long double last_epoch_measured_slc_utilization;
			if (last_epoch_number_of_io_page_write > 0) {
				last_epoch_measured_slc_utilization = static_cast<long double>(last_epoch_number_of_io_slc_page_write)
					/ last_epoch_number_of_io_page_write;
			} else if (last_epoch_number_of_io_page_write == 0) {
				last_epoch_measured_slc_utilization = 0;
			} else {
				assert(0);
			}
			cout << ", measured SLC utilization = " << last_epoch_measured_slc_utilization * 100 << "%";

			assert(GYC_PAPER_SLC_THROTTLING__TARGETED_SLC_UTILIZATION > 0.0);
			assert(GYC_PAPER_SLC_THROTTLING__TARGETED_SLC_UTILIZATION < 1.0);
			assert(GYC_PAPER_SLC_THROTTLING__THROTTLING_RATE_MULTIPLIER_FOR_ZERO_SLC_UTILIZATION > 1.0);
			assert(GYC_PAPER_SLC_THROTTLING__THROTTLING_RATE_INITIAL_VALUE_FOR_ZERO_SLC_UTILIZATION > 0.0);
			assert(GYC_PAPER_SLC_THROTTLING__THROTTLING_RATE_INITIAL_VALUE_FOR_ZERO_SLC_UTILIZATION <= 1.0);

			if (last_epoch_measured_slc_utilization > GYC_PAPER_SLC_THROTTLING__TARGETED_SLC_UTILIZATION) {
				/* need slow down */
				assert(last_epoch_measured_slc_utilization > 0);
				m_slc_throttle_rate *= GYC_PAPER_SLC_THROTTLING__TARGETED_SLC_UTILIZATION
					/ last_epoch_measured_slc_utilization;
				assert(m_slc_throttle_rate >= 0);
			} else if (last_epoch_measured_slc_utilization < GYC_PAPER_SLC_THROTTLING__TARGETED_SLC_UTILIZATION) {
				/* need speed up */
				if (last_epoch_measured_slc_utilization > 0) {
					if (m_slc_throttle_rate > 0) {
						m_slc_throttle_rate *= GYC_PAPER_SLC_THROTTLING__TARGETED_SLC_UTILIZATION
							/ last_epoch_measured_slc_utilization;
						if (m_slc_throttle_rate > 1.0) {
							m_slc_throttle_rate = 1.0;
						}
					} else if (m_slc_throttle_rate == 0) {
						/* if you completely turn off slc, last_epoch_measured_slc_utilization should be zero */
						assert(0);
					} else {
						/* m_slc_throttle_rate should not be non-negative */
						assert(0);
					}
				} else if (last_epoch_measured_slc_utilization == 0) {
					if (m_slc_throttle_rate > 0) {
						m_slc_throttle_rate *= GYC_PAPER_SLC_THROTTLING__THROTTLING_RATE_MULTIPLIER_FOR_ZERO_SLC_UTILIZATION;
						if (m_slc_throttle_rate > 1.0) {
							m_slc_throttle_rate = 1.0;
						}
					} else if (m_slc_throttle_rate == 0) {
						m_slc_throttle_rate = GYC_PAPER_SLC_THROTTLING__THROTTLING_RATE_INITIAL_VALUE_FOR_ZERO_SLC_UTILIZATION;
					} else {
						assert(0);
					}
				} else {
					/* last_epoch_measured_slc_utilization should not be non-negative */
					assert(0);
				}
			}

			cout << ", SLC throttling rate = " << m_slc_throttle_rate * 100 << "%";
#endif

#endif
			cout << endl;
			io_latency_monitor.reset();
		}

		m_complete_req_queue.push_back(parent_req);
		if (peek_next_pkt_to_send(m_upload_ch_id)) {
			upload_ch_port->require_send_next_pkt();
		}
	}
}

#ifdef DEADLINE_AWARE_SLC_POLICY
void simple_ssd_controller::_update_deadline_for_all_die() {
	for (int i = 0; i < die_schedulers.size(); ++i) {
#ifdef DEADLINE_AWARE_SLC_POLICY_SELF_TUNING
		assert(m_tla_next_targ != NULL_MSEC);
		assert(m_tla_next_pr99 == NULL_MSEC);
		die_schedulers[i]->set_deadline(m_tla_next_targ);
#else
		die_schedulers[i]->set_deadline(DEADLINE_AWARE_SLC_POLICY_DEADLINE);
#endif

	}
}
#endif

/* ======================== */
/* implement gyc_bus_dev_if */
/* ======================== */

gyc_bus_pkt* simple_ssd_controller::peek_next_pkt_to_send(int bus_id) const {
	if (bus_id == m_download_ch_id) {
		return NULL;
	} else if (bus_id == m_upload_ch_id) {
		if (!m_upload_ch_busy && !m_complete_req_queue.empty()) {
			return m_complete_req_queue.front();
		}
		return NULL;
	} else {
		assert(0);
		return NULL;
	}
}

void simple_ssd_controller::on_send_started(int bus_id) {
	if (bus_id == m_download_ch_id) {
		assert(0);
	} else if (bus_id == m_upload_ch_id) {
		assert(!m_upload_ch_busy);
		assert(!m_complete_req_queue.empty());
		m_upload_ch_busy = true;
		m_complete_req_queue.pop_front();
	} else {
		assert(0);
	}
}

void simple_ssd_controller::on_send_completed(int bus_id) {
	if (bus_id == m_download_ch_id) {
		assert(0);
	} else if (bus_id == m_upload_ch_id) {
		assert(m_upload_ch_busy);
		m_upload_ch_busy = false;
		assert(m_queue_size > 0);
		m_queue_size--;
		if (!m_complete_req_queue.empty()) {
			upload_ch_port->require_send_next_pkt();
		}
		if (m_queue_size < SSD_QUEUE_DEPTH && !m_download_ch_busy && !m_recv_buf) {
			download_ch_port->require_recv_next_pkt();
		}
	} else {
		assert(0);
	}
}

bool simple_ssd_controller::can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt_to_recv) const {
	if (bus_id == m_download_ch_id) {
		if (m_queue_size < SSD_QUEUE_DEPTH && !m_download_ch_busy && !m_recv_buf) {
			return true;
		}
		return false;
	} else if (bus_id == m_upload_ch_id) {
		return false;
	} else {
		assert(0);
		return false;
	}
}

void simple_ssd_controller::recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt_to_recv) {
	if (bus_id == m_download_ch_id) {
		assert(m_queue_size < SSD_QUEUE_DEPTH);
		assert(!m_download_ch_busy);
		assert(!m_recv_buf);
		m_download_ch_busy = true;
		m_recv_buf = next_pkt_to_recv;
	} else if (bus_id == m_upload_ch_id) {
		assert(0);
	} else {
		assert(0);
	}
}

void simple_ssd_controller::on_recv_completed(int bus_id) {
	if (bus_id == m_download_ch_id) {
		assert(m_download_ch_busy);
		m_download_ch_busy = false;

		/* prepare reversed bus path for the future time when this operation is finished processing */
		assert(m_recv_buf);
		assert(m_recv_buf->has_ssd_req_ext());
		assert(m_recv_buf->get_bus_pkt_dest() == 1);
		m_recv_buf->set_bus_pkt_dest(0);
		switch (m_recv_buf->get_req_type()) {
			case SSD_REQ_TYP_RD:
				m_recv_buf->set_bus_pkt_prio(1);
				m_recv_buf->set_bus_pkt_delay(sc_time(HOST_BUS_IO_CYCLE * (HOST_BUS_HEADER_SIZE + m_recv_buf->get_req_size() * SECTOR_PER_PAGE * BYTE_PER_SECTOR), SC_MS));
				break;
			case SSD_REQ_TYP_WR:
				m_recv_buf->set_bus_pkt_prio(0);
				m_recv_buf->set_bus_pkt_delay(sc_time(HOST_BUS_IO_CYCLE * HOST_BUS_HEADER_SIZE, SC_MS));
				break;
			default:
				assert(0);
				break;
		}

		/* creating sub-request */
		for (lpn_t op_lpn = m_recv_buf->get_req_lpn();
				op_lpn < m_recv_buf->get_req_lpn() + m_recv_buf->get_req_size();
				op_lpn++) {

			int die_index = op_lpn % die_schedulers.size();
			lpn_t die_op_lpn = op_lpn / die_schedulers.size();

			switch (m_recv_buf->get_req_type()) {
				case SSD_REQ_TYP_RD:
					die_schedulers[die_index]->schedule_io_page_read(m_recv_buf, die_op_lpn);
					break;
				case SSD_REQ_TYP_WR:
					die_schedulers[die_index]->schedule_io_page_write(m_recv_buf, die_op_lpn);
					break;
				default:
					assert(0);
					break;
			}
		}
		m_recv_buf = NULL;
		assert(m_queue_size < SSD_QUEUE_DEPTH);
		m_queue_size++;
		if (m_queue_size < SSD_QUEUE_DEPTH) {
			download_ch_port->require_recv_next_pkt();
		}
	} else if (bus_id == m_upload_ch_id) {
		assert(0);
	} else {
		assert(0);
	}
}

#ifdef DEADLINE_AWARE_SLC_POLICY_SELF_TUNING
void simple_ssd_controller::tla() {
	assert(m_tla_next_pr99 != NULL_MSEC);
	if (m_tla_next_pr99 > DEADLINE_AWARE_SLC_POLICY_DEADLINE__MAX) {
		if ((m_tla_next_pr99 - m_tla_prev_1_pr99) * (m_tla_next_targ - m_tla_prev_1_targ) > 0) {
			m_tla_prev_2_targ = m_tla_prev_1_targ;
			m_tla_prev_2_pr99 = m_tla_prev_1_pr99;
		} else {
			m_tla_prev_2_targ = DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE;
			m_tla_prev_2_pr99 = DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE;
		}
		m_tla_prev_1_targ = m_tla_next_targ;
		m_tla_prev_1_pr99 = m_tla_next_pr99;
		if (m_tla_prev_2_targ != DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE
				&& m_tla_prev_2_pr99 != DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE) {
			m_tla_next_targ = m_tla_prev_1_targ + (DEADLINE_AWARE_SLC_POLICY_DEADLINE__MAX - m_tla_prev_1_pr99)
				* (m_tla_prev_1_targ - m_tla_prev_2_targ) / (m_tla_prev_1_pr99 - m_tla_prev_2_pr99);
		} else {
			m_tla_next_targ = m_tla_prev_1_targ + (DEADLINE_AWARE_SLC_POLICY_DEADLINE__MAX - m_tla_prev_1_pr99);
		}
		if (m_tla_next_targ > DEADLINE_AWARE_SLC_POLICY_DEADLINE) {
			m_tla_next_targ = DEADLINE_AWARE_SLC_POLICY_DEADLINE;
		} else if (m_tla_next_targ < 0) {
			m_tla_next_targ = 0;
		}
	} else if (m_tla_next_pr99 < DEADLINE_AWARE_SLC_POLICY_DEADLINE__MIN) {
		if ((m_tla_next_pr99 - m_tla_prev_1_pr99) * (m_tla_next_targ - m_tla_prev_1_targ) > 0) {
			m_tla_prev_2_targ = m_tla_prev_1_targ;
			m_tla_prev_2_pr99 = m_tla_prev_1_pr99;
		} else {
			m_tla_prev_2_targ = DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE;
			m_tla_prev_2_pr99 = DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE;
		}
		m_tla_prev_1_targ = m_tla_next_targ;
		m_tla_prev_1_pr99 = m_tla_next_pr99;
		if (m_tla_prev_2_targ != DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE
				&& m_tla_prev_2_pr99 != DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE) {
			m_tla_next_targ = m_tla_prev_1_targ + (DEADLINE_AWARE_SLC_POLICY_DEADLINE__MIN - m_tla_prev_1_pr99)
				* (m_tla_prev_1_targ - m_tla_prev_2_targ) / (m_tla_prev_1_pr99 - m_tla_prev_2_pr99);
		} else {
			m_tla_next_targ = m_tla_prev_1_targ + (DEADLINE_AWARE_SLC_POLICY_DEADLINE__MIN - m_tla_prev_1_pr99);
		}
		if (m_tla_next_targ > DEADLINE_AWARE_SLC_POLICY_DEADLINE) {
			m_tla_next_targ = DEADLINE_AWARE_SLC_POLICY_DEADLINE;
		} else if (m_tla_next_targ < 0) {
			m_tla_next_targ = 0;
		}
	}
	m_tla_next_pr99 = NULL_MSEC;
}

#endif


