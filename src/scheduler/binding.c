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
* @file binding.c
* @brief Implements load sharing rules for LPs among worker threads
* @author Alessandro Pellegrini
*/

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include <arch/atomic.h>
#include <core/core.h>
#include <core/init.h>
#include <core/timer.h>
#include <datatypes/list.h>
#include <scheduler/binding.h>
#include <scheduler/process.h>
#include <scheduler/scheduler.h>
#include <statistics/statistics.h>
#include <gvt/gvt.h>

#include <mm/numa.h>
#include <arch/thread.h>


#define REBIND_INTERVAL 10.0


struct lp_cost_id {
	double workload_factor;
	unsigned int id;
};

struct lp_cost_id *lp_cost;



/// A guard to know whether this is the first invocation or not
static __thread bool first_lp_binding = true;


static unsigned int *new_LPS_binding;
static timer rebinding_timer;

#ifdef HAVE_LP_REBINDING
static int binding_acquire_phase = 0;
static __thread int local_binding_acquire_phase = 0;

static int binding_phase = 0;
static __thread int local_binding_phase = 0;
#endif

static atomic_t worker_thread_reduction;


// When calling this function, n_prc_per_thread
// must have been updated with the new number of bound LPs.
static void rebind_LPs_to_PTs(void) {
	unsigned int i;
	unsigned int curr_pt_idx = 0;

	for(i = 0; i < n_prc_per_thread; i++) {
		LPS_bound(i)->processing_thread = Threads[tid]->PTs[curr_pt_idx]->tid;
		curr_pt_idx = (curr_pt_idx + 1) % Threads[tid]->num_PTs;
	}
}


/**
* Performs a (deterministic) block allocation between LPs and WTs
*
* @author Alessandro Pellegrini
*/
static inline void LPs_block_binding(void) {
	unsigned int binding_threads;
	unsigned int i, j;
	unsigned int buf1;
	unsigned int offset;
	unsigned int block_leftover;
	LID_t lid;

	// We determine here the number of threads used for binding, depending on the
	// actual (current) incarnation of available threads.
	if(rootsim_config.num_controllers == 0)
		binding_threads = n_cores;
	else
		binding_threads = rootsim_config.num_controllers;

	buf1 = (n_prc / binding_threads);
	block_leftover = n_prc - buf1 * binding_threads;

	if (block_leftover > 0) {
		buf1++;
	}

	n_prc_per_thread = 0;
	i = 0;
	offset = 0;

	while (i < n_prc) {
		j = 0;
		while (j < buf1) {
			if(offset == tid) {
				set_lid(lid, i);
				LPS_bound_set(n_prc_per_thread++, LPS(lid));
				LPS(lid)->worker_thread = tid;
			}
			i++;
			j++;
		}
		offset++;
		block_leftover--;
		if (block_leftover == 0) {
			buf1--;
		}
	}

	// In case we're running with the asymmetric architecture, we have
	// to rebind as well 
	if(rootsim_config.num_controllers > 0)
		rebind_LPs_to_PTs();
}

/**
* Convenience function to compare two elements of struct lp_cost_id.
* This is used for sorting the LP vector in LP_knapsack()
*
* @author Alessandro Pellegrini
*
* @param a Pointer to the first element
* @param b Pointer to the second element
*
* @return The comparison between a and b
*/
static int compare_lp_cost(const void *a, const void *b) {
	struct lp_cost_id *A = (struct lp_cost_id *)a;
	struct lp_cost_id *B = (struct lp_cost_id *)b;

	return ( B->workload_factor - A->workload_factor );
}

/**
* Implements the knapsack load sharing policy in:
*
* Roberto Vitali, Alessandro Pellegrini and Francesco Quaglia
* A Load Sharing Architecture for Optimistic Simulations on Multi-Core Machines
* In Proceedings of the 19th International Conference on High Performance Computing (HiPC)
* Pune, India, IEEE Computer Society, December 2012.
*
*
* @author Alessandro Pellegrini
*/
static inline void LP_knapsack(void) {
	unsigned int i, j;
	unsigned int binding_threads;
	double reference_knapsack = 0;
	bool assigned;
	double assignments[n_cores];

	if(!master_thread())
		return;

	// We determine here the number of threads used for binding, depending on the
	// actual (current) incarnation of available threads.
	if(rootsim_config.num_controllers == 0)
		binding_threads = n_cores;
	else
		binding_threads = rootsim_config.num_controllers;

	// Estimate the reference knapsack
	for(i = 0; i < n_prc; i++) {
		reference_knapsack += lp_cost[i].workload_factor;
	}
	reference_knapsack /= binding_threads;

	// Sort the expected times
	qsort(lp_cost, n_prc, sizeof(struct lp_cost_id) , compare_lp_cost);


	// At least one LP per thread
	bzero(assignments, sizeof(double) * binding_threads);
	j = 0;
	for(i = 0; i < binding_threads; i++) {
		assignments[j] += lp_cost[i].workload_factor;
		new_LPS_binding[i] = j;
		j++;
	}

	// Suboptimal approximation of knapsack
	for(; i < n_prc; i++) {
		assigned = false;

		for(j = 0; j < binding_threads; j++) {
			// Simulate assignment
			if(assignments[j] + lp_cost[i].workload_factor <= reference_knapsack) {
				assignments[j] += lp_cost[i].workload_factor;
				new_LPS_binding[i] = j;
				assigned = true;
				break;
			}
		}

		if(assigned == false)
			break;
	}

	// Check for leftovers
	if(i < n_prc) {
		j = 0;
		for( ; i < n_prc; i++) {
			new_LPS_binding[i] = j;
			j = (j + 1) % binding_threads;
		}
	}
}

