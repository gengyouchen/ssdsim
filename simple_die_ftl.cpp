#include <limits>

#include "simple_die_ftl.h"

#include "ssd_typedef.h"

#include "gyc_ftl_config.h"
#include "gyc_ftl_typedef.h"
#include "gyc_ftl_assert.h"
#include "gyc_ftl_foreach.h"
#include "gyc_ftl_p_meta_table.h"
#include "gyc_ftl_wr_ptr.h"

using namespace std;

ssd_die_ftl_callback* simple_die_ftl::get_callback() const {
	return _callback;
}

void simple_die_ftl::set_callback(ssd_die_ftl_callback* callback) {
	_callback = callback;
}

void simple_die_ftl::reset_data_layout() {
	lp_meta_table = (lp_meta_t *) malloc(sizeof(lp_meta_t) * USER_CAPACITY_IN_PAGE_PER_DIE);
	assert(lp_meta_table);

	p_meta_table = create_p_meta_table(BLOCK_PER_DIE, PAGE_PER_BLOCK);

#ifdef ADD_DUMMY_TO_INITIAL_LAYOUT

	const pbn_t num_of_head_pb_caused_by_wr_ptr = 3; /* i.e. (1) wr_ptr_for_gc, (2) wr_ptr_for_short_io_queue, (3) wr_ptr_for_long_io_queue */
	const pbn_t num_of_pb_to_be_filled = BLOCK_PER_DIE * (100 - min_free_blocks_percentage) / 100 - num_of_head_pb_caused_by_wr_ptr;
	const ppn_t num_of_pp_to_be_filled = num_of_pb_to_be_filled * PAGE_PER_BLOCK;
	const ppn_t num_of_live_pp_to_be_filled = USER_CAPACITY_IN_PAGE_PER_DIE;
	const ppn_t num_of_dead_pp_to_be_filled = num_of_pp_to_be_filled - USER_CAPACITY_IN_PAGE_PER_DIE;

	wr_ptr_for_gc = create_wr_ptr(p_meta_table, MLC_MODE);

	ppn_t num_of_pp_has_been_filled = 0;
	ppn_t num_of_live_pp_has_been_filled = 0;
	ppn_t num_of_dead_pp_has_been_filled = 0;

	while (true) {

		assert(num_of_pp_to_be_filled - num_of_pp_has_been_filled >= 0);
		assert(num_of_live_pp_to_be_filled - num_of_live_pp_has_been_filled >= 0);
		assert(num_of_dead_pp_to_be_filled - num_of_dead_pp_has_been_filled >= 0);

		if (num_of_pp_to_be_filled - num_of_pp_has_been_filled == 0) {
			assert(num_of_live_pp_to_be_filled - num_of_live_pp_has_been_filled == 0);
			assert(num_of_dead_pp_to_be_filled - num_of_dead_pp_has_been_filled == 0);
			break;
		} else if (num_of_live_pp_to_be_filled - num_of_live_pp_has_been_filled == 0) {
			assert(num_of_pp_to_be_filled - num_of_pp_has_been_filled == num_of_dead_pp_to_be_filled - num_of_dead_pp_has_been_filled);
			write_dummy_to_wr_ptr(p_meta_table, wr_ptr_for_gc);
			num_of_dead_pp_has_been_filled++;
			num_of_pp_has_been_filled++;
		} else if (num_of_dead_pp_to_be_filled - num_of_dead_pp_has_been_filled == 0) {
			assert(num_of_pp_to_be_filled - num_of_pp_has_been_filled == num_of_live_pp_to_be_filled - num_of_live_pp_has_been_filled);
			lp_meta_table[num_of_live_pp_has_been_filled].ptr_to_physical_page = write_lp_to_wr_ptr(p_meta_table, wr_ptr_for_gc, num_of_live_pp_has_been_filled);
			num_of_live_pp_has_been_filled++;
			num_of_pp_has_been_filled++;
		} else {
			assert((num_of_pp_to_be_filled - num_of_pp_has_been_filled) == (num_of_dead_pp_to_be_filled - num_of_dead_pp_has_been_filled) + (num_of_live_pp_to_be_filled - num_of_live_pp_has_been_filled));
			if (rand() % num_of_pp_to_be_filled < num_of_dead_pp_to_be_filled) {
				write_dummy_to_wr_ptr(p_meta_table, wr_ptr_for_gc);
				num_of_dead_pp_has_been_filled++;
				num_of_pp_has_been_filled++;
			} else {
				lp_meta_table[num_of_live_pp_has_been_filled].ptr_to_physical_page = write_lp_to_wr_ptr(p_meta_table, wr_ptr_for_gc, num_of_live_pp_has_been_filled);
				num_of_live_pp_has_been_filled++;
				num_of_pp_has_been_filled++;
			}
		}
	}
#else

	/* simply write all initial data sequentially via wr_ptr_for_gc */
	wr_ptr_for_gc = create_wr_ptr(p_meta_table, MLC_MODE);
	for (lpn_t lpn = 0; lpn < USER_CAPACITY_IN_PAGE_PER_DIE; lpn++) {
		ppn_t new_pp = write_lp_to_wr_ptr(p_meta_table, wr_ptr_for_gc, lpn);
		lp_meta_table[lpn].ptr_to_physical_page = new_pp;
		assert(lpn == new_pp);
	}

#endif

	switch (slc_policy) {
		case NO_SLC_POLICY:
			wr_ptr_for_short_io_queue = create_wr_ptr(p_meta_table, MLC_MODE);
			wr_ptr_for_long_io_queue = create_wr_ptr(p_meta_table, MLC_MODE);
			break;
		case QUEUE_AWARE_SLC_POLICY:
			wr_ptr_for_short_io_queue = create_wr_ptr(p_meta_table, MLC_MODE);
			wr_ptr_for_long_io_queue = create_wr_ptr(p_meta_table, SLC_MODE);
			break;
		case ALWAYS_SLC_POLICY:
			wr_ptr_for_short_io_queue = create_wr_ptr(p_meta_table, SLC_MODE);
			wr_ptr_for_long_io_queue = create_wr_ptr(p_meta_table, SLC_MODE);
			break;
		default:
			assert(0);
			break;
	}

	assert(wr_ptr_for_short_io_queue);
	assert(wr_ptr_for_long_io_queue);
	assert(wr_ptr_for_gc);

#ifdef ADD_DUMMY_TO_INITIAL_LAYOUT

	assert(p_meta_table->num_of_free_pbs * 100 > p_meta_table->num_of_all_pbs * min_free_blocks_percentage);
	assert((p_meta_table->num_of_free_pbs - 1) * 100 <= p_meta_table->num_of_all_pbs * min_free_blocks_percentage);

#endif

}

