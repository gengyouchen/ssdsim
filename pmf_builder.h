#ifndef __PMF_BUILDER_H__
#define __PMF_BUILDER_H__

#include <cassert>
#include <map>
#include <fstream>
#include <string>
#include <utility>

template<typename value_t>
class pmf_builder {
	public:
		typedef std::pair<value_t, unsigned long long> value_counter_t;
		typedef std::map<value_t, unsigned long long> value_count_map_t;

		pmf_builder();
		void record_value(const value_t& value);

		unsigned long long count() const;
		unsigned long long count_with_range(value_t min, value_t max) const;
		value_t sum() const;
		value_t avg() const;
		value_t max() const;
		value_t min() const;
		value_t percentile(long double rank) const;
		void reset();

	private:
		value_count_map_t value_count_map;
		value_t sum_of_all_values;
		unsigned long long sum_of_all_value_counts;
};

template<typename value_t>
pmf_builder<value_t>::pmf_builder() {
	sum_of_all_values = 0;
	sum_of_all_value_counts = 0;
}

template<typename value_t>
void pmf_builder<value_t>::record_value(const value_t& value) {
	value_count_map.insert(value_counter_t(value, 0)).first->second++;
	sum_of_all_values += value;
	sum_of_all_value_counts++;
}

template<typename value_t>
unsigned long long pmf_builder<value_t>::count() const {
	return sum_of_all_value_counts;
}

template<typename value_t>
value_t pmf_builder<value_t>::sum() const {
	return sum_of_all_values;
}

template<typename value_t>
value_t pmf_builder<value_t>::avg() const {
	if (sum_of_all_value_counts > 0) {
		return sum_of_all_values / sum_of_all_value_counts;
	} else {
		return 0;
	}
}

template<typename value_t>
value_t pmf_builder<value_t>::max() const {
	if (!value_count_map.empty()) {
		return value_count_map.rbegin()->first;
	} else {
		return 0;
	}
}

template<typename value_t>
value_t pmf_builder<value_t>::min() const {
	if (!value_count_map.empty()) {
		return value_count_map.begin()->first;
	} else {
		return 0;
	}
}

template<typename value_t>
value_t pmf_builder<value_t>::percentile(long double rank) const {
	assert(rank >= 0 && rank <= 1);
	if (!value_count_map.empty()) {
		unsigned long long sum_of_all_below_value_counts = 0;
		for (typename value_count_map_t::const_iterator i = value_count_map.begin(); i != value_count_map.end(); ++i) {
			sum_of_all_below_value_counts += i->second;
			if (sum_of_all_below_value_counts >= sum_of_all_value_counts * rank) {
				return i->first;
			}
		}
	} else {
		return 0;
	}
	assert(0);
}

template<typename value_t>
void pmf_builder<value_t>::reset() {
	sum_of_all_values = 0;
	sum_of_all_value_counts = 0;
	value_count_map.clear();
}

template<typename value_t>
unsigned long long pmf_builder<value_t>::count_with_range(value_t min, value_t max) const {
	unsigned long long sum_of_all_values_within_range = 0;
	for (typename value_count_map_t::const_iterator i = value_count_map.begin(); i != value_count_map.end(); ++i) {
		if (i->first >= min && i->first <= max) {
			sum_of_all_values_within_range += i->second;
		}
	}
	return sum_of_all_values_within_range;
}

#endif

