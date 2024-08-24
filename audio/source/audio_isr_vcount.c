#include "tonc.h"
#include "audio_engine_internal.h"

void audioVcountISR(){
	//disable vcount interrupts
	REG_DISPSTAT = REG_DISPSTAT & ~DSTAT_VCT_IRQ;
	irq_delete(II_VCOUNT);
	
	//enable the timer and timer interrupts
	REG_TM0CNT = TM_FREQ_1 | TM_ENABLE;
}