bool simple_die_ftl::check_mapping_validity(lpn_t logical_page, ppn_t physical_page) const {

	assert(logical_page < USER_CAPACITY_IN_PAGE_PER_DIE);

	ASSERT_IS_VALID_PPN(p_meta_table, physical_page);

	ppn_t correct_physical_page = lp_meta_table[logical_page].ptr_to_physical_page;
	assert(find_lpn_by_ppn(p_meta_table, correct_physical_page) == logical_page);

	return correct_physical_page == physical_page;
}

msec_t simple_die_ftl::issue_page_read(lpn_t logical_page, bool generated_by_gc) {

	assert(logical_page >= 0);
	assert(logical_page < USER_CAPACITY_IN_PAGE_PER_DIE);

	schedule_gc_if_needed();
	return MLC_READ_LATENCY;
}

msec_t simple_die_ftl::issue_page_write(lpn_t logical_page, bool generated_by_gc) {

	assert(logical_page >= 0);
	assert(logical_page < USER_CAPACITY_IN_PAGE_PER_DIE);

	/* ============================ */
	/* invalidate old physical page */
	/* ============================ */

	ppn_t old_physical_page = lp_meta_table[logical_page].ptr_to_physical_page;
	ASSERT_IS_VALID_PPN(p_meta_table, old_physical_page);

	if (generated_by_gc) {
		ASSERT_IS_VALID_TAIL_PB(p_meta_table, find_pbn_by_ppn(p_meta_table, old_physical_page));
	}

	delete_pp_recorded_given_lpn(p_meta_table, logical_page, old_physical_page);

	/* update FTL */
	lp_meta_table[logical_page].ptr_to_physical_page = NULL_PPN;

	/* ========================== */
	/* write to new physical page */
	/* ========================== */

	ppn_t new_pp;
	msec_t program_latency;
	if (generated_by_gc) {

		/* wr_ptr_for_gc */
		new_pp = write_lp_to_wr_ptr(p_meta_table, wr_ptr_for_gc, logical_page);
		program_latency = _determine_write_latency(wr_ptr_for_gc);

	} else if (
			slc_policy == QUEUE_AWARE_SLC_POLICY
#ifdef DEADLINE_AWARE_SLC_POLICY
			&& !_callback->can_all_pending_io_page_write_meet_deadline()
#ifdef GYC_PAPER_SLC_THROTTLING
			&& ((rand() % 100000) < (100000 * _callback->get_slc_throttle_rate()))
#endif
#ifdef GYC_PAPER_OVERLOAD_PROTECTION
			&& should_enable_slc()
			//get_current_busy_period_slc_write_utilization()
			//<= 0.2 + 0.8 * (static_cast<long double>(p_meta_table->num_of_free_pbs) * 100 / (p_meta_table->num_of_all_pbs * min_free_blocks_percentage))
			//p_meta_table->num_of_free_pbs <= 2
#endif
#endif
		  ) {

		/* wr_ptr_for_long_io_queue */
		new_pp = write_lp_to_wr_ptr(p_meta_table, wr_ptr_for_long_io_queue, logical_page);
		program_latency = _determine_write_latency(wr_ptr_for_long_io_queue);

#ifdef GYC_PAPER_OVERLOAD_PROTECTION
		m_current_busy_period_slc_write_count++;
#endif
	} else {

		/* wr_ptr_for_short_io_queue */
		new_pp = write_lp_to_wr_ptr(p_meta_table, wr_ptr_for_short_io_queue, logical_page);
		program_latency = _determine_write_latency(wr_ptr_for_short_io_queue);

#ifdef GYC_PAPER_OVERLOAD_PROTECTION
		m_current_busy_period_mlc_write_count++;
#endif
	}

	/* update FTL */
	lp_meta_table[logical_page].ptr_to_physical_page = new_pp;

	schedule_gc_if_needed();
	return program_latency;
}

