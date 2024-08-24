#include "tonc.h"
#include "audio_engine_internal.h"

void audioTimer1ISR(){
	//disable DMA
	REG_DMA1CNT = 0;
	REG_DMA2CNT = 0;
	
	//increase the audio counter
	if(audioProgress == 2){
		audioTimer++;
		audioProgress = 0;
	}
	
	//if we are in an odd frame
	if(audioTimer & 1){
		//set the DMA to play from buffer 2
		REG_DMA1SAD = (u32)soundBuffer2;
		REG_DMA2SAD = (u32)soundBuffer2 + MAX_SAMPLES_IN_ONE_FRAME;
	}
	else{
		//otherwise, play from buffer 1
		REG_DMA1SAD = (u32)soundBuffer1;
		REG_DMA2SAD = (u32)soundBuffer1 + MAX_SAMPLES_IN_ONE_FRAME;
	}
	
	//reenable DMA
	REG_DMA1CNT = DMA_COUNT(4) | DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
	REG_DMA2CNT = DMA_COUNT(4) | DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
	
	//if audioError is negative, add the period
	if(audioError < 0){
		audioError += SYNC_PERIOD_FRAMES;
	}
	
	//subtract the frame error
	audioError -= SYNC_ERROR_PER_FRAME;
	
	//set the timer reload value for the next frame's timer
	u32 samplesThisFrame;
	if(audioError < 0){
		samplesThisFrame = MAX_SAMPLES_IN_ONE_FRAME;
	}
	else{
		samplesThisFrame = MAX_SAMPLES_IN_ONE_FRAME - 16;
	}
	REG_TM1D = 0x10000 - samplesThisFrame;
}