/**
*                       Copyright (C) 2008-2018 HPDCS Group
*                       http://www.dis.uniroma1.it/~hpdcs
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
* @file mpi.h
* @brief MPI support module
* @author Tommaso Tocci
*/

#pragma once
#ifndef _MPI_H_
#define _MPI_H_

#ifdef HAVE_MPI


#include <stdbool.h>
#include <mpi.h>

#include <core/core.h>
#include <communication/wnd.h>


extern bool mpi_support_multithread;

#define lock_mpi() {if(!mpi_support_multithread) spin_lock(&mpi_lock);}
#define unlock_mpi() {if(!mpi_support_multithread) spin_unlock(&mpi_lock);}

// control access to MPI interface
// used only in the case MPI do not support multithread
extern spinlock_t mpi_lock;

extern MPI_Datatype msg_mpi_t;

void mpi_init(int *argc, char ***argv);
void inter_kernel_comm_init(void);
void inter_kernel_comm_finalize(void);
void mpi_finalize(void);
void synchronize_all(void);
void send_remote_msg(msg_t* msg);
bool pending_msgs(int tag);
void receive_remote_msgs(void);
bool is_request_completed(MPI_Request* req);
bool all_kernels_terminated(void);
void broadcast_termination(void);
void collect_termination(void);

#endif /* HAVE_MPI */
#endif /* _MPI_H_ */