msec_t simple_die_ftl::issue_block_erase(pbn_t physical_block) {
	erase_pb(p_meta_table, physical_block);

#ifdef GYC_PAPER_OVERLOAD_PROTECTION
/*
	long double free_blocks_percentage
		= static_cast<long double>(p_meta_table->num_of_free_pbs) * 100 / p_meta_table->num_of_all_pbs;
	assert(free_blocks_percentage >= 0.0);
	assert(free_blocks_percentage <= 100.0);

	if (free_blocks_percentage > 5.0) {
		long double current_ready_slc_usage = (free_blocks_percentage - 5.0) / (5.0 * 2.0);
		if (current_ready_slc_usage > m_ready_slc_usage) {
			m_ready_slc_usage = current_ready_slc_usage;
			cout << "At " << sc_time_stamp().to_seconds();
			cout << ", " << name();
			cout << " can support SLC usage up to " << m_ready_slc_usage << endl;
		}
	}
*/

	if (p_meta_table->num_of_free_pbs * 100 > p_meta_table->num_of_all_pbs * min_free_blocks_percentage) {
		m_current_busy_period_slc_write_count = 0;
		m_current_busy_period_mlc_write_count = 0;
		m_current_busy_period_has_record_slc_usage = false;
	}
#endif
	schedule_gc_if_needed();
	return MLC_ERASE_LATENCY;
}

