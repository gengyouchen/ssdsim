#include "simple_io_scheduler.h"

#include <cstdlib>
#include <cmath>
#include <utility>
#include <map>

#include "systemc.h"

#include "ssd_typedef.h"
#include "gyc_bus.h"

using namespace std;
using namespace sc_core;

void simple_io_scheduler::load_trace_fs(std::string trace_path, int trace_active_dev_idx) {

	cout << "[INFO] use disk " << trace_active_dev_idx << " in trace file (" << trace_path << ") for simulation" << endl;

	string trace_stat_path = trace_path + ".stat";

	ifstream trace_stat_ifs;
	trace_stat_ifs.open(trace_stat_path.c_str());
	if (trace_stat_ifs.is_open()) {
		int dev_idx;
		lpn_t dev_size;
		while (trace_stat_ifs >> dev_idx >> dev_size) {
			m_trace_dev_info_table[dev_idx].set_size_in_sectors(dev_size);
		}
		trace_stat_ifs.close();
	}

	if (m_trace_dev_info_table.empty()) {
		cout << "[INFO] trace stat cache file (" << trace_stat_path << ") is invalid or not existing" << endl;
		cout << "[INFO] scanning trace file (" << trace_path << ") to build trace stat, please wait..." << endl;
		ifstream trace_ifs;
		trace_ifs.open(trace_path.c_str());
		if (!trace_ifs.is_open()) {
			cerr << "[ERROR] cannot open trace file (" << trace_path << ")" << endl;
			exit(-1);
		}
		msec_t req_time;
		int req_dev;
		lpn_t req_addr;
		lpn_t req_size;
		int req_op;
		tran_id_t line_num = 0;
		while (trace_ifs >> req_time >> req_dev >> req_addr >> req_size >> req_op) {
			assert(req_time >= 0.0);
			assert(req_addr >= 0);
			assert(req_size >= 1);
			assert(req_op == 0 || req_op == 1);
			m_trace_dev_info_table[req_dev].record_request(req_addr, req_size);
			line_num++;
		}
		trace_ifs.close();
		if (m_trace_dev_info_table.empty()) {
			cerr << "[ERROR] trace file (" << trace_path << ") doesn't contain any disk" << endl;
			exit(-1);
		}
		cout << "[INFO] finish scanning total " << line_num << " lines in trace file (" << trace_path << ")" << endl;

		ofstream trace_stat_ofs;
		trace_stat_ofs.open(trace_stat_path.c_str());
		if (trace_stat_ofs.is_open()) {
			for (map<int, trace_dev_info>::iterator iter = m_trace_dev_info_table.begin();
					iter != m_trace_dev_info_table.end(); ++iter) {
				int dev_idx = iter->first;
				lpn_t dev_size = iter->second.get_size_in_sectors();
				trace_stat_ofs << fixed << dec << dev_idx << '\t';
				trace_stat_ofs << fixed << dec << dev_size << endl;
			}
			trace_stat_ofs.close();
			cout << "[INFO] saved trace stat cache file (" << trace_stat_path << ")" << endl;
		}
	} else {
		cout << "[INFO] found trace stat cache file (" << trace_stat_path << ") and directly use it" << endl;
	}

	if (m_trace_dev_info_table.find(trace_active_dev_idx) == m_trace_dev_info_table.end()) {
		cerr << "[ERROR] trace file (" << trace_path << ") doesn't contain disk " << trace_active_dev_idx << endl;
		exit(-1);
	}
	m_trace_active_dev_idx = trace_active_dev_idx;

	m_trace_fs.open(trace_path.c_str());
	if (!m_trace_fs.is_open()) {
		cerr << "[ERROR] cannot open trace file (" << trace_path << ")" << endl;
		exit(-1);
	}
}

int simple_io_scheduler::get_minimum_number_of_dice_required() const {
	map<int, trace_dev_info>::const_iterator iter = m_trace_dev_info_table.find(m_trace_active_dev_idx);
	assert(iter != m_trace_dev_info_table.end());
	lpn_t dev_size_in_sectors = iter->second.get_size_in_sectors();
	lpn_t last_sector_addr = dev_size_in_sectors - 1;
	lpn_t last_page_addr = last_sector_addr / SECTOR_PER_PAGE;
	lpn_t dev_size_in_pages = last_page_addr + 1;
	int min_num_of_dice_required = static_cast<int>(ceil(static_cast<long double>(dev_size_in_pages) / USER_CAPACITY_IN_PAGE_PER_DIE));
	return min_num_of_dice_required;
}

