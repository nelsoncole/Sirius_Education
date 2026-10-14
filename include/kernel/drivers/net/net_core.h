/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: net_core.h
 *    Description: Módulo Driver Dinâmico (LKM) para as placas de rede.
 * 
 *         Author: Nelson Cole
 *   Created Date: 10/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 10/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _NET_CORE_H_
#define _NET_CORE_H_

// Definição do evento
typedef enum {
    NET_EVENT_NONE,
    NET_EVENT_LINK_UP,
    NET_EVENT_LINK_DOWN
} net_event_t;

void kmod_net_init(const char* root);

#endif