void simple_die_ftl::print_status() {

	FOR_EACH_PB(p_meta_table, pbn, pb_meta) {
		cout << "for pb " << pbn << ": ";
		cout << "state: ";
		switch (pb_meta->state) {
			case PB_FREE:
				ASSERT_IS_VALID_FREE_PB(p_meta_table, pbn);
				cout << "PB_FREE";
				break;
			case PB_HEAD:
				ASSERT_IS_VALID_HEAD_PB(p_meta_table, pbn);
				cout << "PB_HEAD";
				break;
			case PB_BODY:
				ASSERT_IS_VALID_BODY_PB(p_meta_table, pbn);
				cout << "PB_BODY";
				break;
			case PB_TAIL:
				ASSERT_IS_VALID_TAIL_PB(p_meta_table, pbn);
				cout << "PB_TAIL";
				break;
			default:
				assert(0);
				break;
		}
		cout << ", ";
		cout << "num_of_free_pps=" << pb_meta->num_of_free_pages << ", ";
		cout << "num_of_live_pps=" << pb_meta->num_of_live_pages << ", ";
		cout << "num_of_dead_pps=" << pb_meta->num_of_dead_pages << ", ";
		cout << "num_of_er_ops=" << pb_meta->tot_num_of_er_ops << endl;
	} END_FOR_EACH_PB;

	cout << "Total Free Block: " << p_meta_table->num_of_free_pbs << endl;
	cout << "wr_ptr_for_short_io_queue: " << wr_ptr_for_short_io_queue->ptr_to_head_pb << endl;
	cout << "wr_ptr_for_long_io_queue: " << wr_ptr_for_long_io_queue->ptr_to_head_pb << endl;
	cout << "wr_ptr_for_gc: " << wr_ptr_for_gc->ptr_to_head_pb << endl;
}