void simple_io_scheduler::trace_replayer() {

	msec_t req_time;
	int req_dev;
	lpn_t req_addr;
	lpn_t req_size;
	int req_op;

	/* we use line number as each I/O transaction' id */
	tran_id_t line_number = 0;

	cout << "Start simulation..." << endl;
	while (m_trace_fs >> req_time >> req_dev >> req_addr >> req_size >> req_op) {

		/* check request format */
		if (req_dev != m_trace_active_dev_idx) {
			continue;
		}
		assert(req_time >= 0.0);
		assert(req_addr >= 0);
		assert(req_size >= 1);
		assert(req_op == 0 || req_op == 1);
		//req_time *= 1.05;
		//if (req_time > 86400.0 * 1000) {
		//	break;
		//}

		/* wait until request is happened */
		assert(sc_time(req_time, SC_MS) >= sc_time_stamp());
		if (sc_time(req_time, SC_MS) > sc_time_stamp()) {
			wait(sc_time(req_time, SC_MS) - sc_time_stamp());
		}
		assert(sc_time(req_time, SC_MS) == sc_time_stamp());

		/* convert unit from sector to page */
		lpn_t first_page_addr = req_addr / SECTOR_PER_PAGE;
		lpn_t last_page_addr = (req_addr + req_size - 1) / SECTOR_PER_PAGE;
		lpn_t page_count = last_page_addr - first_page_addr + 1;

		/* create new I/O transaction */
		gyc_bus_pkt* new_tran = m_mm->alloc();
		new_tran->set_bus_pkt_dest(1);
		new_tran->enable_ssd_req_ext();
		new_tran->set_req_lpn(first_page_addr);
		new_tran->set_req_size(page_count);
		new_tran->set_req_remain_size(page_count);
		new_tran->set_req_arrival_time(sc_time(req_time, SC_MS));
		if (req_op == 1) {
			new_tran->set_req_type(SSD_REQ_TYP_RD);
			new_tran->set_bus_pkt_prio(1);
			new_tran->set_bus_pkt_delay(sc_time(HOST_BUS_IO_CYCLE * HOST_BUS_HEADER_SIZE, SC_MS));
		} else if (req_op == 0) {
			new_tran->set_req_type(SSD_REQ_TYP_WR);
			new_tran->set_bus_pkt_prio(0);
			new_tran->set_bus_pkt_delay(sc_time(HOST_BUS_IO_CYCLE * (HOST_BUS_HEADER_SIZE + new_tran->get_req_size() * SECTOR_PER_PAGE * BYTE_PER_SECTOR), SC_MS));
		} else {
			assert(0);
		}

#ifdef GYC_PAPER_REQ_RATE_PLOT
		switch (new_tran->get_req_type()) {
			case SSD_REQ_TYP_RD:
				m_sampled_rd_req_rate++;
				//m_sampled_rd_req_rate += new_tran->get_req_size();
				break;
			case SSD_REQ_TYP_WR:
				m_sampled_wr_req_rate++;
				//m_sampled_wr_req_rate += new_tran->get_req_size();
				break;
			default:
				assert(0);
				break;
		}
#endif

#ifdef IO_SCHEDULER__READ_OVER_WRITE
		switch (new_tran->get_req_type()) {
			case SSD_REQ_TYP_RD:
				m_row_rd_req_queue.push_back(new_tran);
				break;
			case SSD_REQ_TYP_WR:
				m_row_wr_req_queue.push_back(new_tran);
				break;
			default:
				assert(0);
				break;
		}
#else
		m_noop_req_queue.push_back(new_tran);
#endif

		if (peek_next_pkt_to_send(m_download_ch_id)) {
			download_ch_port->require_send_next_pkt();
		}

		line_number++;
	}

	cout << "Reached the end of trace file with total number of transactions = " << line_number << endl;
	m_trace_fs.close();
}

