/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: process_loader.h
 *    Description: Cabeçalho do Carregador Nativo de Executáveis ELF64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 13/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 19/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _PROCESS_LOADER_H_
#define _PROCESS_LOADER_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/elf.h>

#include "process.h"
/**
 * Lê um binário executável do disco para um buffer alinhado via pool_alloc 
 * e envia-o para ser instanciado como um novo processo no Escalonador.
 */
process_t* elf_load_and_create_process(const char* path, int argc, char** argv, uint32_t cpu_id);

#endif /* _ELF_H_ */