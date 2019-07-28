#ifndef __SIMPLE_IO_SCHEDULER_H__
#define __SIMPLE_IO_SCHEDULER_H__

#include <fstream>
#include <map>
#include <string>
#include <queue>

#include "systemc.h"

#include "pmf_builder.h"
#include "trace_dev_info.h"

#include "ssd_typedef.h"
#include "gyc_bus.h"

class simple_io_scheduler :
	public sc_core::sc_module,
	public gyc_bus_dev_if
{
	private:
		gyc_bus_pkt_mm* m_mm;
		std::ifstream m_trace_fs;
		int m_trace_active_dev_idx;
		std::map<int, trace_dev_info> m_trace_dev_info_table;
#ifdef IO_SCHEDULER__READ_OVER_WRITE
		std::deque<gyc_bus_pkt *> m_row_rd_req_queue;
		std::deque<gyc_bus_pkt *> m_row_wr_req_queue;
#else
		std::deque<gyc_bus_pkt *> m_noop_req_queue;
#endif
		void load_trace_fs(std::string trace_path, int trace_active_dev_idx);
		void trace_replayer();

#ifdef GYC_PAPER_REQ_LATENCY_PLOT
		void req_latency_plot();
		pmf_builder<msec_t> m_sampled_rd_req_latency_pmf;
		pmf_builder<msec_t> m_sampled_wr_req_latency_pmf;
		std::ofstream m_sampled_req_latency_fs;
#endif

#ifdef GYC_PAPER_REQ_RATE_PLOT
		void req_rate_plot_sampler();
		void req_rate_plot();
		lpn_t m_sampled_rd_req_rate;
		lpn_t m_sampled_wr_req_rate;
		pmf_builder<lpn_t> m_sampled_rd_req_rate_pmf;
		pmf_builder<lpn_t> m_sampled_wr_req_rate_pmf;
		std::ofstream m_sampled_req_rate_fs;
		lpn_t m_global_max_rd_req_rate;
		lpn_t m_global_max_wr_req_rate;
		lpn_t m_global_avg_nume_rd_req_rate;
		lpn_t m_global_avg_nume_wr_req_rate;
		lpn_t m_global_avg_deno_rd_req_rate;
		lpn_t m_global_avg_deno_wr_req_rate;
#endif

	public:
#ifdef IO_SCHEDULER__RECORD_ALL_LATENCY
		pmf_builder<msec_t> m_req_resp_pmf;
#endif
#ifdef IO_SCHEDULER__RECORD_READ_LATENCY
		pmf_builder<msec_t> m_rd_req_resp_pmf;
#endif
#ifdef IO_SCHEDULER__RECORD_WRITE_LATENCY
		pmf_builder<msec_t> m_wr_req_resp_pmf;
#endif

		SC_HAS_PROCESS(simple_io_scheduler);
		simple_io_scheduler(
				sc_core::sc_module_name module_name,
				gyc_bus_pkt_mm* global_mm,
				std::string trace_filename,
				int trace_active_dev_idx);

		sc_port<gyc_bus_dev_cb> download_ch_port;
		sc_port<gyc_bus_dev_cb> upload_ch_port;

		int get_minimum_number_of_dice_required() const;

	private:
		gyc_bus_pkt* m_recv_buf;
		int m_download_ch_id;
		int m_upload_ch_id;
		bool m_download_ch_busy;
		bool m_upload_ch_busy;
	public:
		void set_download_ch_id(int download_ch_id) { m_download_ch_id = download_ch_id; }
		int get_download_ch_id() const { return m_download_ch_id; }
		void set_upload_ch_id(int upload_ch_id) { m_upload_ch_id = upload_ch_id; }
		int get_upload_ch_id() const { return m_upload_ch_id; }

		void print_score() const;

		/* ======================== */
		/* implement gyc_bus_dev_if */
		/* ======================== */
		gyc_bus_pkt* peek_next_pkt_to_send(int bus_id) const;
		void on_send_started(int bus_id);
		void on_send_completed(int bus_id);
		bool can_recv_next_pkt(int bus_id, const gyc_bus_pkt* next_pkt_to_recv) const;
		void recv_next_pkt(int bus_id, gyc_bus_pkt* next_pkt_to_recv);
		void on_recv_completed(int bus_id);
};

#endif