simple_io_scheduler::simple_io_scheduler(
		sc_module_name module_name,
		gyc_bus_pkt_mm* global_mm,
		string trace_filename,
		int trace_active_dev_idx) :
	sc_module(module_name),
	download_ch_port("download_ch_port"),
	upload_ch_port("upload_ch_port")
{

	assert(global_mm);
	m_mm = global_mm;

	load_trace_fs(trace_filename, trace_active_dev_idx);

	m_recv_buf = NULL;
	m_download_ch_id = -1;
	m_upload_ch_id = -1;
	m_download_ch_busy = false;
	m_upload_ch_busy = false;

#ifdef GYC_PAPER_REQ_LATENCY_PLOT
	m_global_max_rd_req_rate = 0;
	m_global_max_wr_req_rate = 0;
	m_global_avg_nume_rd_req_rate = 0;
	m_global_avg_nume_wr_req_rate = 0;
	m_global_avg_deno_rd_req_rate = 0;
	m_global_avg_deno_wr_req_rate = 0;
	m_sampled_req_latency_fs.open("req_latency.txt");
	SC_THREAD(req_latency_plot);
#endif

#ifdef GYC_PAPER_REQ_RATE_PLOT
	m_sampled_rd_req_rate = 0;
	m_sampled_wr_req_rate = 0;
	m_sampled_req_rate_fs.open("req_rate.txt");
	SC_THREAD(req_rate_plot_sampler);
	SC_THREAD(req_rate_plot);
#endif

	SC_THREAD(trace_replayer);
}

void simple_io_scheduler::print_score() const {

#ifdef IO_SCHEDULER__RECORD_ALL_LATENCY
	cout << "For total " << m_req_resp_pmf.count() << " I/O requests:" << endl;
	cout << '\t' << "average I/O latency = " << m_req_resp_pmf.avg() << " ms" << endl;
	cout << '\t' << "99th percentile I/O latency = " << m_req_resp_pmf.percentile(0.99) << " ms" << endl;
	cout << '\t' << "Number of Requests = " << m_req_resp_pmf.count() << endl;
	cout << '\t' << "Number of SLO Miss Requests = " << m_req_resp_pmf.count_with_range(
			DEADLINE_AWARE_SLC_POLICY_DEADLINE, m_req_resp_pmf.max()) << endl;
	assert(m_req_resp_pmf.count_with_range(m_req_resp_pmf.min(), m_req_resp_pmf.max())
			== m_req_resp_pmf.count());
#endif

#ifdef IO_SCHEDULER__RECORD_READ_LATENCY
	cout << "For total " << m_rd_req_resp_pmf.count() << " read requests:" << endl;
	cout << '\t' << "average read latency = " << m_rd_req_resp_pmf.avg() << " ms" << endl;
	cout << '\t' << "99th percentile read latency = " << m_rd_req_resp_pmf.percentile(0.99) << " ms" << endl;
#endif

#ifdef IO_SCHEDULER__RECORD_WRITE_LATENCY
	cout << "For total " << m_wr_req_resp_pmf.count() << " write requests:" << endl;
	cout << '\t' << "average write latency = " << m_wr_req_resp_pmf.avg() << " ms" << endl;
	cout << '\t' << "99th percentile write latency = " << m_wr_req_resp_pmf.percentile(0.99) << " ms" << endl;
#endif
}

/* ======================== */
/* implement gyc_bus_dev_if */
/* ======================== */

gyc_bus_pkt* simple_io_scheduler::peek_next_pkt_to_send(int bus_id) const {
	if (bus_id == m_download_ch_id) {
#ifdef IO_SCHEDULER__READ_OVER_WRITE
		if (!m_download_ch_busy && !m_row_rd_req_queue.empty()) {
			return m_row_rd_req_queue.front();
		}
		if (!m_download_ch_busy && !m_row_wr_req_queue.empty()) {
			return m_row_wr_req_queue.front();
		}
		return NULL;
#else
		if (!m_download_ch_busy && !m_noop_req_queue.empty()) {
			return m_noop_req_queue.front();
		}
		return NULL;
#endif
	} else if (bus_id == m_upload_ch_id) {
		return NULL;
	} else {
		assert(0);
		return NULL;
	}
	assert(0);
	return NULL;
}

void simple_io_scheduler::on_send_started(int bus_id) {
	if (bus_id == m_download_ch_id) {
#ifdef IO_SCHEDULER__READ_OVER_WRITE
		assert(!m_download_ch_busy);
		m_download_ch_busy = true;
		if (!m_row_rd_req_queue.empty()) {
			m_row_rd_req_queue.pop_front();
		} else if (!m_row_wr_req_queue.empty()) {
			m_row_wr_req_queue.pop_front();
		} else {
			assert(0);
		}
		return;
#else
		assert(!m_download_ch_busy);
		assert(!m_noop_req_queue.empty());
		m_download_ch_busy = true;
		m_noop_req_queue.pop_front();
		return;
#endif
	} else if (bus_id == m_upload_ch_id) {
		assert(0);
	} else {
		assert(0);
	}
	assert(0);
}

