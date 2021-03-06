/**
*			Copyright (C) 2008-2018 HPDCS Group
*			http://www.dis.uniroma1.it/~hpdcs
*
*
* This file is part of ROOT-Sim (ROme OpTimistic Simulator).
*
* ROOT-Sim is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; only version 3 of the License applies.
*
* ROOT-Sim is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* ROOT-Sim; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
* @file scheduler.h
* @brief Scheduling Subsystem main header file. Scheduler-specific data structures
*        and defines should be placed into specific headers, not here.
* @author Francesco Quaglia
* @author Alessandro Pellegrini
* @author Roberto Vitali
*/

#pragma once
#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <ROOT-Sim.h>
#include <core/core.h>
#include <queues/queues.h>
#include <communication/communication.h>
#include <scheduler/stf.h>
#include <arch/ult.h>
#include <scheduler/process.h>


/// This macro defines after how many idle cycles the simulation is stopped
#define MAX_CONSECUTIVE_IDLE_CYCLES	1000


/// Smallest Timestamp Scheduler's Code
#define SMALLEST_TIMESTAMP_FIRST	0

/* This macro defines a threshold in the percentage of utilization
* of the port in asymmetric execution. Utilization rates higher than 
* this value are considered inadequate as they might lead to an empty port 
* the PTs. Consequently, if utilization is higher than this value,
* the port size is increased.
*
* Author: Stefano Conoci
*/
#define UPPER_PORT_THRESHOLD 0.9


/* This macro defines a threshold in the percentage of utilization
* of the port in asymmetric execution. Utilization rates lower than 
* this value are considered inadequate as they might lead to an excessive 
* amount of unnecessary speculation . Consequently, if utilization  is
* lower than this value, the port size is decreased.
*
* Author: Stefano Conoci
*/ 
#define LOWER_PORT_THRESHOLD 0.5

/* Functions invoked by other modules */
extern void scheduler_init(void);
extern void scheduler_fini(void);
extern void schedule(void);
extern void asym_schedule(void);
extern void asym_process(void);
extern void initialize_LP(LID_t lp);
extern void initialize_worker_thread(void);
extern void activate_LP(LID_t lp, simtime_t lvt, void *evt, void *state);



extern bool receive_control_msg(msg_t *);
extern bool process_control_msg(msg_t *);
extern bool reprocess_control_msg(msg_t *);
extern void rollback_control_message(LID_t, simtime_t);
extern bool anti_control_message(msg_t * msg);

#ifdef HAVE_PREEMPTION
extern void preempt_init(void);
extern void preempt_fini(void);
extern void reset_min_in_transit(unsigned int);
extern void update_min_in_transit(unsigned int, simtime_t);
void enable_preemption(void);
void disable_preemption(void);
#endif


extern __thread LID_t current_lp;
extern __thread simtime_t current_lvt;
extern __thread msg_t *current_evt;
extern __thread void *current_state;
extern __thread unsigned int n_prc_per_thread;


#ifdef HAVE_PREEMPTION
extern __thread volatile bool platform_mode;
#define switch_to_platform_mode() do {\
				   if(LPS[current_lp]->state != LP_STATE_SILENT_EXEC) {\
					platform_mode = true;\
				   }\
				  } while(0)

#define switch_to_application_mode() platform_mode = false
#else
#define switch_to_platform_mode() {}
#define switch_to_application_mode() {}
#endif /* HAVE_PREEMPTION */

#endif
