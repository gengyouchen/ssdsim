#ifndef __SSD_TYPEDEF_H__
#define __SSD_TYPEDEF_H__

#include "gyc_ftl_typedef.h"

#define BYTE_PER_COMMAND 1
#define BYTE_PER_ADDRESS 5
#define BYTE_PER_STATUS 1

/* Assume each die is a typical 8GB MLC die using 8KB as their page size */
/*     and we use its clustered page size to support multi-plane command */
#define PLANE_PER_DIE 2
#define BYTE_PER_SECTOR 512
#define SECTOR_PER_PAGE (16 * PLANE_PER_DIE)
#define PAGE_PER_BLOCK 516
#define BLOCK_PER_DIE (2048 / PLANE_PER_DIE)

#define HOST_BUS_IO_CYCLE ((msec_t) 0.00000025)
#define HOST_BUS_HEADER_SIZE 16

#define SSD_QUEUE_DEPTH 512
#define SSD_CHANNEL_NUMBER 32
#define SSD_OVER_PROVISIONING_PERCENTAGE 15

#define PHYSICAL_CAPACITY_IN_PAGE_PER_DIE (PAGE_PER_BLOCK * BLOCK_PER_DIE)
#define USER_CAPACITY_IN_PAGE_PER_DIE (PHYSICAL_CAPACITY_IN_PAGE_PER_DIE * 100 / (100 + SSD_OVER_PROVISIONING_PERCENTAGE))
#define HIDDEN_CAPACITY_IN_PAGE_PER_DIE (PHYSICAL_CAPACITY_IN_PAGE_PER_DIE - USER_CAPACITY_IN_PAGE_PER_DIE)

/* Assume bandwidth of each channel using ONFi 2.1 interface (mode 5), */
/*     which is equal to 200Mbyte/s or 5ns/byte in terms of I/O cycle  */
#define CHANNEL_IO_CYCLE ((msec_t) 0.000005)

/* Assume each 8GB MLC die has the read latency, the page program latnecy in SLC / MLC, and the erase latency */
/*     characterized in Wear Unleveling: Improving NAND Flash Lifetime by Balancing Page Endurance (FAST '14) */
#define MLC_READ_LATENCY ((msec_t) 0.220)
#define MLC_PROGRAM_LATENCY ((msec_t) (0.800 + 3.400 + 7.600) / 3)
#define MLC_ERASE_LATENCY ((msec_t) 5.000)
#define SLC_PROGRAM_LATENCY ((msec_t) 0.400)

#define CHANNEL_READ_INPUT_LATENCY (CHANNEL_IO_CYCLE * (BYTE_PER_COMMAND + BYTE_PER_ADDRESS))
#define CHANNEL_READ_OUTPUT_LATENCY (CHANNEL_IO_CYCLE * (BYTE_PER_STATUS + BYTE_PER_SECTOR * SECTOR_PER_PAGE))
#define CHANNEL_PROGRAM_INPUT_LATENCY (CHANNEL_IO_CYCLE * (BYTE_PER_COMMAND + BYTE_PER_ADDRESS + BYTE_PER_SECTOR * SECTOR_PER_PAGE))
#define CHANNEL_PROGRAM_OUTPUT_LATENCY (CHANNEL_IO_CYCLE * BYTE_PER_STATUS)
#define CHANNEL_ERASE_INPUT_LATENCY (CHANNEL_IO_CYCLE * (BYTE_PER_COMMAND + BYTE_PER_ADDRESS))
#define CHANNEL_ERASE_OUTPUT_LATENCY (CHANNEL_IO_CYCLE * BYTE_PER_STATUS)

#define ADD_DUMMY_TO_INITIAL_LAYOUT

#define DEADLINE_AWARE_SLC_POLICY
#define DEADLINE_AWARE_SLC_POLICY_DEADLINE ((msec_t) 100.0)
#define DEADLINE_AWARE_SLC_POLICY_NO_DEADLINE ((msec_t) -1)

#define DEADLINE_AWARE_SLC_POLICY_SELF_TUNING
#define DEADLINE_AWARE_SLC_POLICY_SELF_TUNING__ERROR_MARGIN_PERCENTAGE 10
#define DEADLINE_AWARE_SLC_POLICY_SELF_TUNING__LATENCY_GRANULARITY 0.001

#define DEADLINE_AWARE_SLC_POLICY_DEADLINE__MAX (DEADLINE_AWARE_SLC_POLICY_DEADLINE)
#define DEADLINE_AWARE_SLC_POLICY_DEADLINE__MIN (DEADLINE_AWARE_SLC_POLICY_DEADLINE * (100 - DEADLINE_AWARE_SLC_POLICY_SELF_TUNING__ERROR_MARGIN_PERCENTAGE) / 100)

//#define GYC_PAPER_D_MONITOR

#define GYC_PAPER_REQ_LATENCY_PLOT
#define GYC_PAPER_REQ_LATENCY_PLOT__TIME_INTERVAL ((msec_t) 1000 * 60 * 15)

#define GYC_PAPER_REQ_RATE_PLOT
#define GYC_PAPER_REQ_RATE_PLOT__SAMPLE_INTERVAL ((msec_t) 1000)
#define GYC_PAPER_REQ_RATE_PLOT__TIME_INTERVAL ((msec_t) 1000 * 60 * 15)

//#define GYC_PAPER_SLC_THROTTLING
#define GYC_PAPER_SLC_THROTTLING__TARGETED_SLC_UTILIZATION 0.10
#define GYC_PAPER_SLC_THROTTLING__THROTTLING_RATE_MULTIPLIER_FOR_ZERO_SLC_UTILIZATION 2
#define GYC_PAPER_SLC_THROTTLING__THROTTLING_RATE_INITIAL_VALUE_FOR_ZERO_SLC_UTILIZATION 0.3

//#define GYC_PAPER_OVERLOAD_PROTECTION
#define GYC_PAPER_OVERLOAD_PROTECTION__MAXIMUM_SLC_UTILIZATION 0.5

//#define IO_SCHEDULER__RECORD_READ_LATENCY
//#define IO_SCHEDULER__RECORD_WRITE_LATENCY
#define IO_SCHEDULER__RECORD_ALL_LATENCY

#define IO_SCHEDULER__READ_OVER_WRITE

typedef long double msec_t;
#define NULL_MSEC ((msec_t) -1)

typedef long long int tran_id_t;

typedef enum tran_type {
	READ_TRAN,
	WRITE_TRAN
} tran_type_t;

typedef struct tran {
	tran_id_t id;
	tran_type_t type;
	lpn_t lpn;
	lpn_t size;
	lpn_t remain_size;
	msec_t arrival_time;
} tran_t;

typedef enum op_type {
	READ_OP,
	PROGRAM_OP,
	ERASE_OP
} op_type_t;

typedef struct op {
	tran_id_t parent_id;
	op_type_t type;
	lpn_t lpn;
	ppn_t ppn;
	pbn_t pbn;
#ifdef DEADLINE_AWARE_SLC_POLICY
	msec_t deadline;
#endif
} op_t;

#define NO_PARENT ((tran_id_t) -1)

typedef enum {
	FOREGROUND_GC,
	BACKGROUND_GC
} gc_priority_t;

typedef enum {
	NO_SLC_POLICY,
	QUEUE_AWARE_SLC_POLICY,
	ALWAYS_SLC_POLICY
} slc_policy_t;

#endif

