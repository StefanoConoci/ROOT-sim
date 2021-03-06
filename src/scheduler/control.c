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
* @file control.c
* @brief
* @author
*/


#include <stdbool.h>

#include <scheduler/scheduler.h>
#include <scheduler/process.h>
#include <core/core.h>
#include <communication/communication.h>
#include <mm/dymelor.h>
#include <mm/ecs.h>
#include <datatypes/list.h>
#include <gvt/gvt.h>

// Questa funzione serve a mandare in rollback qualcuno che mi
// aveva mandato un RENDEZVOUS_START. Viene invocata da rollback()
// e simtime è il tempo del bound, quindi il tempo dell'ultimo evento
// correttamente processato nella traiettoria speculativa.
// Ogni volta che ricevo un RENDEZVOUS_START da un altro processo
// copio il messaggio nella rendezvous_queue.
void rollback_control_message(LID_t lid, simtime_t simtime) {
	msg_t *control_antimessage;
	msg_t *msg, *msg_prev;

	if(list_empty(LPS(lid)->rendezvous_queue)) {
		return;
	}

	msg = list_tail(LPS(lid)->rendezvous_queue);
	while(msg != NULL && msg->timestamp > simtime) {

		// Control antimessage
		pack_msg(&control_antimessage, msg->receiver, msg->sender, RENDEZVOUS_ROLLBACK, msg->timestamp, msg->send_time, 0, NULL);
		control_antimessage->rendezvous_mark = msg->rendezvous_mark;
		control_antimessage->message_kind = control;
		Send(control_antimessage);
		msg_prev = list_prev(msg);
		list_delete_by_content(LPS(lid)->rendezvous_queue, msg);
		msg = msg_prev;
	}
}

// return false if the antimessage is recognized (and processed) as a control antimessage
bool anti_control_message(msg_t * msg) {
#ifndef HAVE_CROSS_STATE
	(void)msg;
#else
	msg_t *old_rendezvous ;

	if(msg->type == RENDEZVOUS_ROLLBACK) {

		LID_t lid_receiver = GidToLid(msg->receiver);
		//Check if a relative message exists
		//TODO non serve andare indietro più del tempo di rendezvous_rollback (VERO!!! Ma in quel caso devo uscire dal ciclo con old_rendezvous == NULL per cadere nell'if successivo)
		old_rendezvous = list_tail(LPS(lid_receiver)->queue_in);
		while(old_rendezvous != NULL && old_rendezvous->rendezvous_mark != msg->rendezvous_mark) {
			old_rendezvous = list_prev(old_rendezvous);
		}

		if(old_rendezvous == NULL) {
			return false;
		}

		//If this event is in the past
		if(old_rendezvous->timestamp <= lvt(lid_receiver)) {

			//Set LP->bound to the message that caused ECS
			LPS(lid_receiver)->bound = list_prev(old_rendezvous);
			while (LPS(lid_receiver)->bound != NULL && LPS(lid_receiver)->bound->timestamp >= old_rendezvous->timestamp) {
				//        if(list_prev(LPS(lid_receiver)->bound) == NULL) {
				//          break;
				//        }
				LPS(lid_receiver)->bound = list_prev(LPS(lid_receiver)->bound);
			}

			LPS(lid_receiver)->state = LP_STATE_ROLLBACK;
		}

		old_rendezvous->rendezvous_mark = 0;



		//Reset ECS information
		if(LPS(lid_receiver)->wait_on_rendezvous == msg->rendezvous_mark) {
			LPS(lid_receiver)->ECS_index = 0;
			LPS(lid_receiver)->wait_on_rendezvous = 0;
		}

		return false;
	}
#endif

	return true;
}

// return true if the control message should be reprocessed during silent exceution
bool reprocess_control_msg(msg_t *msg) {

	if(msg->type < MIN_VALUE_CONTROL) {
		return true;
	}

	return false;
}


