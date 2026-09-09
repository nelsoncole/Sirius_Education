/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ipc.h
 *    Description: Interface pública da API de Utilizador (UAPI) para IPC.
 *                 Define os códigos de sinais POSIX, flags de controlo de SHM
 *                 e os protótipos das chamadas de sistema expostas ao User Space.
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _UAPI_IPC_H_
#define _UAPI_IPC_H_

#include <kernel/kernel/ipc/ipc.h>

int shm_get(int key, int flags);

void* shm_at(int shmid);

int shm_dt(int shmid);

int kill(int pid, int sig);

#endif