#ifdef HAVE_LP_REBINDING

static void post_local_reduction(void) {
	unsigned int i;
	LID_t lid;
	unsigned int lid_id;
	msg_t *first_evt, *last_evt;

	for(i = 0; i < n_prc_per_thread; i++) {
		lid = LPS_bound(i)->lid;
		lid_id = lid_to_int(lid);
		first_evt = list_head(LPS(lid)->queue_in);
		last_evt = list_tail(LPS(lid)->queue_in);

		lp_cost[lid_id].id = i;
		lp_cost[lid_id].workload_factor = list_sizeof(LPS(lid)->queue_in);
		lp_cost[lid_id].workload_factor *= statistics_get_lp_data(STAT_GET_EVENT_TIME_LP, lid);
		lp_cost[lid_id].workload_factor /= ( last_evt->timestamp - first_evt->timestamp );
	}
}

static void install_binding(void) {
	unsigned int i;
	LID_t lid;

	n_prc_per_thread = 0;

	for(i = 0; i < n_prc; i++) {
		set_lid(lid, i);

		if(new_LPS_binding[i] == tid) {
			LPS_bound_set(n_prc_per_thread++, LPS(lid));

			if(tid != LPS(lid)->worker_thread) {

				#ifdef HAVE_NUMA
				numa_move_request(i, get_numa_node(running_core()));
				#endif

				LPS(lid)->worker_thread = tid;
			}
		}
	}

	// In case we're running with the asymmetric architecture, we have
	// to rebind as well 
	if(rootsim_config.num_controllers > 0)
		rebind_LPs_to_PTs();
}

#endif


/**
* This function is used to create a temporary binding between LPs and KLT.
* The first time this function is called, each worker thread sets up its data
* structures, and the performs a (deterministic) block allocation. This is
* because no runtime data is available at the time, so we "share" the load
* as the number of LPs.
* Then, successive invocations, will use the knapsack load sharing policy

* @author Alessandro Pellegrini
*/
void rebind_LPs(void) {
	unsigned int binding_threads;

	// We determine here the number of threads used for binding, depending on the
	// actual (current) incarnation of available threads.
	if(rootsim_config.num_controllers == 0)
		binding_threads = n_cores;
	else
		binding_threads = rootsim_config.num_controllers;


	if(first_lp_binding) {
		first_lp_binding = false;

		initialize_binding_blocks();

		LPs_block_binding();

		timer_start(rebinding_timer);

		if(master_thread()) {
			new_LPS_binding = rsalloc(sizeof(int) * n_prc);

			lp_cost = rsalloc(sizeof(struct lp_cost_id) * n_prc);

			atomic_set(&worker_thread_reduction, binding_threads);
		}

		return;
	}

#ifdef HAVE_LP_REBINDING
	if(master_thread()) {
		if(timer_value_seconds(rebinding_timer) >= REBIND_INTERVAL) {
			timer_restart(rebinding_timer);
			binding_phase++;
		}

		if(atomic_read(&worker_thread_reduction) == 0) {

			LP_knapsack();

			binding_acquire_phase++;
		}
	}

	if(local_binding_phase < binding_phase) {
		local_binding_phase = binding_phase;
		post_local_reduction();
		atomic_dec(&worker_thread_reduction);
	}

	if(local_binding_acquire_phase < binding_acquire_phase) {
		local_binding_acquire_phase = binding_acquire_phase;

		install_binding();

		#ifdef HAVE_PREEMPTION
		reset_min_in_transit(tid);
		#endif

		if(thread_barrier(&all_thread_barrier)) {
			atomic_set(&worker_thread_reduction, binding_threads);
		}

	}
#endif
}