void simple_die_ftl::schedule_gc_if_needed() {

#ifdef GYC_PAPER_OVERLOAD_PROTECTION

	if (m_after_foreground_gc) {
		if (_callback->get_number_of_pending_io_page_write() == 0) {
			m_after_foreground_gc = false;
			//cout << "At " << sc_time_stamp().to_seconds();
			//cout << ", " << name();
			//cout << " set m_after_foreground_gc = off" << endl;
		}
	}

	if (p_meta_table->num_of_free_pbs * 1000
			< p_meta_table->num_of_all_pbs * min_free_blocks_percentage) {

		long double target_slc_usage = get_current_busy_period_slc_write_utilization();
		if (target_slc_usage >= GYC_PAPER_OVERLOAD_PROTECTION__MAXIMUM_SLC_UTILIZATION) {
			target_slc_usage = GYC_PAPER_OVERLOAD_PROTECTION__MAXIMUM_SLC_UTILIZATION;
		}
		m_max_slc_usage = target_slc_usage;

		int possible_pr = ceil(5.0 * ((1.0 - target_slc_usage) * 1 + target_slc_usage * BIT_PER_MLC));
		if (possible_pr > min_free_blocks_percentage) {
			min_free_blocks_percentage = possible_pr;
			cout << "At " << sc_time_stamp().to_seconds();
			cout << ", " << name() << " change GC threshold to" << min_free_blocks_percentage << endl;
		}

		//if (!m_current_busy_period_has_record_slc_usage) {
		//m_current_busy_period_has_record_slc_usage = true;

		//long double next_slc_usage = get_current_busy_period_slc_write_utilization();
		//if (next_slc_usage > m_max_slc_usage) {
		//	m_max_slc_usage = next_slc_usage;
		//	min_free_blocks_percentage = ceil(5 * (1.0 + 2.0 * m_max_slc_usage));
			//cout << "At " << sc_time_stamp().to_seconds();
			//cout << ", " << name();
			//cout << " set GC threshold = " << min_free_blocks_percentage << endl;

		//}
		//}

	}
/*
	if (p_meta_table->num_of_free_pbs * 100 < p_meta_table->num_of_all_pbs * (5 * 0.5)
			&& get_current_busy_period_slc_write_utilization() > 0.2) {
		if (!m_enable_slc_throttling) {
			cout << "At " << sc_time_stamp().to_seconds() << ", " << name() << " enable SLC throttling";
			cout << " due to use many free space= " << (static_cast<long double>(p_meta_table->num_of_free_pbs) * 100 /  p_meta_table->num_of_all_pbs) << "%";
			cout << " and the slc utiliz=" << get_current_busy_period_slc_write_utilization() << endl;
			m_enable_slc_throttling = true;
		}
	}
*/

#endif

	if (p_meta_table->num_of_free_pbs <= 1) {
		if (_callback->get_gc_priority() != FOREGROUND_GC) {
			_callback->set_gc_priority(FOREGROUND_GC);
		}
	} else {
		if (_callback->get_gc_priority() != BACKGROUND_GC) {
			_callback->set_gc_priority(BACKGROUND_GC);
#ifdef GYC_PAPER_OVERLOAD_PROTECTION
			if (_callback->get_number_of_pending_io_page_write() > 0 && !m_after_foreground_gc) {
				m_after_foreground_gc = true;
				//cout << "At " << sc_time_stamp().to_seconds();
				//cout << ", " << name();
				//cout << " set m_after_foreground_gc = on" << endl;
			}
#endif
		}
	}

	if (p_meta_table->num_of_free_pbs * 100 > p_meta_table->num_of_all_pbs * min_free_blocks_percentage) {
		return;
	}
	if (_callback->get_number_of_pending_gc_page_read() > 0) {
		return;
	}
	if (_callback->get_number_of_pending_gc_page_write() > 0) {
		return;
	}
	if (_callback->get_number_of_pending_gc_block_erase() > 0) {
		return;
	}

	if (p_meta_table->num_of_free_pbs <= 1) {
		//cout << "NEW FOREGROUND_GC" << endl;
	}

#ifdef GYC_PAPER_D_MONITOR
#if 1
	/* Find SLC victim block */
	pbn_t slc_victim_block = NULL_PBN;
	ppn_t slc_min_num_of_live_pages = PAGE_PER_BLOCK + 1;
	FOR_EACH_BODY_PB(p_meta_table, pbn, pbn_meta) {
		if (pbn_meta->mode == PB_SLC) {
			if (pbn_meta->num_of_live_pages < slc_min_num_of_live_pages) {
				slc_victim_block = pbn;
				slc_min_num_of_live_pages = pbn_meta->num_of_live_pages;
			}
		} else if (pbn_meta->mode == PB_MLC) {
		} else {
			assert(0);
		}
	} END_FOR_EACH_BODY_PB;
	//assert(slc_victim_block != NULL_PBN);

	/* Find MLC victim block */
	pbn_t mlc_victim_block = NULL_PBN;
	ppn_t mlc_min_num_of_live_pages = PAGE_PER_BLOCK + 1;
	FOR_EACH_BODY_PB(p_meta_table, pbn, pbn_meta) {
		if (pbn_meta->mode == PB_MLC) {
			if (pbn_meta->num_of_live_pages < mlc_min_num_of_live_pages) {
				mlc_victim_block = pbn;
				mlc_min_num_of_live_pages = pbn_meta->num_of_live_pages;
			}
		} else if (pbn_meta->mode == PB_SLC) {
		} else {
			assert(0);
		}
	} END_FOR_EACH_BODY_PB;
	assert(mlc_victim_block != NULL_PBN);

	long double mlc_victim_utilization =
		static_cast<long double>(p_meta_table->pb_meta_array[mlc_victim_block].num_of_live_pages)
		/ PAGE_PER_BLOCK;
	bool case1=false;
	bool case2=false;
	bool case3=false;
	bool case4=false;

	if (slc_victim_block != NULL_PBN && mlc_victim_block != NULL_PBN) {

		long double slc_victim_utilization = static_cast<long double>(p_meta_table->pb_meta_array[slc_victim_block].num_of_live_pages)
					/ (PAGE_PER_BLOCK / BIT_PER_MLC);

		if (mlc_victim_utilization <= slc_victim_utilization / BIT_PER_MLC) {
			case1 = true;
		}

		if (slc_victim_utilization / BIT_PER_MLC <= mlc_victim_utilization
				&& mlc_victim_utilization <= slc_victim_utilization) {
			case2 = true;
		}

		if (mlc_victim_utilization >= slc_victim_utilization) {
			case3 = true;
		}

		if (mlc_victim_utilization <= slc_victim_utilization / BIT_PER_MLC) {
			if (string(name()) == string("ssd.die_ftl_4")) {
				cout << "\tlucky MLC utili=" << mlc_victim_utilization << endl;
			}
			//_callback->record_big_d(mlc_victim_utilization * BIT_PER_MLC);
		} else if (slc_victim_utilization / BIT_PER_MLC < mlc_victim_utilization
				&& mlc_victim_utilization < slc_victim_utilization) {
			//_callback->record_small_d(1.0 / BIT_PER_MLC);
			//_callback->record_big_d(1.0 / BIT_PER_MLC);
			/*
			if (slc_victim_utilization < 1) {
				_callback->record_small_d((1 - mlc_victim_utilization) / (1 - slc_victim_utilization));
			} else {
				_callback->record_small_d((1 - mlc_victim_utilization) / (1.0 / PAGE_PER_BLOCK));
				_callback->record_big_d(1.0);
			}
			*/

			//cout << name() << '\t' << slc_victim_utilization << '\t' << mlc_victim_utilization << endl;
			_callback->record_big_d(slc_victim_utilization);
			_callback->record_small_d(slc_victim_utilization * (slc_victim_utilization - mlc_victim_utilization));
			//_callback->record_big_d(slc_victim_utilization);
			//_callback->record_small_d(slc_victim_utilization);
			if (string(name()) == string("ssd.die_ftl_4")) {
				cout << "\tAt t=" << sc_time_stamp().to_seconds() << ", " << name() << " in case 2: clean SLC, utili=" << slc_victim_utilization << endl;
			}
		} else if (mlc_victim_utilization >= slc_victim_utilization) {

			if (string(name()) == string("ssd.die_ftl_4")) {
				cout << "\tAt t=" << sc_time_stamp().to_seconds() << ", " << name() << " in case 3: clean SLC, utili=" << slc_victim_utilization << endl;
			}
			//_callback->record_big_d(slc_victim_utilization);
		} else {
			assert(0);
		}

	} else {
		case4 = true;
		if (string(name()) == string("ssd.die_ftl_4")) {
			cout << "At t=" << sc_time_stamp().to_seconds() << ", " << name() << " clean MLC because no SLC, utili=" << mlc_victim_utilization << endl;
		}
	}
	//_callback->record_small_d(0.0);
	//_callback->record_big_d(0.0);

/*
	if (slc_victim_block != NULL_PBN) {
		_callback->record_small_d(static_cast<long double>(p_meta_table->pb_meta_array[slc_victim_block].num_of_live_pages)
					/ (PAGE_PER_BLOCK / BIT_PER_MLC));
	}

	if (mlc_victim_block != NULL_PBN) {
		_callback->record_big_d(static_cast<long double>(p_meta_table->pb_meta_array[mlc_victim_block].num_of_live_pages)
					/ PAGE_PER_BLOCK);
	}
*/
#endif
/*
	switch (p_meta_table->pb_meta_array[victim_old_physical_block].mode) {
		case PB_SLC:
			_callback->record_small_d(1.0
					- (static_cast<long double>(p_meta_table->pb_meta_array[victim_old_physical_block].num_of_live_pages)
					/ (PAGE_PER_BLOCK / BIT_PER_MLC)));
			break;
		case PB_MLC:
			_callback->record_big_d(1.0
					- (static_cast<long double>(p_meta_table->pb_meta_array[victim_old_physical_block].num_of_live_pages)
					/ PAGE_PER_BLOCK));
			break;
		default:
			assert(0);
			break;
	}
*/

#endif

	pbn_t victim_old_physical_block = NULL_PBN;
	ppn_t min_num_of_live_pages = p_meta_table->num_of_pps_per_pb + 1;
	FOR_EACH_BODY_PB(p_meta_table, pbn, pbn_meta) {
		if (pbn_meta->num_of_live_pages < min_num_of_live_pages) {
			victim_old_physical_block = pbn;
			min_num_of_live_pages = pbn_meta->num_of_live_pages;
		}
	} END_FOR_EACH_BODY_PB;
	assert(victim_old_physical_block != NULL_PBN);

	mark_as_tail_pb(p_meta_table, victim_old_physical_block);


	FOR_EACH_PP_IN_PB(p_meta_table, victim_old_physical_block, ppn, pp_meta) {
		if (pp_meta->state == PP_LIVE) {
			assert(pp_meta->ptr_to_logical_page != NULL_LPN);
			_callback->schedule_gc_page_copy(ppn, pp_meta->ptr_to_logical_page);
		}
	} END_FOR_EACH_PP_IN_PB;

	switch (p_meta_table->pb_meta_array[victim_old_physical_block].mode) {
		case PB_SLC:
			_callback->schedule_gc_block_erase(victim_old_physical_block, true);
			break;
		case PB_MLC:
			_callback->schedule_gc_block_erase(victim_old_physical_block, false);
			break;
		default:
			assert(0);
			break;
	}
}

