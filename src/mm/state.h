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
* @file state.h
* @brief State Management subsystem's header
* @author Francesco Quaglia
* @author Roberto Vitali
* @author Alessandro Pellegrini
*/

#pragma once
#ifndef _STATE_MGNT_H_
#define _STATE_MGNT_H_


#include <ROOT-Sim.h>
#include <core/core.h>


/// Checkpointing interval not yet set
#define  INVALID_STATE_SAVING 		0
/// Copy State Saving checkpointing interval
#define  COPY_STATE_SAVING 		1
/// Periodic State Saving checkpointing interval
#define  PERIODIC_STATE_SAVING		2


/// Structure for LP's state
typedef struct _state_t {
	// Pointers to chain this structure to the state queue
	struct _state_t *next;
	struct _state_t *prev;

	/// Simulation time associated with the state log
	simtime_t	lvt;
	/// This is a pointer used to keep track of changes to simulation states via <SetState>()
	void		*base_pointer;
	/// A pointer to the actual log
	void		*log;
	/// This log has been taken after the execution of this event
	msg_t		*last_event;
	/// Execution state
	short unsigned int state;
} state_t;


extern void ParallelSetState(void *new_state);
extern bool LogState(LID_t);
extern void RestoreState(LID_t lid, state_t *restore_state);
extern void rollback(LID_t lid);
extern state_t *find_time_barrier(LID_t lid,  simtime_t time);
extern void clean_queue_states(LID_t lid, simtime_t new_gvt);
extern void rebuild_state(LID_t lid, state_t *state_pointer, simtime_t time);
extern void set_checkpoint_period(LID_t lid, int period);
extern void force_LP_checkpoint(LID_t lid);
extern unsigned int silent_execution(LID_t lid, void *state_buffer, msg_t *evt, msg_t *final_evt);
#endif /* _STATE_MGNT_H_ */