void simple_io_scheduler::on_send_completed(int bus_id) {
	if (bus_id == m_download_ch_id) {
#ifdef IO_SCHEDULER__READ_OVER_WRITE
		assert(m_download_ch_busy);
		m_download_ch_busy = false;
		if (!m_row_rd_req_queue.empty() || !m_row_wr_req_queue.empty()) {
			download_ch_port->require_send_next_pkt();
		}
		return;
#else
		assert(m_download_ch_busy);
		m_download_ch_busy = false;
		if (!m_noop_req_queue.empty()) {
			download_ch_port->require_send_next_pkt();
		}
		return;
#endif
	} else if (bus_id == m_upload_ch_id) {
		assert(0);
	} else {
		assert(0);
	}
	assert(0);
}

bool simple_io_scheduler::can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt_to_recv) const {
	if (bus_id == m_download_ch_id) {
		return false;
	} else if (bus_id == m_upload_ch_id) {
		if (!m_upload_ch_busy && !m_recv_buf) {
			return true;
		}
		return false;
	} else {
		assert(0);
		return false;
	}
}

void simple_io_scheduler::recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt_to_recv) {
	if (bus_id == m_download_ch_id) {
		assert(0);
	} else if (bus_id == m_upload_ch_id) {
		assert(!m_upload_ch_busy);
		assert(!m_recv_buf);
		m_upload_ch_busy = true;
		m_recv_buf = next_pkt_to_recv;
	} else {
		assert(0);
	}
}

void simple_io_scheduler::on_recv_completed(int bus_id) {
	if (bus_id == m_download_ch_id) {
		assert(0);
	} else if (bus_id == m_upload_ch_id) {
		assert(m_upload_ch_busy);
		m_upload_ch_busy = false;

		assert(m_recv_buf);
		assert(m_recv_buf->has_ssd_req_ext());
		assert(m_recv_buf->get_bus_pkt_dest() == 0);
		assert(m_recv_buf->get_req_remain_size() == 0);
		assert(sc_time_stamp() > m_recv_buf->get_req_arrival_time());
		msec_t resp_time = (sc_time_stamp() - m_recv_buf->get_req_arrival_time()).to_seconds() * 1000;

		switch (m_recv_buf->get_req_type()) {
			case SSD_REQ_TYP_RD:
#ifdef IO_SCHEDULER__RECORD_ALL_LATENCY
				m_req_resp_pmf.record_value(resp_time);
#endif
#ifdef IO_SCHEDULER__RECORD_READ_LATENCY
				m_rd_req_resp_pmf.record_value(resp_time);
#endif
#ifdef GYC_PAPER_REQ_LATENCY_PLOT
				m_sampled_rd_req_latency_pmf.record_value(resp_time);
#endif
				break;
			case SSD_REQ_TYP_WR:
#ifdef IO_SCHEDULER__RECORD_ALL_LATENCY
				m_req_resp_pmf.record_value(resp_time);
#endif
#ifdef IO_SCHEDULER__RECORD_WRITE_LATENCY
				m_wr_req_resp_pmf.record_value(resp_time);
#endif
#ifdef GYC_PAPER_REQ_LATENCY_PLOT
				m_sampled_wr_req_latency_pmf.record_value(resp_time);
#endif
				break;
			default:
				assert(0);
				break;
		}

		m_recv_buf->free();
		m_recv_buf = NULL;

		upload_ch_port->require_recv_next_pkt();
	} else {
		assert(0);
	}
}

#ifdef GYC_PAPER_REQ_LATENCY_PLOT
void simple_io_scheduler::req_latency_plot() {
	while (true) {
		if (m_trace_fs.is_open() || m_mm->count() > 0) {
			wait(sc_time(GYC_PAPER_REQ_LATENCY_PLOT__TIME_INTERVAL, SC_MS));
		} else {
			return;
		}

		msec_t sampled_avg_rd_req_latency = m_sampled_rd_req_latency_pmf.avg();
		msec_t sampled_peak_rd_req_latency = m_sampled_rd_req_latency_pmf.percentile(0.99);
		m_sampled_rd_req_latency_pmf.reset();

		msec_t sampled_avg_wr_req_latency = m_sampled_wr_req_latency_pmf.avg();
		msec_t sampled_peak_wr_req_latency = m_sampled_wr_req_latency_pmf.percentile(0.99);
		m_sampled_wr_req_latency_pmf.reset();

		assert(m_sampled_req_latency_fs.is_open());
		int interval_index = static_cast<msec_t>(sc_time_stamp().to_seconds() * 1000.0) / GYC_PAPER_REQ_LATENCY_PLOT__TIME_INTERVAL;

		m_sampled_req_latency_fs << fixed << dec << interval_index;
		m_sampled_req_latency_fs << '\t' << sampled_avg_rd_req_latency;
		m_sampled_req_latency_fs << '\t' << sampled_peak_rd_req_latency;
		m_sampled_req_latency_fs << '\t' << sampled_avg_wr_req_latency;
		m_sampled_req_latency_fs << '\t' << sampled_peak_wr_req_latency;
		m_sampled_req_latency_fs << endl;
	}
}
#endif