msec_t simple_die_ftl::_determine_write_latency(const wr_ptr_t *wr_ptr) const {
	assert(wr_ptr);
	msec_t write_latency = 0.0;
	switch (wr_ptr->mode) {
		case SLC_MODE:
			write_latency = SLC_PROGRAM_LATENCY;
			break;
		case MLC_MODE:
			write_latency = MLC_PROGRAM_LATENCY;
			break;
		default:
			assert(0);
			break;
	}
	return write_latency;
}

simple_die_ftl::simple_die_ftl(sc_module_name module_name) : sc_module(module_name) {
	wr_ptr_for_short_io_queue = NULL;
	wr_ptr_for_long_io_queue = NULL;
	wr_ptr_for_gc = NULL;

	min_free_blocks_percentage = 5;
	slc_policy = QUEUE_AWARE_SLC_POLICY;

#ifdef GYC_PAPER_OVERLOAD_PROTECTION
	m_after_foreground_gc = false;
	m_current_busy_period_slc_write_count = 0;
	m_current_busy_period_mlc_write_count = 0;
	m_current_busy_period_has_record_slc_usage = false;
	m_max_slc_usage = 0;
	m_ready_slc_usage = 0;
#endif

}

#ifdef GYC_PAPER_OVERLOAD_PROTECTION
long double simple_die_ftl::get_current_busy_period_slc_write_utilization() const {

	assert(m_current_busy_period_slc_write_count >= 0);
	assert(m_current_busy_period_mlc_write_count >= 0);

	if (m_current_busy_period_mlc_write_count == 0) {
		if (m_current_busy_period_slc_write_count == 0) {
			return 0.0;
		} else {
			return 1.0;
		}
	} else {
		return static_cast<long double>(m_current_busy_period_slc_write_count)
			/ (m_current_busy_period_slc_write_count + m_current_busy_period_mlc_write_count);
	}
	assert(0);
}

bool simple_die_ftl::should_enable_slc() const {
	return true;

	if (m_after_foreground_gc) {
		return true;
	}

	if (p_meta_table->num_of_free_pbs * 200 /* i.e. 50% free space usage */
			> p_meta_table->num_of_all_pbs * min_free_blocks_percentage) {
		return true;
	}

	long double throttle_level
		= static_cast<long double>(p_meta_table->num_of_free_pbs) * 200
		/ (p_meta_table->num_of_all_pbs * min_free_blocks_percentage);
	assert(throttle_level >= 0.0);
	assert(throttle_level <= 1.0);

	if (get_current_busy_period_slc_write_utilization()
			<= GYC_PAPER_OVERLOAD_PROTECTION__MAXIMUM_SLC_UTILIZATION * (1.0 + throttle_level)) {
		return true;
	}

	return false;
}

#endif

