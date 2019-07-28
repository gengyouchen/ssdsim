#ifndef __TRACE_DEV_INFO_H__
#define __TRACE_DEV_INFO_H__

#include <cassert>

#include "ssd_typedef.h"

class trace_dev_info {
	private:
		lpn_t m_max_sector_addr;
	public:
		trace_dev_info() : m_max_sector_addr(0) {
		}
		void record_request(lpn_t addr_in_sectors, lpn_t len_in_sectors) {
			assert(addr_in_sectors >= 0);
			assert(len_in_sectors > 0);
			lpn_t last_addr_in_sectors = addr_in_sectors + len_in_sectors - 1;
			assert(last_addr_in_sectors >= 0);
			if (last_addr_in_sectors > m_max_sector_addr) {
				m_max_sector_addr = last_addr_in_sectors;
			}
		}
		lpn_t get_size_in_sectors() const {
			return m_max_sector_addr + 1;
		}
		void set_size_in_sectors(lpn_t size_in_sectors) {
			assert(size_in_sectors > 0);
			m_max_sector_addr = size_in_sectors - 1;
		}
};

#endif