#ifdef GYC_PAPER_REQ_RATE_PLOT
void simple_io_scheduler::req_rate_plot_sampler() {
	while (true) {
		if (m_trace_fs.is_open() || m_mm->count() > 0) {
			wait(sc_time(GYC_PAPER_REQ_RATE_PLOT__SAMPLE_INTERVAL, SC_MS));
		} else {
			return;
		}

		if (m_sampled_rd_req_rate > m_global_max_rd_req_rate) {
			m_global_max_rd_req_rate = m_sampled_rd_req_rate;
		}
		m_global_avg_nume_rd_req_rate += m_sampled_rd_req_rate;
		m_global_avg_deno_rd_req_rate++;
		m_sampled_rd_req_rate_pmf.record_value(m_sampled_rd_req_rate);
		m_sampled_rd_req_rate = 0;

		if (m_sampled_wr_req_rate > m_global_max_wr_req_rate) {
			m_global_max_wr_req_rate = m_sampled_wr_req_rate;
		}
		m_global_avg_nume_wr_req_rate += m_sampled_wr_req_rate;
		m_global_avg_deno_wr_req_rate++;
		m_sampled_wr_req_rate_pmf.record_value(m_sampled_wr_req_rate);
		m_sampled_wr_req_rate = 0;
	}
}
#endif

#ifdef GYC_PAPER_REQ_RATE_PLOT
void simple_io_scheduler::req_rate_plot() {
	while (true) {
		if (m_trace_fs.is_open() || m_mm->count() > 0) {
			wait(sc_time(GYC_PAPER_REQ_RATE_PLOT__TIME_INTERVAL, SC_MS));
		} else {
			assert(m_sampled_req_rate_fs.is_open());
			long double m_global_avg_rd_req_rate = (m_global_avg_deno_rd_req_rate > 0) ? (
				static_cast<long double>(m_global_avg_nume_rd_req_rate) /
				m_global_avg_deno_rd_req_rate) : 0;
			long double m_global_avg_wr_req_rate = (m_global_avg_deno_wr_req_rate > 0) ? (
				static_cast<long double>(m_global_avg_nume_wr_req_rate) /
				m_global_avg_deno_wr_req_rate) : 0;
			m_sampled_req_rate_fs << "#!";
			m_sampled_req_rate_fs << '\t' << m_global_avg_rd_req_rate;
			m_sampled_req_rate_fs << '\t' << m_global_max_rd_req_rate;
			m_sampled_req_rate_fs << '\t' << m_global_avg_wr_req_rate;
			m_sampled_req_rate_fs << '\t' << m_global_max_wr_req_rate;
			m_sampled_req_rate_fs << endl;
			return;
		}

		msec_t sampled_avg_rd_req_rate = m_sampled_rd_req_rate_pmf.avg();
		msec_t sampled_peak_rd_req_rate = m_sampled_rd_req_rate_pmf.percentile(0.99);
		m_sampled_rd_req_rate_pmf.reset();

		msec_t sampled_avg_wr_req_rate = m_sampled_wr_req_rate_pmf.avg();
		msec_t sampled_peak_wr_req_rate = m_sampled_wr_req_rate_pmf.percentile(0.99);
		m_sampled_wr_req_rate_pmf.reset();

		int interval_index = static_cast<msec_t>(sc_time_stamp().to_seconds() * 1000.0) / GYC_PAPER_REQ_RATE_PLOT__TIME_INTERVAL;
		assert(m_sampled_req_rate_fs.is_open());
		m_sampled_req_rate_fs << fixed << dec << interval_index;
		m_sampled_req_rate_fs << '\t' << sampled_avg_rd_req_rate;
		m_sampled_req_rate_fs << '\t' << sampled_peak_rd_req_rate;
		m_sampled_req_rate_fs << '\t' << sampled_avg_wr_req_rate;
		m_sampled_req_rate_fs << '\t' << sampled_peak_wr_req_rate;
		m_sampled_req_rate_fs << endl;
	}
}
#endif