// return true if the event must not be filtered here
bool receive_control_msg(msg_t *msg) {
	
	if(msg->type < MIN_VALUE_CONTROL || msg->type > MAX_VALUE_CONTROL) {
		return true;
	}

#ifdef  HAVE_CROSS_STATE
	LID_t lid_receiver = GidToLid(msg->receiver);
	switch(msg->type) {

		case RENDEZVOUS_START:
			return true;

		case RENDEZVOUS_GET_PAGE:
			ecs_send_pages(msg);
			break;

		case RENDEZVOUS_PAGE_WRITE_BACK:
		case RENDEZVOUS_GET_PAGE_ACK:
			if(LPS(lid_receiver)->state == LP_STATE_ROLLBACK ||
					LPS(lid_receiver)->state == LP_STATE_SILENT_EXEC) {
				break;
			}
			if(LPS(lid_receiver)->wait_on_rendezvous == msg->rendezvous_mark) {
				ecs_install_pages(msg);
				LPS(lid_receiver)->state = LP_STATE_READY_FOR_SYNCH;
			}
			break;

		case RENDEZVOUS_ACK:
			printf("Received ACK\n");
			if(LPS(lid_receiver)->state == LP_STATE_ROLLBACK ||
					LPS(lid_receiver)->state == LP_STATE_SILENT_EXEC) {
				break;
			}
			if(LPS(lid_receiver)->wait_on_rendezvous == msg->rendezvous_mark) {
				setup_ecs_on_segment(msg);
				LPS(lid_receiver)->state = LP_STATE_READY_FOR_SYNCH;
			}

			break;

		case RENDEZVOUS_UNBLOCK:
			if(LPS(lid_receiver)->state == LP_STATE_ROLLBACK ||
				LPS(lid_receiver)->state == LP_STATE_SILENT_EXEC)  {
				break;
			}

			if(LPS(lid_receiver)->wait_on_rendezvous == msg->rendezvous_mark) {
				LPS(lid_receiver)->wait_on_rendezvous = 0;
				LPS(lid_receiver)->state = LP_STATE_READY;
			}
			
			current_lp = GidToLid(msg->receiver);
			current_lvt = msg->timestamp;
			force_LP_checkpoint(current_lp);
			LogState(current_lp);
			current_lvt = INFTY;
			current_lp = idle_process;

			break;

		case RENDEZVOUS_ROLLBACK:
			return true;

		default:
			rootsim_error(true, "Trying to handle a control message which is meaningless at receive time: %d\n", msg->type);

	}
#endif

	return false;
}


// return true if must be passed to the LP
bool process_control_msg(msg_t *msg) {

#ifdef HAVE_CROSS_STATE
	msg_t *control_msg;
#endif

	if(msg->type < MIN_VALUE_CONTROL || msg->type > MAX_VALUE_CONTROL) {
		return true;
	}

#ifdef HAVE_CROSS_STATE
	LID_t lid_receiver = GidToLid(msg->receiver);
	msg_t *copy; 
	switch(msg->type) {

		case RENDEZVOUS_START:
			copy = rsalloc(sizeof(msg_t));
			*copy = *msg;	
			list_insert(LPS(lid_receiver)->rendezvous_queue, timestamp, copy);
			// Place this into input queue
			LPS(lid_receiver)->wait_on_rendezvous = msg->rendezvous_mark;

			LPS(lid_receiver)->state = LP_STATE_WAIT_FOR_UNBLOCK;
			
			pack_msg(&control_msg, msg->receiver, msg->sender, RENDEZVOUS_ACK, msg->timestamp, msg->timestamp, 0, NULL);
			control_msg->message_kind = positive;
			control_msg->rendezvous_mark = msg->rendezvous_mark;
			Send(control_msg);

			break;

		default:
			rootsim_error(true, "Trying to handle a control message which is meaningless at schedule time: %d\n", msg->type);

	}
#endif

	return false;
}
