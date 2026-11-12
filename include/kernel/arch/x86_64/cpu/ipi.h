#ifndef _IPI_H_
#define _IPI_H_

#define IPI_FLUSH_TLB 0xF0

void handle_ipi_flush_tlb(void);
void smp_flush_tlb_all(void);


#endif
