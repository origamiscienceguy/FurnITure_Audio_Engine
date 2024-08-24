#include "audio_engine_internal.h"

//global variables
s8 soundBuffer1[MAX_SAMPLES_IN_ONE_FRAME * 2] ALIGN(4) EWRAM_DATA;
s8 soundBuffer2[MAX_SAMPLES_IN_ONE_FRAME * 2] ALIGN(4) EWRAM_DATA;
s8 audioError;
u8 audioTimer;
ChannelData channelsMixData[MAX_DMA_CHANNELS] EWRAM_DATA;
CurrentSoundSettings activeSounds[MAX_SOUNDS_IN_QUEUE] EWRAM_DATA;
CurrentChannelSettings *allocatedMixChannels[MAX_DMA_CHANNELS] EWRAM_DATA;
u8 audioProgress;

void audioInitialize(){
	//reset the timer registers
	REG_TM0CNT = 0;
	REG_TM0D = 0x10000 - CYCLES_PER_SAMPLE;
	REG_TM1CNT = 0;
	REG_TM1D = 0x10000 - MAX_SAMPLES_IN_ONE_FRAME + 16;
	REG_TM1CNT = TM_CASCADE | TM_IRQ | TM_ENABLE;

	//reset some of the sound registers
	REG_SNDSTAT = SSTAT_ENABLE;
	REG_SNDDSCNT = SDS_DMG100 | SDS_A100 | SDS_B100 | SDS_AR | SDS_ATMR0 | SDS_ARESET | SDS_BL | SDS_BTMR0 | SDS_BRESET | SDS_DMG100;
	REG_SNDDMGCNT = SDMG_LSQR1 | SDMG_LSQR2 | SDMG_LWAVE | SDMG_LNOISE | SDMG_RSQR1 | SDMG_RSQR2 | SDMG_RWAVE | SDMG_RNOISE | SDMG_LVOL(7) | SDMG_RVOL(7);
	REG_SNDBIAS = 0x200;
	
	//fill the sound FIFO with 0
	REG_FIFO_A = 0;
	REG_FIFO_A = 0;
	REG_FIFO_A = 0;
	REG_FIFO_A = 0;
	REG_FIFO_B = 0;
	REG_FIFO_B = 0;
	REG_FIFO_B = 0;
	REG_FIFO_B = 0;
	
	//reset the DMA registers
	REG_DMA1CNT = 0;
	REG_DMA1SAD = (u32)soundBuffer1;
	REG_DMA1DAD = (u32)&REG_FIFO_A;
	REG_DMA1CNT = DMA_COUNT(4) | DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
	REG_DMA2CNT = 0;
	REG_DMA2DAD = (u32)&REG_FIFO_B;
	REG_DMA2SAD = (u32)soundBuffer1 + (MAX_SAMPLES_IN_ONE_FRAME >> 2);
	REG_DMA2CNT = DMA_COUNT(4) | DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
	
	//set the correct global variables
	audioError = SYNC_PERIOD_FRAMES - 1; //127
	audioTimer = 0;
	
	//clear soundBuffer1 and 2, both L and R;
	for(u32 i = 0; i < (MAX_SAMPLES_IN_ONE_FRAME >> 1); i++){
		((u32 *)soundBuffer1)[i] = 0;
		((u32 *)soundBuffer2)[i] = 0;
	}
	
	//initialize all channels
	for(u32 i = 0; i < MAX_DMA_CHANNELS; i++){
		channelsMixData[i].samplePtr = sampleList[0];
		channelsMixData[i].sampleIndex = 0;
		channelsMixData[i].state = 0;
		channelsMixData[i].leftVolume = 0x0;
		channelsMixData[i].rightVolume = 0x0;
		channelsMixData[i].pitch = 0x0;
		allocatedMixChannels[i] = 0;
	}
	
	//initialize all assets
	for(u32 i = 0; i < MAX_SOUNDS_IN_QUEUE; i++){
		activeSounds[i].enabled = 0;
		for(u32 j = 0; j < MAX_DMA_CHANNELS; j++){
			activeSounds[i].channelSettings[j].channelIndex = 0xff;
		}
	}
	
	//enable vcount interrupts so that the audio iterrupt can be synced to this point
	REG_DISPSTAT = REG_DISPSTAT & ~DSTAT_VCT_MASK;
	REG_DISPSTAT = REG_DISPSTAT | (DSTAT_VCT_IRQ | DSTAT_VCT(158));
}

//starts a new asset playing, based on the id into the audio_list asset list.
u8 playNewSound(u16 assetID){
	
	u8 lowestPriority = 0xff;
	u8 lowestIndex = 0;
	u32 soundIndex = 0;
	
	//find an open assetID slot, or the one with the lowest priority.
	for(;soundIndex < MAX_SOUNDS_IN_QUEUE; soundIndex++){
		//if we find an open asset slot, select it.
		if(activeSounds[soundIndex].enabled == 0){
			lowestPriority = 0;
			lowestIndex = soundIndex;
			break;
		}
		//otherwise, record the lowest priority slot.
		if(activeSounds[soundIndex].priority <= lowestPriority){
			lowestIndex = soundIndex;
			lowestPriority = activeSounds[soundIndex].priority;
		}
	}
	
	//check if this asset has a higher priority than the lowest-priority asset currently playing, exit

	if(*soundList[assetID]->assetPriority < lowestPriority){
		return 0xff;
	}
	
	soundIndex = lowestIndex;
	
	if(activeSounds[soundIndex].enabled != 0){
		endSound(soundIndex);
	}
	
	CurrentSoundSettings *soundPointer = &activeSounds[soundIndex];
	
	soundPointer->asset = soundList[assetID];
	soundPointer->assetID = assetID;
	soundPointer->rowNum = 0;
	soundPointer->patternOffset = 0;
	soundPointer->orderIndex = 0;
	soundPointer->enabled = 1;
	soundPointer->currentTickSpeed = soundList[assetID]->initTickSpeed;
	soundPointer->tickCounter = 0;
	soundPointer->currentTempo = soundList[assetID]->initTempo;
	soundPointer->leftoverSamples = 0;
	soundPointer->globalVolume = soundList[assetID]->initGlobalVol;
	soundPointer->globalEffects.A = 0xff;
	soundPointer->globalEffects.B = 0xff;
	soundPointer->globalEffects.C = 0xff;
	soundPointer->globalEffects.SE = 0xff;
	soundPointer->priority = *soundList[assetID]->assetPriority;
	soundPointer->mixVolume = 0xff;
	
	for(u32 channel = 0; channel < MAX_DMA_CHANNELS; channel++){
		soundPointer->channelSettings[channel].noteState = NO_NOTE;
		soundPointer->channelSettings[channel].effectMemory.SBx = 0xff;
		soundPointer->channelSettings[channel].channelVolume = soundList[assetID]->initChannelVol[channel];
		soundPointer->channelSettings[channel].instrumentPointer = &soundList[0]->instruments[0]; //pointer to the instrument struct that is currently playing
		soundPointer->channelSettings[channel].samplePointer = sampleList[0]; //pointer to the sample that is currently playing
		soundPointer->channelSettings[channel].pitchModifier = 0x1000;
		soundPointer->channelSettings[channel].channelPan = soundList[assetID]->initChannelPan[channel];
		soundPointer->channelSettings[channel].vibrato.waveformType = 0;
		soundPointer->channelSettings[channel].tremolo.waveformType = 0;
		soundPointer->channelSettings[channel].panbrello.waveformType = 0;
		soundPointer->channelSettings[channel].autoVibrato.waveformType = 0;
		soundPointer->channelSettings[channel].priority = soundList[assetID]->assetChannelPriority[channel];
		soundPointer->channelSettings[channel].channelIndex = 0xff;
	}
	
	//allocate this asset's channels among the available mixing channels
	allocateChannels();
	
	return soundIndex;
}

u8 endSound(u8 soundIndex){
	activeSounds[soundIndex].enabled = 0;
	for(u32 i = 0; i < MAX_DMA_CHANNELS; i++){
		if(activeSounds[soundIndex].channelSettings[i].channelIndex != 0xff){
			channelsMixData[activeSounds[soundIndex].channelSettings[i].channelIndex].state = 0;
			allocatedMixChannels[activeSounds[soundIndex].channelSettings[i].channelIndex] = 0;
			activeSounds[soundIndex].channelSettings[i].channelIndex = 0xff;
		}
	}
	
	//reallocate channels now that this asset is no longer using them
	allocateChannels();
	
	//check if there are still assets playing
	u32 numActiveSounds = 0;
	//check if there is at least one asset playing right now
	for(u32 slotIndex = 0; slotIndex < MAX_SOUNDS_IN_QUEUE; slotIndex++){
		if(activeSounds[slotIndex].enabled == 1){
			numActiveSounds++;
		}
	}
	//if nothing is playing, fill the buffers with 0
	if(numActiveSounds == 0){
		for(u32 i = 0; i < (MAX_SAMPLES_IN_ONE_FRAME / 2); i++){
			((u32 *)soundBuffer1)[i] = 0;
			((u32 *)soundBuffer2)[i] = 0;
		}
	}
	
	return numActiveSounds;
}

u8 endAllSound(){
	for (int i = 0; i < MAX_SOUNDS_IN_QUEUE; i++){
		endSound(i);
	}
	return 0;
}

void pauseAsset(u8 soundIndex){
	//pause the asset
	if(activeSounds[soundIndex].enabled == 1){
		activeSounds[soundIndex].enabled = 2;
	}
	
	for(u32 i = 0; i < MAX_DMA_CHANNELS; i++){
		if(activeSounds[soundIndex].channelSettings[i].channelIndex != 0xff){
			ChannelData *mixDataPointer = &channelsMixData[activeSounds[soundIndex].channelSettings[i].channelIndex];
			mixDataPointer->padding = mixDataPointer->state;
			mixDataPointer->state = 0;
		}
	}
	
	u32 numActiveSounds = 0;
	//check if there is at least one asset playing right now
	for(u32 slotIndex = 0; slotIndex < MAX_SOUNDS_IN_QUEUE; slotIndex++){
		if(activeSounds[slotIndex].enabled == 1){
			numActiveSounds++;
		}
	}
	//if nothing is playing, fill the buffers with 0
	if(numActiveSounds == 0){
		for(u32 i = 0; i < (MAX_SAMPLES_IN_ONE_FRAME / 2); i++){
			((u32 *)soundBuffer1)[i] = 0;
			((u32 *)soundBuffer2)[i] = 0;
		}
	}
}

void resumeAsset(u8 soundIndex){
	//resume the asset
	if(activeSounds[soundIndex].enabled == 2){
		activeSounds[soundIndex].enabled = 1;
	}
	
	for(u32 i = 0; i < MAX_DMA_CHANNELS; i++){
		if(activeSounds[soundIndex].channelSettings[i].channelIndex != 0xff){
			ChannelData *mixDataPointer = &channelsMixData[activeSounds[soundIndex].channelSettings[i].channelIndex];
			mixDataPointer->state = mixDataPointer->padding;
			mixDataPointer->padding = 0;
		}
	}
}

void setAssetVolume(u8 soundIndex, u8 volume){
	activeSounds[soundIndex].mixVolume = volume;
}

u8 getAssetDefaultVolume(u8 soundIndex){
	return _sndAssetDefaultVol[soundIndex];
}

u8 getAssetMixVolume(u8 soundIndex){
	return activeSounds[soundIndex].mixVolume;
}

void volumeSlideAsset(u8 assetIndex, u8 volumePerTick, u8 finalVolume){
	activeSounds[assetIndex].volumeSlideAmount = volumePerTick;
	activeSounds[assetIndex].finalVolume = finalVolume;
}

void syncAsset(u8 soundIndex1, u8 soundIndex2){
	CurrentSoundSettings *assetPointer1 = &activeSounds[soundIndex1];
	activeSounds[soundIndex1].orderIndex = activeSounds[soundIndex2].orderIndex;
	activeSounds[soundIndex1].rowNum = activeSounds[soundIndex2].rowNum;
	
	PatternData *patternPtr = &assetPointer1->asset->patterns[assetPointer1->asset->orders[assetPointer1->orderIndex]];
	assetPointer1->patternOffset = 0;
	for(u32 i = 0; i < assetPointer1->rowNum; i++){
		nextRow(patternPtr, &assetPointer1->patternOffset, assetPointer1);
		for(u8 channel = 0; channel < MAX_DMA_CHANNELS; channel++){
			CurrentChannelSettings *channelPointer = &assetPointer1->channelSettings[channel];
			channelPointer->previousBasicSettings.volume = channelPointer->currentBasicSettings.volume;
			channelPointer->previousBasicSettings.note = channelPointer->currentBasicSettings.note;
			channelPointer->previousBasicSettings.instrument = channelPointer->currentBasicSettings.instrument;
		}
	}
	for(u8 channel = 0; channel < MAX_DMA_CHANNELS; channel++){
		assetPointer1->channelSettings[channel].noteState = NO_NOTE;
		assetPointer1->channelSettings[channel].triggerState = NO_TRIGGER;
		assetPointer1->channelSettings[channel].volumeState = NO_VOLUME;
		assetPointer1->channelSettings[channel].effectState = NO_EFFECT;
		assetPointer1->channelSettings[channel].pitchState = NO_PITCH;
	}
	
	activeSounds[soundIndex1].tickCounter = activeSounds[soundIndex2].tickCounter;
	activeSounds[soundIndex1].leftoverSamples = activeSounds[soundIndex2].leftoverSamples;
}

u8 isSoundPlaying(u16 assetName, u8 assetIndex){
	if((activeSounds[assetIndex].assetID == assetName) && (activeSounds[assetIndex].enabled != 0)){
		return 1;
	}
	else{
		return 0;
	}
}

void allocateChannels(){
	u32 unallocatedMixNum = 0;
	u8 unallocatedMix[MAX_DMA_CHANNELS];
	u32 unallocatedAssetNum = 0;
	CurrentChannelSettings *unallocatedSound[MAX_SOUNDS_IN_QUEUE * MAX_DMA_CHANNELS];
	
	//count the number of unallocated mix channels
	for(u32 i = 0; i < MAX_DMA_CHANNELS; i++){
		if(allocatedMixChannels[i] == 0){
			unallocatedMix[unallocatedMixNum] = i;
			unallocatedMixNum++;
		}
	}
	
	//count the number of unallocated asset channels
	for(u32 j = 0; j < MAX_SOUNDS_IN_QUEUE; j++){
		if(activeSounds[j].enabled == 0){
			continue;
		}
		for(u32 i = 0; i < MAX_DMA_CHANNELS; i++){
			if(activeSounds[j].channelSettings[i].channelIndex == 0xff){
				unallocatedSound[unallocatedAssetNum] = &activeSounds[j].channelSettings[i];
				unallocatedAssetNum++;
			}
		}
	}
	
	//allocate the highest-priority unallocated asset channels among the unallocated mix channels
	while(unallocatedMixNum != 0){
		if(unallocatedAssetNum == 0){
			return;
		}
		
		u32 highestPriorityAsset = 0xff;
		u32 highestAssetPriority = 0;
		for(u32 i = 0; i < unallocatedAssetNum; i++){
			if(unallocatedSound[i]->priority > highestAssetPriority){
				highestAssetPriority = unallocatedSound[i]->priority;
				highestPriorityAsset = i;
			}
		}
		if(highestPriorityAsset == 0xff){
			return;
		}
		allocatedMixChannels[unallocatedMix[unallocatedMixNum - 1]] = unallocatedSound[highestPriorityAsset];
		unallocatedSound[highestPriorityAsset]->channelIndex = unallocatedMix[unallocatedMixNum - 1];
		unallocatedSound[highestPriorityAsset] = unallocatedSound[unallocatedAssetNum - 1];
		channelsMixData[unallocatedMix[unallocatedMixNum - 1]].state = 0;
		allocatedMixChannels[unallocatedMix[unallocatedMixNum - 1]]->noteState = NO_NOTE;
		allocatedMixChannels[unallocatedMix[unallocatedMixNum - 1]]->triggerState = CANCEL_TRIGGER;
		unallocatedMixNum--;
		unallocatedAssetNum--;
	}
	
	u32 highestUnallocatedChannel = 0;
	u32 highestUnallocatedPriority = 0;
	u32 lowestAllocatedChannel = 0;
	u32 lowestAllocatedPriority = 0xff;
	//if all mix channels are allocated, check if there is an unallocated asset channel with a higher priority than an allocated asset channel
	do{
		if(unallocatedAssetNum == 0){
			return;
		}
		
		highestUnallocatedChannel = 0;
		highestUnallocatedPriority = 0;
		lowestAllocatedChannel = 0;
		lowestAllocatedPriority = 0xff;
		
		//find the highest priority unallocated asset
		for(u32 i = 0; i < unallocatedAssetNum; i++){
			if(unallocatedSound[i]->priority >= highestUnallocatedPriority){
				highestUnallocatedPriority = unallocatedSound[i]->priority;
				highestUnallocatedChannel = i;
			}
		}
		
		//find the lowest priority allocated asset
		for(u32 i = 0; i < MAX_DMA_CHANNELS; i++){
			if(allocatedMixChannels[i]->priority < lowestAllocatedPriority){
				lowestAllocatedPriority = allocatedMixChannels[i]->priority;
				lowestAllocatedChannel = i;
			}
		}
		
		//check if the unallocated channel is higher priority than the allocated channel
		if(highestUnallocatedPriority >= lowestAllocatedPriority){
			allocatedMixChannels[lowestAllocatedChannel]->channelIndex = 0xff;
			allocatedMixChannels[lowestAllocatedChannel] = unallocatedSound[highestUnallocatedChannel];
			unallocatedSound[highestUnallocatedChannel]->channelIndex = lowestAllocatedChannel;
			channelsMixData[lowestAllocatedChannel].state = 0;
			allocatedMixChannels[lowestAllocatedChannel]->noteState = NO_NOTE;
			allocatedMixChannels[lowestAllocatedChannel]->triggerState = CANCEL_TRIGGER;
			unallocatedSound[highestUnallocatedChannel] = unallocatedSound[unallocatedAssetNum - 1];
			
			unallocatedAssetNum--;
		}
	}
	while(highestUnallocatedPriority > lowestAllocatedPriority);
	
}

void processAudio(){
	//mark the audio as started
	audioProgress = 1;	
	
	s8 *audioBufferPtr;
	u32 samplesNeeded;
	
	//set the audio pointers based on the parity of the current frame
	if (audioTimer & 1) {
	    audioBufferPtr = soundBuffer1;
	}
	else{
	    audioBufferPtr = soundBuffer2;
	}
	
	//figure out how many samples we need this frame
	if(audioError < 0){
		samplesNeeded = MAX_SAMPLES_IN_ONE_FRAME;
	}
	else{
		samplesNeeded = MAX_SAMPLES_IN_ONE_FRAME - 16;
	}
	
	u32 areAssetsPlaying = 0;
	//check if there is at least one asset playing right now
	for(u32 slotIndex = 0; slotIndex < MAX_SOUNDS_IN_QUEUE; slotIndex++){
		if(activeSounds[slotIndex].enabled == 1){
			areAssetsPlaying = 1;
		}
	}
	
	//if at least one asset is playing
	if(areAssetsPlaying){
		//repeat until all samples needed this frame are processed
		while(samplesNeeded != 0){
			//check which channel has the fewest leftoverSamples
			u32 samplesThisBatch = 0x10000;
			for(u32 slotIndex = 0; slotIndex < MAX_SOUNDS_IN_QUEUE; slotIndex++){
				//ignore assets that aren't playing
				if(activeSounds[slotIndex].enabled != 1){
					continue;
				}
				//if this asset has no leftover samples, process a tick
				if(activeSounds[slotIndex].leftoverSamples == 0){
					processAssetTick(&activeSounds[slotIndex], slotIndex);
					activeSounds[slotIndex].leftoverSamples = tempoTable[activeSounds[slotIndex].currentTempo - 32];
				}
				if(activeSounds[slotIndex].leftoverSamples < samplesThisBatch){
					samplesThisBatch = activeSounds[slotIndex].leftoverSamples;
				}
			}
			//check if the value is bigger than the maximum batch size
			if(samplesThisBatch > MAX_SAMPLES_AT_ONCE){
				samplesThisBatch = MAX_SAMPLES_AT_ONCE;
			}
			//check if the value will finish this frame
			if(samplesThisBatch > samplesNeeded){
				samplesThisBatch = samplesNeeded;
			}
			//mix this many samples using the current settings
			mixAudio(channelsMixData, audioBufferPtr, samplesThisBatch, MAX_DMA_CHANNELS);
			audioBufferPtr += samplesThisBatch;
			
			samplesNeeded -= samplesThisBatch;
			//update the leftover samples of every channel
			for(u32 assetIndex = 0; assetIndex < MAX_SOUNDS_IN_QUEUE; assetIndex++){
				activeSounds[assetIndex].leftoverSamples -= samplesThisBatch;
			}
		}
	}

	//mark the audio as finished
	audioProgress = 2;
}

void processAssetTick(CurrentSoundSettings *assetPointer, u8 soundIndex){
	//if the asset just started
	if(assetPointer->rowNum == 0){
		nextRow(&assetPointer->asset->patterns[assetPointer->asset->orders[0]], &assetPointer->patternOffset, assetPointer);
		assetPointer->rowNum = 1;
	}
	//if there is a delay
	else if(assetPointer->delayTicks > 0){
		assetPointer->delayTicks--;
	}
	//if we are moving on to a new row
	else if(assetPointer->tickCounter == (assetPointer->currentTickSpeed - 1)){
		PatternData *patternPtr;
		u16 numRows;
		
		//reset the tick counter
		assetPointer->tickCounter = 0;
		
		//store the last used settings
		for(u8 channel = 0; channel < MAX_DMA_CHANNELS; channel++){
			CurrentChannelSettings *channelPointer = &assetPointer->channelSettings[channel];
			channelPointer->previousBasicSettings.volume = channelPointer->currentBasicSettings.volume;
			channelPointer->previousBasicSettings.note = channelPointer->currentBasicSettings.note;
			channelPointer->previousBasicSettings.instrument = channelPointer->currentBasicSettings.instrument;
			
			if(channelPointer->vibrato.state == PLAY_VIBRATO){
				channelPointer->vibrato.state = PROCESSED_VIBRATO;
			}
			if(channelPointer->tremolo.state == PLAY_VIBRATO){
				channelPointer->tremolo.state = PROCESSED_VIBRATO;
			}
			if(channelPointer->panbrello.state == PLAY_VIBRATO){
				channelPointer->panbrello.state = PROCESSED_VIBRATO;
			}
			if(channelPointer->autoVibrato.state == PLAY_VIBRATO){
				channelPointer->autoVibrato.state = PROCESSED_VIBRATO;
			}
			
			if(channelPointer->periodicEffectState == ARPEGGIO_1){
				channelPointer->currentPitch = (channelPointer->currentPitch * arpeggioTable[15 - ((channelPointer->currentBasicSettings.effectValue & 0xf0) >> 4)]) >> 12;
			}
			else if(channelPointer->periodicEffectState == ARPEGGIO_2){
				channelPointer->currentPitch = (channelPointer->currentPitch * arpeggioTable[15 - (channelPointer->currentBasicSettings.effectValue & 0xf)]) >> 12;
			}
			channelPointer->periodicEffectState = NO_PERIODIC_EFFECT;
		}
		
		//load the pattern data we are currently working through
		patternPtr = &assetPointer->asset->patterns[assetPointer->asset->orders[assetPointer->orderIndex]];
		numRows = patternPtr->rowsNum;
		
		//check if there is a position jump command
		if((assetPointer->globalEffects.B != 0xff) || (assetPointer->globalEffects.C != 0xff)){
			positionJump(assetPointer);
			patternPtr = &assetPointer->asset->patterns[assetPointer->asset->orders[assetPointer->orderIndex]];
			assetPointer->patternOffset = 0;
			for(u32 i = 1; i < assetPointer->rowNum; i++){
				nextRow(patternPtr, &assetPointer->patternOffset, assetPointer);
			}
		}
		
		//check if we have reached the last row number of this pattern
		else if(assetPointer->rowNum == numRows){
			//increment the order ID
			assetPointer->orderIndex++;
			//check if the asset is over
			if((assetPointer->orderIndex >= (assetPointer->asset->ordersNum)) || (assetPointer->asset->orders[assetPointer->orderIndex] == 0xff)){
				//end this asset and free it's slot for others
				endSound(soundIndex);
				return;
			}
			assetPointer->rowNum = 1;
			assetPointer->patternOffset = 0;
			//load the pattern data for the next pattern
			patternPtr = &assetPointer->asset->patterns[assetPointer->asset->orders[assetPointer->orderIndex]];
		}
		//if we have not yet reached the last row of this pattern
		else{
			//increment the row Number
			assetPointer->rowNum++;
		}
		//reset the global settings
		assetPointer->globalEffects.SE = 0xff;
		assetPointer->globalEffects.A = 0xff;
		//get the data for this row from the pattern table
		nextRow(patternPtr, &assetPointer->patternOffset, assetPointer);
		
	}
	//if we are not yet moving on to a new row
	else{
		//decrement the tick counter
		assetPointer->tickCounter++;
	}
	
	//check if there is a mixing volume slide
	if(assetPointer->volumeSlideAmount != 0){
		s32 finalVolume = assetPointer->finalVolume;
		s32 currentVolume = assetPointer->mixVolume;
		s32 slideAmount = assetPointer->volumeSlideAmount;
		
		//if volume is ramping down
		if(currentVolume > finalVolume){
			//if we hit the target volume
			if((currentVolume - slideAmount) <= finalVolume){
				assetPointer->volumeSlideAmount = 0;
				assetPointer->mixVolume = finalVolume;
				if(finalVolume == 0){
					endSound(soundIndex);
					return;
				}
			}
			else{
				assetPointer->mixVolume -= slideAmount;
			}
		}
		else{
			if((currentVolume + slideAmount) >= finalVolume){
				assetPointer->volumeSlideAmount = 0;
				assetPointer->mixVolume = finalVolume;
			}
			else{
				assetPointer->mixVolume += slideAmount;
			}
		}
	}
	
	for(u8 channel = 0; channel < MAX_DMA_CHANNELS; channel++){
		CurrentChannelSettings *channelPointer = &assetPointer->channelSettings[channel];
		//process any effects
		processEffects(channelPointer, assetPointer);
	}
	
	//now we go through each of the 8 sampled channels, and process all of their data.
	for(u8 channel = 0; channel < MAX_DMA_CHANNELS; channel++){
		u8 channelIndex = assetPointer->channelSettings[channel].channelIndex;
		if(channelIndex != 0xff){
			processSampledChannel(&assetPointer->channelSettings[channel], &channelsMixData[channelIndex], assetPointer);
		}
	}
}

void processSampledChannel(CurrentChannelSettings *channelPointer, ChannelData *channelMixBuffer, CurrentSoundSettings *assetPointer){
	//setup some local variables, and set them to default values
	u8 finalVolume = 128;
	u16 finalPitch = 0;
	s8 finalPanning = 0;
	u8 envelopeVolume = 64;
	s8 envelopePitch = 0;
	s8 envelopePanning = 0;
	s8 vibrato = 0;
	s8 tremolo = 0;
	s8 panbrello = 0;
	s8 autoVibrato = 0;
	u16 offset = 0;
	
	//process any volume commands
	if(channelPointer->triggerState != DELAY_TRIGGER){
		processVolume(channelPointer);
	}
	
	//process any new note data in the pattern
	if((channelPointer->pitchState == NEW_PITCH) && (channelPointer->triggerState != DELAY_TRIGGER)){
		processNote(channelPointer, assetPointer);
	}
	
	if(channelPointer->triggerState != NO_TRIGGER){
		processTrigger(channelPointer, assetPointer);
	}

	//if the note is currently fading out
	if(channelPointer->noteState == FADEOUT_NOTE){
		//if the fade component reaches zero
		if(channelPointer->noteFadeComponent <= (channelPointer->instrumentPointer->fadeOut)){
			channelPointer->noteState = NO_NOTE;
		}
		//otherwise, reduce the fade component
		else{
			channelPointer->noteFadeComponent -= (channelPointer->instrumentPointer->fadeOut);
		}
	}
	
	if(channelPointer->noteState != NO_NOTE){
		//process the three envelopes
		if((channelPointer->volumeEnvelope.enabled & 2) || ((channelPointer->volumeEnvelope.enabled & 1) && !(channelPointer->volumeEnvelope.enabled & 4))){
			envelopeVolume = processEnvelope(&channelPointer->volumeEnvelope, &channelPointer->instrumentPointer->volEnvelope, channelPointer->noteState);
			//if the end of the volume envelope is reached, switch to fadeout
			if((channelPointer->instrumentPointer->volEnvelope.nodeCount - 1) == (channelPointer->volumeEnvelope.currentNodeIndex)){
				channelPointer->noteState = FADEOUT_NOTE;
			}	
		}
		else if(channelPointer->noteState == PLAY_NOTE){
			channelPointer->noteState = FADEOUT_NOTE;
		}
		if((channelPointer->pitchEnvelope.enabled & 2) || ((channelPointer->pitchEnvelope.enabled & 1) && !(channelPointer->pitchEnvelope.enabled & 4))){
			envelopePitch = processEnvelope(&channelPointer->pitchEnvelope, &channelPointer->instrumentPointer->pitchEnvelope, channelPointer->noteState);
			channelPointer->pitchModifier = (channelPointer->pitchModifier * portamentoTable[256 + (envelopePitch << 3)]) >> 12;
		}
		if((channelPointer->panningEnvelope.enabled & 2) || ((channelPointer->panningEnvelope.enabled & 1) && !(channelPointer->panningEnvelope.enabled & 4))){
			envelopePanning = processEnvelope(&channelPointer->panningEnvelope, &channelPointer->instrumentPointer->panEnvelope, channelPointer->noteState);
		}
	}
	
	//process the 4 vibratos
	vibrato = processVibrato(&channelPointer->vibrato);
	tremolo = processVibrato(&channelPointer->tremolo);
	panbrello = processVibrato(&channelPointer->panbrello);
	autoVibrato = processVibrato(&channelPointer->autoVibrato);
	
	//calculate the final volume
	finalVolume = ((((envelopeVolume * channelPointer->noteFadeComponent * channelPointer->currentVolume * assetPointer->globalVolume >> 6) *
				channelPointer->channelVolume >> 6) * channelPointer->samplePointer->globalVolume >> 6) * channelPointer->instrumentPointer->globalVolume >> 8) * 
				assetPointer->mixVolume >> 23;
	//if the tremor has turned off the note right now
	if(finalVolume + tremolo > 0x80){
		finalVolume = 0x80;
	}
	else if(finalVolume + tremolo < 0){
		finalVolume = 0;
	}
	else{
		finalVolume += tremolo;
	}
	if(channelPointer->periodicEffectState == TREMOR_OFF){
		finalVolume = 0;
	}
	
	//calculate the final pitch
	finalPitch = (channelPointer->currentPitch * channelPointer->pitchModifier) >> 12;
	channelPointer->currentPitch = finalPitch;
	finalPitch = (finalPitch * vibratoTable[128 + vibrato]) >> 12;
	finalPitch = (finalPitch * vibratoTable[128 + autoVibrato]) >> 12;
	channelPointer->pitchModifier = 0x1000;
	
	//calculate the final pan
	//if we are using envelope pan
	if(channelPointer->currentPanning & 0x80){
		finalPanning = envelopePanning;
		channelPointer->currentPanning = envelopePanning + 32;
	}
	else{
		finalPanning = channelPointer->currentPanning - 32;
	}
	if(finalPanning + panbrello < -32){
		finalPanning = -32;
	}
	else if(finalPanning + panbrello > 32){
		finalPanning = 32;
	}
	else{
		finalPanning += panbrello;
	}
	
	//if there is an active sample offset
	if(channelPointer->offset & 0x8000){
		offset = channelPointer->offset & 0xfff;
		channelPointer->offset &= 0xf00;
	}
	
	applySettings(channelMixBuffer, channelPointer->samplePointer, finalPitch, finalVolume, finalPanning, &channelPointer->noteState, offset);
}

//volume from 0 to 128
//panning from -32 to 32

//take the settings dictated by the IT asset, and translate them into settings for the mixer
void applySettings(ChannelData *channelMixData, AudioSample *samplePtr, u32 pitch, u8 volume, s8 panning, enum NoteState *state, u16 offset){
	s32 temp;
	//set the new pitch
	channelMixData->pitch = pitch;
	
	//set the right volume
	temp = -((volume * panning) >> 5) + volume;
	if(temp == 256){
		temp = 255;
	}
	channelMixData->rightVolume = (u8)temp;
	
	//set the left volume
	temp = ((volume * panning) >> 5) + volume;
	if(temp == 256){
		temp = 255;
	}
	channelMixData->leftVolume = (u8)temp;
	
	u8 flags = samplePtr->sampleType;
	//set the correct state of the sample according to the state of the channel
	switch(*state){
	case NO_NOTE:
		channelMixData->state = 0;
		break;
	case TRIGGER_TICK_NOTE:
		//set the starting index
		channelMixData->sampleIndex = offset << 20;
		//set the sample pointer
		channelMixData->samplePtr = samplePtr;
		
		//set the correct starting state
		//if the sample has a sustain loop
		if(flags & 0x2){
			//if the sustain loop is a ping pong loop
			if(flags & 0x8){
				channelMixData->state = 8;
			}
			//if the sustain loop is an overflow loop
			else{
				channelMixData->state = 7;
			}
		}
		//if the sample does not have a sustain loop, but does have a normal loop
		else if(flags & 0x1){
			//if the normal loop is a ping pong loop
			if(flags & 0x4){
				channelMixData->state = 4;
			}
			//if the normal loop is an overflow loop
			else{
				channelMixData->state = 3;
			}
		}
		//if the sample does not have any loops
		else{
			channelMixData->state = 1;
		}
		*state = SUSTAIN_NOTE;
		break;
	case SUSTAIN_NOTE:
		//no changes to state necessary
		break;
	case PLAY_NOTE:
		//same state management as FADEOUT
	case FADEOUT_NOTE:
		//if the sample is sustain-looping
		if(channelMixData->state >= 7){
			//if the sustain loop is currently moving right
			if(channelMixData->state != 9){
				//if the sample has a loop
				if(flags & 0x1){
					//if the loop is a ping-pong loop
					if(flags & 0x4){
						channelMixData->state = 4;
					}
					//if the loop is an overflow loop
					else{
						channelMixData->state = 3;
					}
				}
				//if the sample has no loop
				else{
					channelMixData->state = 1;
				}
			}
			//if the sustain loop is a ping-pong loop, currently moving left
			else{
				//if the sample has a loop
				if(flags & 0x1){
					//if the loop is a ping-pong loop
					if(flags & 0x4){
						channelMixData->state = 5;
					}
					//if the loop is an overflow loop
					else{
						channelMixData->state = 6;
					}
				}
				//if the sample has no loop
				else{
					channelMixData->state = 2;
				}
			}
		}
		break;
	default:
		break;
	}
}

void nextRow(PatternData *patternPtr, u16 *patternOffsetPtr, CurrentSoundSettings *assetPointer){
	u8 channelMask;
	ChannelBasicSettings *previousSettings;
	ChannelBasicSettings *currentSettings;
	u8 isChannelModified[MAX_DMA_CHANNELS] = {0};
	u16 patternOffset = *patternOffsetPtr;

	//load the channel mask
	channelMask = patternPtr->packedPatternData[patternOffset];
	patternOffset++;
	
	//complete this loop until the channelMask becomes 0
	while(channelMask != 0){
		u8 channel;
		u8 maskVariable;
		
		//determine the channel being modified
		channel = (channelMask & 0x3f) - 1;
		previousSettings = &assetPointer->channelSettings[channel].previousBasicSettings;
		currentSettings = &assetPointer->channelSettings[channel].currentBasicSettings;
		isChannelModified[channel] = 1;
		
		//load a new maskVariable if channelMask says to
		if(channelMask & 0x80){
			maskVariable = patternPtr->packedPatternData[patternOffset];
			patternOffset++;
			assetPointer->channelSettings[channel].maskVariable = maskVariable;
		}
		//otherwise reuse the last maskVariable
		else{
			maskVariable = assetPointer->channelSettings[channel].maskVariable;
		}
		
		//check if a new note needs to be loaded
		if(maskVariable & 0x1){
			//load the new note
			currentSettings->note = patternPtr->packedPatternData[patternOffset];
			patternOffset++;
			assetPointer->channelSettings[channel].pitchState = NEW_PITCH;
		}
		//if the last note gets reused
		else if(maskVariable & 0x10){
			//load the last note used
			currentSettings->note = previousSettings->note;
			assetPointer->channelSettings[channel].pitchState = NEW_PITCH;
		}
		
		//check if a new instrument needs to be loaded
		if(maskVariable & 0x2){
			currentSettings->instrument = patternPtr->packedPatternData[patternOffset];
			patternOffset++;
			assetPointer->channelSettings[channel].triggerState = TRIGGER;
		}
		//if the last instrument gets reused
		else if(maskVariable & 0x20){
			currentSettings->instrument = previousSettings->instrument;
			assetPointer->channelSettings[channel].triggerState = TRIGGER;
		}
		
		//check if a new volume needs to be loaded
		if(maskVariable & 0x4){
			currentSettings->volume = patternPtr->packedPatternData[patternOffset];
			patternOffset++;
			assetPointer->channelSettings[channel].volumeState = TRIGGER_TICK_VOLUME;
		}
		//if the last volume gets reused
		else if(maskVariable & 0x40){
			currentSettings->volume = previousSettings->volume;
			assetPointer->channelSettings[channel].volumeState = TRIGGER_TICK_VOLUME;
		}
		else{
			assetPointer->channelSettings[channel].volumeState = NO_VOLUME;
		}
		
		//check if a new effect needs to be loaded
		if(maskVariable & 0x8){
			currentSettings->effect = patternPtr->packedPatternData[patternOffset];
			patternOffset++;
			currentSettings->effectValue = patternPtr->packedPatternData[patternOffset];
			patternOffset++;
			previousSettings->effectValue = currentSettings->effectValue;
			previousSettings->effect = currentSettings->effect;
			assetPointer->channelSettings[channel].effectState = TRIGGER_TICK_EFFECT;
		}
		//if the last effect gets reused
		else if(maskVariable & 0x80){
			currentSettings->effect = previousSettings->effect;
			currentSettings->effectValue = previousSettings->effectValue;
			assetPointer->channelSettings[channel].effectState = TRIGGER_TICK_EFFECT;
		}
		else{
			assetPointer->channelSettings[channel].effectState = NO_EFFECT;
		}
		
		//load the next channelmask
		channelMask = patternPtr->packedPatternData[patternOffset];
		patternOffset++;
	}
	
	//handle all the unmodified channels
	for(u32 channel = 0; channel < MAX_DMA_CHANNELS; channel++){
		if(isChannelModified[channel]){
			continue;
		}
		else{
			assetPointer->channelSettings[channel].effectState = NO_EFFECT;
			assetPointer->channelSettings[channel].volumeState = NO_VOLUME;
		}
	}
	*patternOffsetPtr = patternOffset;
}

void processEffects(CurrentChannelSettings *channelPointer, CurrentSoundSettings *assetPointer){
	u8 command = channelPointer->currentBasicSettings.effect;
	u8 commandAmount = channelPointer->currentBasicSettings.effectValue;
	//if there was no effect command to process
	if(channelPointer->effectState == NO_EFFECT){
		return;
	}
	//if there is a new effect command this tick
	else if(channelPointer->effectState == TRIGGER_TICK_EFFECT){
		switch(command){
		//if it is an Axx "set speed" command
		case 1:
			if((commandAmount != 0) && (assetPointer->globalEffects.A == 0xff)){
				assetPointer->currentTickSpeed = commandAmount;
			}
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Bxx "position jump" command
		case 2:
			if(assetPointer->globalEffects.B == 0xff){
				assetPointer->globalEffects.B = commandAmount;
			}
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Cxx "pattern break" command
		case 3:
			if(assetPointer->globalEffects.C == 0xff){
				assetPointer->globalEffects.C = commandAmount;
			}
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Dxx "volume slide" command
		case 4:
			//if it is a "fine volume slide" command
			if(slideCheck(&channelPointer->effectMemory.D, commandAmount)){
				volumeSlide(&channelPointer->currentVolume, &channelPointer->effectMemory.D, 64, commandAmount);
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a regular "volume slide" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			return;
		//if it is a Exx "portamento down" command
		case 5:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.EeFf;
			}
			else{
				channelPointer->effectMemory.EeFf = commandAmount;
			}
			//if it is a "fine portamento down" or "extra fine portamento down" command
			if(portamentoCheck(commandAmount)){
				portamentoDown(channelPointer, commandAmount);
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a regular "portamento down" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			return;
		//if it is a Fxx "portamento up" command
		case 6:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.EeFf;
			}
			else{
				channelPointer->effectMemory.EeFf = commandAmount;
			}
			//if it is a "fine portamento up" or "extra fine portamento up" command
			if(portamentoCheck(commandAmount)){
				portamentoUp(channelPointer, commandAmount);
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a regular "portamento down" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			return;
		//if it is a Gxx "tone portamento" command
		case 7:
			channelPointer->triggerState = CANCEL_TRIGGER;
			channelPointer->effectState = EVERY_TICK_EFFECT;
			return;
		//if it is a Hxx "vibrato" command
		case 8:
			initVibrato(&channelPointer->vibrato, &channelPointer->effectMemory.HUh, commandAmount, 0, 0);
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Ixx "tremor" command
		case 9:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.I;
			}
			else{
				channelPointer->effectMemory.I = commandAmount;
			}
			channelPointer->effectState = EVERY_TICK_EFFECT;
			channelPointer->periodicEffectState = TREMOR_ON;
			channelPointer->periodicEffectTimer = ((commandAmount & 0xf0) >> 4) - 1;
			return;
		//if it is a Jxx "arpeggio" command
		case 10:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.J;
			}
			else{
				channelPointer->effectMemory.J = commandAmount;
			}
			channelPointer->periodicEffectState = ARPEGGIO_0;
			channelPointer->effectState = EVERY_TICK_EFFECT;
			return;
		//if it is a Kxx "volume slide + vibrato" command
		case 11:
			//if it is a "fine volume slide" command
			if(slideCheck(&channelPointer->effectMemory.D, commandAmount)){
				volumeSlide(&channelPointer->currentVolume, &channelPointer->effectMemory.D, 64, commandAmount);
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a regular "volume slide" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			initVibrato(&channelPointer->vibrato, &channelPointer->effectMemory.HUh, 0, 0, 0);
			return;
		//if it is a Lxx "volume slide + tone portamento" command
		case 12:
			//if it is a "fine volume slide" command
			if(slideCheck(&channelPointer->effectMemory.D, commandAmount)){
				volumeSlide(&channelPointer->currentVolume, &channelPointer->effectMemory.D, 64, commandAmount);
			}
			channelPointer->effectState = EVERY_TICK_EFFECT;
			return;
		//if it is a Mxx "set channel volume" command
		case 13:
			//bounds check
			if(commandAmount > 0x40){
				commandAmount = 0x40;
			}
			channelPointer->channelVolume = commandAmount;
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Nxx "channel volume slide" command
		case 14:
			//if it is a "fine channel volume slide" command
			if(slideCheck(&channelPointer->effectMemory.N, commandAmount)){
				volumeSlide(&channelPointer->channelVolume, &channelPointer->effectMemory.N, 64, commandAmount);
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a regular "channel volume slide" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			return;
		//if it is a Oxx "sample offset" command
		case 15:
			channelPointer->offset |= (0x8000 | commandAmount);
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Pxx "panning slide" command
		case 16:
			//if it is a "fine panning slide" command
			if(slideCheck(&channelPointer->effectMemory.P, commandAmount)){
				panningSlide(&channelPointer->currentPanning, &channelPointer->effectMemory.P, commandAmount);
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a regular "panning slide" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			return;
		//if it is a Qxx "retrigger" command
		case 17:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.Q;
			}
			else{
				channelPointer->effectMemory.Q = commandAmount;
			}
			channelPointer->periodicEffectTimer = (commandAmount & 0xf) - 1;
			channelPointer->effectState = EVERY_TICK_EFFECT;
			return;
		//if it is a Rxx "Tremolo" command
		case 18:
			initVibrato(&channelPointer->tremolo, &channelPointer->effectMemory.R, commandAmount, 0, 0);
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is one of the many Sxx commands
		case 19:
			command = (commandAmount & 0xf0) >> 4;
			commandAmount = commandAmount & 0xf;
			switch(command){
			//if it is a S1x "glissando control" command
			case 1:
				
				return;
			//if it is a S3x "set vibrato waveform" command
			case 3:
				channelPointer->vibrato.waveformType = commandAmount;
				channelPointer->effectState = NO_EFFECT;
				return;
			//if it is a S4x "set tremolo waveform" command
			case 4:
				channelPointer->tremolo.waveformType = commandAmount;
				channelPointer->effectState = NO_EFFECT;
				return;
			//if it is a S5x "set panbrello waveform" command
			case 5:
				channelPointer->panbrello.waveformType = commandAmount;
				channelPointer->effectState = NO_EFFECT;
				return;
			//if it is a S6x "fine pattern delay" command
			case 6:
				assetPointer->delayTicks += commandAmount & 0xf;
				channelPointer->effectState = NO_EFFECT;
				return;
			//if it is one of the many S7x commands
			case 7:
				command = commandAmount;
				switch(command){
				//if it is a S77 "volume envelope off" command
				case 7:
					channelPointer->volumeEnvelope.enabled = (channelPointer->volumeEnvelope.enabled & 1) | 0x84;
					channelPointer->effectState = EVERY_TICK_EFFECT;
					return;
				//if it is a S78 "volume envelope on" command
				case 8:
					channelPointer->volumeEnvelope.enabled = (channelPointer->volumeEnvelope.enabled & 1) | 0x82;
					channelPointer->effectState = EVERY_TICK_EFFECT;
					return;
				//if it is a S79 "panning envelope off" command
				case 9:
					channelPointer->panningEnvelope.enabled = (channelPointer->panningEnvelope.enabled & 1) | 0x84;
					//mark the panning envelope as not in use
					channelPointer->currentPanning &= 0x7f;
					channelPointer->effectState = EVERY_TICK_EFFECT;
					return;
				//if it is a S7A "panning envelope on" command
				case 10:
					channelPointer->panningEnvelope.enabled = (channelPointer->panningEnvelope.enabled & 1) | 0x82;
					channelPointer->effectState = EVERY_TICK_EFFECT;
					return;
				//if it is a S7B "pitch envelope off" command
				case 11:
					channelPointer->pitchEnvelope.enabled = (channelPointer->pitchEnvelope.enabled & 1) | 0x84;
					channelPointer->effectState = EVERY_TICK_EFFECT;
					return;
				//if it is a S7C "pitch envelope on" command
				case 12:
					channelPointer->panningEnvelope.enabled = (channelPointer->panningEnvelope.enabled & 1) | 0x82;
					channelPointer->effectState = EVERY_TICK_EFFECT;
					return;
				default:
					channelPointer->effectState = NO_EFFECT;
					return;
				}
				return;
			//if it is a S8x "set panning" command
			case 8:
				if(commandAmount == 0xf){
					commandAmount = 64;
				}
				else{
					commandAmount <<= 2;
				}
				channelPointer->channelPan = commandAmount | 0x80;
				return;
			//if it is a SAx "high offset" command
			case 10:
				channelPointer->offset = commandAmount << 8;
				channelPointer->effectState = NO_EFFECT;
				return;
			//if it is a SBx "pattern loop" command
			case 11:;
				//if we are setting a loop point and are not currently in a loop
				if((commandAmount == 0) && (channelPointer->effectMemory.SBx == 0xff)){
					channelPointer->effectMemory.SB0 = assetPointer->rowNum;
				}
				//if we are starting a new loop
				else if((commandAmount != 0) && (channelPointer->effectMemory.SBx == 0xff)){
					channelPointer->effectMemory.SBx = commandAmount;
				}
				//if we are ending a loop
				if((commandAmount != 0) && (channelPointer->effectMemory.SBx == 0)){
					channelPointer->effectMemory.SBx = 0xff;
				}
				//if we are performing one loop
				else if(commandAmount != 0){
					channelPointer->effectMemory.SBx--;
					assetPointer->globalEffects.B = assetPointer->orderIndex;
					assetPointer->globalEffects.C = channelPointer->effectMemory.SB0;
				}
				channelPointer->effectState = NO_EFFECT;
				return;
			//if it is a SCx "note cut" command
			case 12: 
				if(commandAmount == 0){
					commandAmount = 1;
				}
				channelPointer->periodicEffectTimer = commandAmount - 1;
				channelPointer->effectState = EVERY_TICK_EFFECT;
				return;
			//if it is a SDx "note delay" command
			case 13:
				if(commandAmount == 0){
					commandAmount = 1;
				}
				channelPointer->periodicEffectTimer = commandAmount - 1;
				channelPointer->triggerState = DELAY_TRIGGER;
				channelPointer->effectState = EVERY_TICK_EFFECT;
				return;
			//if it is a SEx "pattern delay" command
			case 14:
				if(assetPointer->globalEffects.SE == 0xff){
					assetPointer->globalEffects.SE = commandAmount;
					assetPointer->delayTicks += assetPointer->currentTickSpeed * (commandAmount);
					channelPointer->effectState = NO_EFFECT;
				}
				return;
			default:
				channelPointer->effectState = NO_EFFECT;
				return;
			}
			return;
		//if it is a Txx "tempo control" command
		case 20:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.T;
			}
			else{
				channelPointer->effectMemory.T = commandAmount;
			}
			//if it is a "set tempo command"
			if(commandAmount >= 0x20){
				assetPointer->currentTempo = commandAmount;
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a "increase tempo" or "decrease tempo" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			return;
		//if it is a Uxx "fine vibrato" command
		case 21:
			initVibrato(&channelPointer->vibrato, &channelPointer->effectMemory.HUh, commandAmount, 0, 1);
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Vxx "set global volume" command
		case 22:
			//bounds check
			if(commandAmount > 0x80){
				commandAmount = 0x80;
			}
			assetPointer->globalVolume = commandAmount;
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Wxx "global volume slide" command
		case 23:
			//if it is a "fine global volume slide" command
			if(slideCheck(&channelPointer->effectMemory.W, commandAmount)){
				volumeSlide(&assetPointer->globalVolume, &channelPointer->effectMemory.W, 128, commandAmount);
				channelPointer->effectState = NO_EFFECT;
			}
			//if it is a regular "global volume slide" command
			else{
				channelPointer->effectState = EVERY_TICK_EFFECT;
			}
			return;
		//if it is a Xxx "set panning" command
		case 24:
			if(commandAmount == 0xff){
				commandAmount = 64;
			}
			else{
				commandAmount >>= 2;
			}
			channelPointer->channelPan = commandAmount | 0x80;
			channelPointer->effectState = NO_EFFECT;
			return;
		//if it is a Yxx "panbrello" command
		case 25:
			initVibrato(&channelPointer->panbrello, &channelPointer->effectMemory.Y, commandAmount, 0, 2);
			channelPointer->effectState = NO_EFFECT;
			return;
		default:
			channelPointer->effectState = NO_EFFECT;
			return;
		}
	}
	else if(channelPointer->effectState == EVERY_TICK_EFFECT){
		switch(command){
		//if it is a Dxx "volume slide" command
		case 4:
			volumeSlide(&channelPointer->currentVolume, &channelPointer->effectMemory.D, 64, commandAmount);
			return;
		//if it is a Exx "portamento down" command
		case 5:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.EeFf;
			}
			portamentoDown(channelPointer, commandAmount);
			return;
		//if it is a Fxx "portamento up" command
		case 6:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.EeFf;
			}
			portamentoUp(channelPointer, commandAmount);
			return;
		//if it is a Gxx "tone portamento" command
		case 7:
			tonePortamento(channelPointer, commandAmount);
			return;
		//if it is a Ixx "tremor" command
		case 9:
			if(channelPointer->periodicEffectTimer == 0){
				if(channelPointer->periodicEffectState == TREMOR_ON){
					channelPointer->periodicEffectState = TREMOR_OFF;
					channelPointer->periodicEffectTimer = (commandAmount & 0xf) - 1;
				}
				else{
					channelPointer->periodicEffectState = TREMOR_ON;
					channelPointer->periodicEffectTimer = ((commandAmount & 0xf0) >> 4) - 1;
				}
			}
			else{
				channelPointer->periodicEffectTimer--;
			}
			return;
		//if it is a Jxx "arpeggio" command
		case 10:
			arpeggio(channelPointer, commandAmount);
			
			return;
		//if it is a Kxx "volume slide + vibrato" command
		case 11:
			volumeSlide(&channelPointer->currentVolume, &channelPointer->effectMemory.D, 64, commandAmount);
			return;
		//if it is a Lxx "volume slide + tone portamento" command
		case 12:
			if(slideCheck(&channelPointer->effectMemory.D, commandAmount) == 0){
				volumeSlide(&channelPointer->currentVolume, &channelPointer->effectMemory.D, 64, commandAmount);
			}
			commandAmount = channelPointer->effectMemory.Gg;
			tonePortamento(channelPointer, commandAmount);
			return;
		//if it is a Nxx "channel volume slide" command
		case 14:
			volumeSlide(&channelPointer->channelVolume, &channelPointer->effectMemory.N, 64, commandAmount);
			return;
		//if it is a Pxx "panning slide" command
		case 16:
			panningSlide(&channelPointer->currentPanning, &channelPointer->effectMemory.P, commandAmount);
			return;
		//if it is a Qxx "retrigger" command
		case 17:
			if(channelPointer->periodicEffectTimer == 0){
				retrigger(channelPointer, commandAmount);
			}
			else{
				channelPointer->periodicEffectTimer--;
			}
			return;
		//if it is one of the many Sxx commands
		case 19:
			command = (commandAmount & 0xf0) >> 4;
			switch(command){
			//if it is a SCx "note cut" command
			case 12: 
				if(channelPointer->periodicEffectTimer == 0){
					channelPointer->noteState = NO_NOTE;
					channelPointer->effectState = NO_EFFECT;
				}
				else{
					channelPointer->periodicEffectTimer--;
				}
				return;
			//if it is a SDx "note delay" command
			case 13:
				if(channelPointer->periodicEffectTimer == 0){
					channelPointer->triggerState = NO_TRIGGER;
					channelPointer->effectState = NO_EFFECT;
				}
				else{
					channelPointer->periodicEffectTimer--;
				}
				return;
			default:
				channelPointer->effectState = NO_EFFECT;
				return;
			}
			return;
		//if it is a Txx "tempo control" command
		case 20:
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.T;
			}
			//if it is a "increase tempo" command
			if(commandAmount >= 0x10){
				increaseTempo(commandAmount & 0xf, assetPointer);
			}
			else{
				decreaseTempo(commandAmount & 0xf, assetPointer);
			}
			return;
		//if it is a Wxx "global volume slide" command
		case 23:
			volumeSlide(&assetPointer->globalVolume, &channelPointer->effectMemory.W, 128, commandAmount);
			return;
		default:
			channelPointer->effectState = NO_EFFECT;
			return;
		}
	}
}

void processVolume(CurrentChannelSettings *channelPointer){
	u8 command = channelPointer->currentBasicSettings.volume;
	u8 commandAmount;
	//if there was no volume command to process
	if(channelPointer->volumeState == NO_VOLUME){
		return;
	}
	//if there was a new volume command this tick
	else if(channelPointer->volumeState == TRIGGER_TICK_VOLUME){
		//if it is a "set volume" command
		if(command <= 64){
			channelPointer->currentVolume = command;
			channelPointer->volumeState = PROCESSED_VOLUME;
			return;
		}
		//if it is a "set panning" command
		else if((command <= 192) && (command >= 128)){
			channelPointer->currentPanning = (command - 192) | 0x80;
			channelPointer->volumeState = NO_VOLUME;
			return;
		}
		//if it is a "fine volume up" command
		else if(command <= 74){
			commandAmount = command - 65;
			channelPointer->volumeState = NO_VOLUME;
			volumeSlideUp(channelPointer, commandAmount);
			return;
		}
		//if it is a "fine volume down command"
		else if(command <= 84){
			commandAmount = command - 75;
			channelPointer->volumeState = NO_VOLUME;
			volumeSlideDown(channelPointer, commandAmount);
			return;
		}
		//if it is a "volume slide up" command
		else if(command <= 94){
			channelPointer->volumeState = EVERY_TICK_VOLUME;
			return;
		}
		//if it is a "volume slide down" command
		else if(command <= 104){
			channelPointer->volumeState = EVERY_TICK_VOLUME;
			return;
		}
		//if it is a "portamento down" command
		else if(command <= 114){
			commandAmount = (command - 105) << 2;
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.EeFf;
			}
			else{
				channelPointer->effectMemory.EeFf = commandAmount;
			}
			//if the function returns 1, this effect is done
			if(portamentoCheck(commandAmount)){
				channelPointer->volumeState = NO_VOLUME;
				portamentoDown(channelPointer, commandAmount);
			}
			else{
				channelPointer->volumeState = EVERY_TICK_VOLUME;
			}
			return;
		}
		//if it is a "portamento up" command
		else if(command <= 124){
			commandAmount = (command - 115) << 2;
			if(commandAmount == 0){
				commandAmount = channelPointer->effectMemory.EeFf;
			}
			else{
				channelPointer->effectMemory.EeFf = commandAmount;
			}
			//if the function returns 1, this effect is done
			if(portamentoCheck(commandAmount)){
				channelPointer->volumeState = NO_VOLUME;
				portamentoUp(channelPointer, commandAmount);
			}
			else{
				channelPointer->volumeState = EVERY_TICK_VOLUME;
			}
			return;
		}
		//if it is an unsopported command
		else if(command <= 127){
			channelPointer->volumeState = NO_VOLUME;
			return;
		}
		//if it is a "tone portamento" command
		else if(command <= 202){
			channelPointer->triggerState = CANCEL_TRIGGER;
			channelPointer->volumeState = EVERY_TICK_VOLUME;
			return;
		}
		//if it is a "vibrato" command
		else if(command <= 212){
			channelPointer->volumeState = NO_VOLUME;
			commandAmount = command - 203;
			if(commandAmount != 0){
				commandAmount = (channelPointer->effectMemory.HUh & 0xf0) | commandAmount;
			}
			initVibrato(&channelPointer->vibrato, &channelPointer->effectMemory.HUh, commandAmount, 0, 0);
			return;
		}
		//if it is an unsupported command
		else{
			channelPointer->volumeState = NO_VOLUME;
			return;
		}
	}
	//if an old volume command is still running
	else if(channelPointer->volumeState == EVERY_TICK_VOLUME){
		//if it is a "volume slide up" command
		if(command <= 94){
			commandAmount = command - 85;
			volumeSlideUp(channelPointer, commandAmount);
			return;
		}
		//if it is a "volume slide down" command
		else if(command <= 104){
			commandAmount = command - 95;
			volumeSlideDown(channelPointer, commandAmount);
			return;
		}
		//if it is a "portamento down" command
		else if(command <= 114){
			commandAmount = (command - 105) << 2;
			portamentoDown(channelPointer, commandAmount);
			return;
		}
		//if it is a "portamento up" command
		else if(command <= 124){
			commandAmount = (command - 115) << 2;
			portamentoUp(channelPointer, commandAmount);
			return;
		}
		//if it is a "tone portamento" command
		else{
			commandAmount = tonePortamentoLUT[command - 193];
			tonePortamento(channelPointer, commandAmount);
			return;
		}
	}
}

void processTrigger(CurrentChannelSettings *channelPointer, CurrentSoundSettings *assetPointer){
	//if a note just got triggered, record the new instrument and sample
	if(channelPointer->triggerState == TRIGGER){
		channelPointer->instrumentPointer = &assetPointer->asset->instruments[channelPointer->currentBasicSettings.instrument - 1];
		channelPointer->samplePointer = sampleList[assetPointer->asset->samples[channelPointer->instrumentPointer->keyboardSample[channelPointer->currentBasicSettings.note]]];
		channelPointer->noteFadeComponent = 1024;
		channelPointer->currentPitch = channelPointer->notePitch;
		channelPointer->noteState = TRIGGER_TICK_NOTE;
		channelPointer->triggerState = NO_TRIGGER;
		if(channelPointer->volumeState != PROCESSED_VOLUME){
			channelPointer->currentVolume = channelPointer->samplePointer->defaultVolume;
		}
		else{
			channelPointer->volumeState = NO_VOLUME;
		}
		
		//reset the envelopes
		if(channelPointer->volumeEnvelope.enabled & 0x80){
			channelPointer->volumeEnvelope.enabled = (channelPointer->volumeEnvelope.enabled & 0x7e) | (channelPointer->instrumentPointer->volEnvelope.flags & 1);
		}
		else{
			channelPointer->volumeEnvelope.enabled = channelPointer->instrumentPointer->volEnvelope.flags & 1;
		}
		if(channelPointer->pitchEnvelope.enabled & 0x80){
			channelPointer->pitchEnvelope.enabled = (channelPointer->pitchEnvelope.enabled & 0x7e) | (channelPointer->instrumentPointer->pitchEnvelope.flags & 1);
		}
		else{
			channelPointer->pitchEnvelope.enabled = channelPointer->instrumentPointer->pitchEnvelope.flags & 1;
		}
		if(channelPointer->panningEnvelope.enabled & 0x80){
			channelPointer->panningEnvelope.enabled = (channelPointer->panningEnvelope.enabled & 0x7e) | (channelPointer->instrumentPointer->panEnvelope.flags & 1);
		}
		else{
			channelPointer->panningEnvelope.enabled = channelPointer->instrumentPointer->panEnvelope.flags & 1;
		}
		
		//figure out the panning
		//if there was a pan effect on this row
		if(channelPointer->channelPan & 0x80){
			channelPointer->currentPanning = channelPointer->channelPan & 0x7f;
			channelPointer->channelPan &= 0x7f;
		}
		//if the sample has a default pan
		else if(channelPointer->samplePointer->defaultPan & 0x80){
			channelPointer->currentPanning = channelPointer->samplePointer->defaultPan & 0x7f;
		}
		//if the instrument has a default pan
		else if((channelPointer->instrumentPointer->defaultPan & 0x80) == 0){
			channelPointer->currentPanning = channelPointer->instrumentPointer->defaultPan & 0x7f;
		}
		//if the instrument has an enabled panning envelope. 
		else if((channelPointer->panningEnvelope.enabled & 2) || ((channelPointer->panningEnvelope.enabled & 1) && !(channelPointer->panningEnvelope.enabled & 4))){
			//mark the currentPan as using the panning envelope
			channelPointer->currentPanning = 0x80 | channelPointer->channelPan;
		}
		//if none of the above are true, use the channel pan
		else{
			channelPointer->currentPanning = channelPointer->channelPan;
		}
		
		//handle auto-vibrato
		u8 wave = channelPointer->samplePointer->vibratoWave;
		if(wave == 4){
			channelPointer->autoVibrato.waveformType = 8;
		}
		channelPointer->autoVibrato.state = NO_VIBRATO;
		channelPointer->autoVibrato.sweepProgress = 0;
		
		if((channelPointer->samplePointer->vibratoDepth != 0) && (channelPointer->samplePointer->vibratoSpeed != 0)){
			channelPointer->autoVibrato.state = TRIGGER_VIBRATO;
			channelPointer->autoVibrato.depth = channelPointer->samplePointer->vibratoDepth >> 1;
			channelPointer->autoVibrato.speed = channelPointer->samplePointer->vibratoSpeed;
			channelPointer->autoVibrato.sweep = channelPointer->samplePointer->vibratoSweep;
		}
	}
	else if(channelPointer->triggerState == CANCEL_TRIGGER){
		channelPointer->triggerState = NO_TRIGGER;
	}
}

void processNote(CurrentChannelSettings *channelPointer, CurrentSoundSettings *assetPointer){
	//if it is a regular note
	if(channelPointer->currentBasicSettings.note <= 119){
		channelPointer->instrumentPointer = &assetPointer->asset->instruments[channelPointer->currentBasicSettings.instrument - 1];
		channelPointer->samplePointer = sampleList[assetPointer->asset->samples[channelPointer->instrumentPointer->keyboardSample[channelPointer->currentBasicSettings.note]]];
		channelPointer->notePitch = (channelPointer->samplePointer->middleCPitch * pitchTable[channelPointer->currentBasicSettings.note]) >> 14;
		if(channelPointer->triggerState != CANCEL_TRIGGER){
			channelPointer->triggerState = TRIGGER;
		}
	}
	//if it is a note cut command
	else if(channelPointer->currentBasicSettings.note == 254){
		channelPointer->noteState = NO_NOTE;
	}
	//if it is a note end
	else if(channelPointer->currentBasicSettings.note == 255){
		channelPointer->noteState = PLAY_NOTE;
	}
	//if it is a note fadeout
	else{
		channelPointer->noteState = FADEOUT_NOTE;
	}
	channelPointer->previousBasicSettings.instrument = channelPointer->currentBasicSettings.instrument;
	channelPointer->previousBasicSettings.note = channelPointer->currentBasicSettings.note;
	channelPointer->pitchState = NO_PITCH;
}

s8 processEnvelope(ChannelEnvelopeSettings *envelopeVariables, EnvelopeData *envelopeData, enum NoteState state){
	//if we need to initialize the envelope
	if(state == FADEOUT_NOTE){
		return envelopeVariables->currentYPos;
	}
	else if(state == TRIGGER_TICK_NOTE){
		envelopeVariables->currentNode = &envelopeData->envelopeNodes[0];
		envelopeVariables->nextNode = &envelopeData->envelopeNodes[1];
		envelopeVariables->currentXPos = 0;
		envelopeVariables->currentYPos = envelopeVariables->currentNode->nodeYPosition;
		envelopeVariables->YPosError = 0;
		envelopeVariables->currentNodeIndex = 0;
	}
	//check if the current tick position is at the sustain loop node, and instrument is sustaining
	else if((envelopeVariables->currentXPos == envelopeData->envelopeNodes[envelopeData->sustainEnd].nodeXPosition) && (state == SUSTAIN_NOTE) && (envelopeData->flags & 4)){
		envelopeVariables->currentNode = &envelopeData->envelopeNodes[envelopeData->sustainBegin];
		envelopeVariables->nextNode = &envelopeData->envelopeNodes[envelopeData->sustainBegin + 1];
		envelopeVariables->currentXPos = envelopeVariables->currentNode->nodeXPosition;
		envelopeVariables->currentYPos = envelopeVariables->currentNode->nodeYPosition;
		envelopeVariables->YPosError = 0;
		envelopeVariables->currentNodeIndex = envelopeData->sustainBegin;
	}
	//check if the current tick position is at the loop node
	else if((envelopeVariables->currentXPos == envelopeData->envelopeNodes[envelopeData->loopEnd].nodeXPosition) && (envelopeData->flags & 2)){
		envelopeVariables->currentNode = &envelopeData->envelopeNodes[envelopeData->loopBegin];
		envelopeVariables->nextNode = &envelopeData->envelopeNodes[envelopeData->loopBegin + 1];
		envelopeVariables->currentXPos = envelopeVariables->currentNode->nodeXPosition;
		envelopeVariables->currentYPos = envelopeVariables->currentNode->nodeYPosition;
		envelopeVariables->YPosError = 0;
		envelopeVariables->currentNodeIndex = envelopeData->loopBegin;
	}
	//if the current tick position is at the final node
	else if(envelopeVariables->currentXPos == envelopeData->envelopeNodes[envelopeData->nodeCount - 1].nodeXPosition){
		envelopeVariables->currentYPos = envelopeData->envelopeNodes[envelopeData->nodeCount - 1].nodeYPosition;
		envelopeVariables->currentNodeIndex = envelopeData->nodeCount - 1;
	}
	//if neither, increment the x position
	else{
		envelopeVariables->currentXPos++;
		//check if we have reached the next node
		if(envelopeVariables->currentXPos >= envelopeVariables->nextNode->nodeXPosition){
			envelopeVariables->currentNodeIndex++;
			envelopeVariables->currentNode = envelopeVariables->nextNode;
			envelopeVariables->nextNode = &envelopeData->envelopeNodes[envelopeVariables->currentNodeIndex + 1];
			envelopeVariables->currentYPos = envelopeVariables->currentNode->nodeYPosition;
			envelopeVariables->YPosError = 0;
		}
		//otherwise, interpolate the y position based on the node data
		else{
			s8 deltaY;
			u16 deltaX;
			u8 absDeltaY;
			s8 deltaYSign;
			deltaY = envelopeVariables->nextNode->nodeYPosition - envelopeVariables->currentNode->nodeYPosition;
			deltaX = envelopeVariables->nextNode->nodeXPosition - envelopeVariables->currentNode->nodeXPosition;
			if(deltaY < 0){
				absDeltaY = -deltaY;
				deltaYSign = -1;
			}
			else{
				absDeltaY = deltaY;
				deltaYSign = 1;
			}
			
			//if the slope is between -1 and 1, use DDA algorithm
			if(deltaX > absDeltaY){
				envelopeVariables->YPosError += absDeltaY;
				if(envelopeVariables->YPosError >= deltaX){
					envelopeVariables->currentYPos += deltaYSign;
					envelopeVariables->YPosError -= deltaX;
				}
			}
			//if the slope has a magnitude 1 or greater, use the interpolation formula LUT
			else{
				envelopeVariables->currentYPos = envelopeVariables->currentNode->nodeYPosition + (deltaY * envelopeInverseTable[deltaX] * (envelopeVariables->currentXPos - envelopeVariables->currentNode->nodeXPosition) >> 15);
			}
		}
	}
	return envelopeVariables->currentYPos;
}

void volumeSlideUp(CurrentChannelSettings *channelPointer, u8 commandAmount){
	if(commandAmount == 0){
		commandAmount = channelPointer->effectMemory.abcd;
	}
	else{
		channelPointer->effectMemory.abcd = commandAmount;
	}
	//apply the change
	channelPointer->currentVolume += commandAmount;
	//check bounds
	if(channelPointer->currentVolume >= 64){
		channelPointer->currentVolume = 64;
	}
}

void volumeSlideDown(CurrentChannelSettings *channelPointer, u8 commandAmount){
	if(commandAmount == 0){
		commandAmount = channelPointer->effectMemory.abcd;
	}
	else{
		channelPointer->effectMemory.abcd = commandAmount;
	}
	//check bounds
	if(channelPointer->currentVolume - commandAmount <= 0){
		channelPointer->currentVolume = 0;
	}
	else{
		channelPointer->currentVolume -= commandAmount;
	}
}

void portamentoDown(CurrentChannelSettings *channelPointer, u8 commandAmount){
	//if it is a "fine portamento down" command
	if(commandAmount >= 0xf0){
		commandAmount &= 0xf;
	}
	//if it is an "extra fine portamento down" command
	else if(commandAmount >= 0xe0){
		channelPointer->pitchModifier = (channelPointer->pitchModifier * extraFinePortamentoTable[15 - (commandAmount & 0xf)]) >> 12;
	}
	else{
		channelPointer->pitchModifier = (channelPointer->pitchModifier * portamentoTable[256 - commandAmount]) >> 12;
	}
}

void portamentoUp(CurrentChannelSettings *channelPointer, u8 commandAmount){
	//if it is a "fine portamento down" command
	if(commandAmount >= 0xf0){
		commandAmount &= 0xf;
	}
	//if it is an "extra fine portamento down" command
	else if(commandAmount >= 0xe0){
		channelPointer->pitchModifier = (channelPointer->pitchModifier * extraFinePortamentoTable[15 + (commandAmount & 0xf)]) >> 12;
	}
	else{
		channelPointer->pitchModifier = (channelPointer->pitchModifier * portamentoTable[256 + commandAmount]) >> 12;
	}
}

u8 portamentoCheck(u8 commandAmount){
	//if this effect happens for just one tick
	if(commandAmount >= 0xE0){
		return 1;
	}
	//if it happens for all ticks other than the first
	else{
		return 0;
	}
}

void initVibrato(VibratoSettings *vibratoPointer, u8 *effectMemory, u8 commandAmount, u8 sweep, u8 flags){
	if((commandAmount & 0xf) == 0){
		commandAmount  = (commandAmount & 0xf0) | (*effectMemory & 0xf);
	}
	else{
		*effectMemory = (*effectMemory & 0xf0) | (commandAmount & 0xf);
	}
	
	if((commandAmount & 0xf0) == 0){
		commandAmount  = (commandAmount & 0xf) | (*effectMemory & 0xf0);
	}
	else{
		*effectMemory = (*effectMemory & 0xf) | (commandAmount & 0xf0);
	}
	
	if((flags & 1) == 0){
		vibratoPointer->depth = (commandAmount & 0xf) << 4;
	}
	else{
		vibratoPointer->depth = (commandAmount & 0xf) << 2;
	}
	
	if((flags & 2) == 0){
		vibratoPointer->speed = (commandAmount & 0xf0) >> 2;
	}
	else{
		vibratoPointer->speed = (commandAmount & 0xf0) >> 4;
	}
	vibratoPointer->sweep = sweep;
	if(sweep == 0){
		vibratoPointer->sweepProgress = 256;
	}
	else{
		vibratoPointer->sweepProgress = 0;
	}
	
	if(vibratoPointer->state == NO_VIBRATO){
		vibratoPointer->state = TRIGGER_VIBRATO;
	}
	else if(vibratoPointer->state == PROCESSED_VIBRATO){
		vibratoPointer->state = PLAY_VIBRATO;
	}
}
s8 processVibrato(VibratoSettings *vibratoPointer){
	u8 retriggerWave = 0;
	if(vibratoPointer->state == NO_VIBRATO){
		return 0;
	}
	else if(vibratoPointer->state == PROCESSED_VIBRATO){
		vibratoPointer->state = NO_VIBRATO;
		return 0;
	}
	else if(vibratoPointer->state == TRIGGER_VIBRATO){
		retriggerWave = 1;
	}
	vibratoPointer->state = PLAY_VIBRATO;
	
	cu8 *waveformTable = sinWaveform;
	switch(vibratoPointer->waveformType){
	case 0:
		waveformTable = sinWaveform;
		if(retriggerWave){
			vibratoPointer->phase = 0;
		}
		break;
	case 1:
		waveformTable = rampDownWaveform;
		if(retriggerWave){
			vibratoPointer->phase = 0;
		}
		break;
	case 2:
		waveformTable = squareWaveform;
		if(retriggerWave){
			vibratoPointer->phase = 0;
		}
		break;
	case 3:
		waveformTable = randomWaveform;
		break;
	case 4:
		waveformTable = sinWaveform;
		break;
	case 5:
		waveformTable = rampDownWaveform;
		break;
	case 6:
		waveformTable = squareWaveform;
		break;
	case 7:
		waveformTable = randomWaveform;
		break;
	case 8:
		waveformTable = rampUpWaveform;
		if(retriggerWave){
			vibratoPointer->phase = 0;
		}
		break;
	}
	
	s8 amplitude;
	//get the amplitude from the selected waveform table
	amplitude = waveformTable[vibratoPointer->phase];
	
	if(vibratoPointer->phase + vibratoPointer->speed >= 256){
		vibratoPointer->phase = (vibratoPointer->phase - 256) + vibratoPointer->speed;
	}
	else{
		vibratoPointer->phase += vibratoPointer->speed;
	}
	
	//calculate the current amplitude of the vibrato
	amplitude = (amplitude * vibratoPointer->depth * vibratoPointer->sweepProgress) >> 16;
	
	//update the sweep
	if(vibratoPointer->sweepProgress + vibratoPointer->sweep >= 256){
		vibratoPointer->sweepProgress = 256;
	}
	else{
		vibratoPointer->sweepProgress += vibratoPointer->sweep;
	}
	
	return amplitude;
};
void tonePortamento(CurrentChannelSettings *channelPointer, u8 commandAmount){
	if(commandAmount == 0){
		commandAmount = channelPointer->effectMemory.Gg;
	}
	else{
		channelPointer->effectMemory.Gg = commandAmount;
	}

	//if we are sliding down
	if(channelPointer->currentPitch > channelPointer->notePitch){
		channelPointer->pitchModifier = (channelPointer->pitchModifier * portamentoTable[256 - commandAmount]) >> 12;
		//if we have reached the target note
		if(((channelPointer->currentPitch * channelPointer->pitchModifier) >> 12) <= channelPointer->notePitch){
			channelPointer->currentPitch = channelPointer->notePitch;
			channelPointer->pitchModifier = 0x1000;
		}
	}
	//if we are sliding up
	else{
		channelPointer->pitchModifier = (channelPointer->pitchModifier * portamentoTable[256 + commandAmount]) >> 12;
		//if we have reached the target note
		if(((channelPointer->currentPitch * channelPointer->pitchModifier) >> 12) >= channelPointer->notePitch){
			channelPointer->currentPitch = channelPointer->notePitch;
			channelPointer->pitchModifier = 0x1000;
		}
	}
}

void positionJump(CurrentSoundSettings *assetPointer){

	u8 nextOrder;
	u8 nextRow;
	//first, figure out which order we are jumping to
	if(assetPointer->globalEffects.B == 0xff){
		nextOrder = assetPointer->orderIndex + 1;
	}
	else{
		nextOrder = assetPointer->globalEffects.B;
	}
	//check if the jumped-to order is a valid one. If invalid, jump to 0
	if(nextOrder >= assetPointer->asset->ordersNum){
		nextOrder = 0;
	}
	//then, figure out which row we are jumping to
	if(assetPointer->globalEffects.C == 0xff){
		nextRow = 1;
	}
	//check if the new row number is bigger than the number of rows in the new order. If invalid, set to 0
	else if(assetPointer->globalEffects.C >= assetPointer->asset->patterns[assetPointer->asset->orders[nextOrder]].rowsNum){
		nextRow = 1;
	}
	else{
		nextRow = assetPointer->globalEffects.C + 1;
	}
	//set the new values
	assetPointer->orderIndex = nextOrder;
	assetPointer->rowNum = nextRow;

	//clear the global effects from the queue
	assetPointer->globalEffects.B = 0xff;
	assetPointer->globalEffects.C = 0xff;
}


void increaseTempo(u8 commandAmount, CurrentSoundSettings *assetPointer){
	//do bounds checking
	if((assetPointer->currentTempo + commandAmount) > 0xff){
		assetPointer->currentTempo = 0xff;
	}
	else{
		assetPointer->currentTempo = assetPointer->currentTempo + commandAmount;
	}
}
void decreaseTempo(u8 commandAmount, CurrentSoundSettings *assetPointer){
	//do bounds checking
	if(assetPointer->currentTempo - commandAmount < 0x20){
		assetPointer->currentTempo = 0x20;
	}
	else{
		assetPointer->currentTempo = assetPointer->currentTempo - commandAmount;
	}
}
void volumeSlide(u8 *currentVolume, u8 *effectMemory, u8 maxVolume, u8 commandAmount){
	//consult the effect memory
	if(commandAmount == 0){
		commandAmount = *effectMemory;
	}
	else{
		*effectMemory = commandAmount;
	}
	//if it is a "volume slide up" 
	if(((commandAmount & 0xf) == 0x0) || (((commandAmount & 0xf) == 0xf) && ((commandAmount & 0xf0) != 0x0))) {
		commandAmount = (commandAmount & 0xf0) >> 4;
		if(*currentVolume + commandAmount > maxVolume){
			*currentVolume = maxVolume;
		}
		else{
			*currentVolume += commandAmount;
		}
	}
	//if it is a "volume slide down"
	else{
		commandAmount = (commandAmount & 0xf);
		if(*currentVolume - commandAmount < 0){
			*currentVolume = 0;
		}
		else{
			*currentVolume -= commandAmount;
		}
	}
}

u8 slideCheck(u8 *effectMemory, u8 commandAmount){
	//consult the effect memory
	if(commandAmount == 0){
		commandAmount = *effectMemory;
	}
	else{
		*effectMemory = commandAmount;
	}
	//if it is a regular "volume slide" return 0
	if(((commandAmount & 0xf) == 0x0) || ((commandAmount & 0xf0) == 0x0)){
		return 0;
	}
	else{
		return 1;
	}
}
void panningSlide(u8 *currentPanning, u8 *effectMemory, u8 commandAmount){
	//consult the effect memory
	if(commandAmount == 0){
		commandAmount = *effectMemory;
	}
	else{
		*effectMemory = commandAmount;
	}
	//if it is a "panning slide up"
	if(((commandAmount & 0xf) == 0x0) || (((commandAmount & 0xf) == 0xf) && ((commandAmount & 0xf0) != 0x0))) {
		commandAmount = (commandAmount & 0xf0) >> 4;
		if(*currentPanning + commandAmount > 64){
			*currentPanning = 64;
		}
		else{
			*currentPanning += commandAmount;
		}
	}
	//if it is a "panning slide down"
	else{
		commandAmount = (commandAmount & 0xf);
		if(*currentPanning - commandAmount < 0){
			*currentPanning = 0;
		}
		else{
			*currentPanning -= commandAmount;
		}
	}
}
void retrigger(CurrentChannelSettings *channelPointer, u8 commandAmount){
	if(commandAmount == 0){
		commandAmount = channelPointer->effectMemory.Q;
	}
	channelPointer->triggerState = TRIGGER;
	channelPointer->volumeState = PROCESSED_VOLUME;
	channelPointer->periodicEffectTimer = (commandAmount & 0xf) - 1;
	commandAmount = (commandAmount & 0xf0) >> 4;
	switch(commandAmount){
	case 1:
		if((channelPointer->currentVolume - 1) < 0){
			channelPointer->currentVolume = 0;
		}
		else{
			channelPointer->currentVolume -= 1;
		}
		break;
	case 2:
		if((channelPointer->currentVolume - 2) < 0){
			channelPointer->currentVolume = 0;
		}
		else{
			channelPointer->currentVolume -= 2;
		}
		break;
	case 3:
		if((channelPointer->currentVolume - 4) < 0){
			channelPointer->currentVolume = 0;
		}
		else
		{
			channelPointer->currentVolume -= 4;
		}
		break;
	case 4:
		if((channelPointer->currentVolume - 8) < 0){
			channelPointer->currentVolume = 0;
		}
		else{
			channelPointer->currentVolume -= 8;
		}
		break;
	case 5:
		if((channelPointer->currentVolume - 16) < 0){
			channelPointer->currentVolume = 0;
		}
		else{
			channelPointer->currentVolume -= 16;
		}
		break;
	case 6:
		channelPointer->currentVolume = (channelPointer->currentVolume * 171) >> 8;
		break;
	case 7:
		channelPointer->currentVolume >>= 1;
		break;
	case 9:
		if(channelPointer->currentVolume + 1 > 0x80){
			channelPointer->currentVolume = 0x80;
		}
		else{
			channelPointer->currentVolume += 1;
		}
		break;
	case 10:
		if(channelPointer->currentVolume + 2 > 0x80){
			channelPointer->currentVolume = 0x80;
		}
		else{
			channelPointer->currentVolume += 2;
		}
		break;
	case 11:
		if(channelPointer->currentVolume + 4 > 0x80){
			channelPointer->currentVolume = 0x80;
		}
		else{
			channelPointer->currentVolume += 4;
		}
		break;
	case 12:
		if(channelPointer->currentVolume + 8 > 0x80){
			channelPointer->currentVolume = 0x80;
		}
		else{
			channelPointer->currentVolume += 8;
		}
		break;
	case 13:
		if(channelPointer->currentVolume + 16 > 0x80){
			channelPointer->currentVolume = 0x80;
		}
		else{
			channelPointer->currentVolume += 16;
		}
		break;
	case 14:
		if(((channelPointer->currentVolume * 3) >> 1) > 0x80){
			channelPointer->currentVolume = 0x80;
		}
		else{
			channelPointer->currentVolume = ((channelPointer->currentVolume * 3) >> 1);
		}
		break;
	case 15:
		if(channelPointer->currentVolume << 1 > 0x80){
			channelPointer->currentVolume = 0x80;
		}
		else{
			channelPointer->currentVolume <<= 1;
		}
		break;
	}
}
void arpeggio(CurrentChannelSettings *channelPointer, u8 commandAmount){
	if(commandAmount == 0){
		commandAmount = channelPointer->effectMemory.J;
	}
	if(channelPointer->periodicEffectState == ARPEGGIO_0){
		channelPointer->periodicEffectState = ARPEGGIO_1;
		channelPointer->pitchModifier = (channelPointer->pitchModifier * arpeggioTable[15 + ((commandAmount & 0xf0) >> 4)]) >> 12;
	}
	else if(channelPointer->periodicEffectState == ARPEGGIO_1){
		channelPointer->periodicEffectState = ARPEGGIO_2;
		channelPointer->pitchModifier = (channelPointer->pitchModifier * arpeggioTable[15 + (commandAmount & 0xf) - ((commandAmount & 0xf0) >> 4)]) >> 12;
	}
	else{
		channelPointer->periodicEffectState = ARPEGGIO_0;
		channelPointer->pitchModifier = (channelPointer->pitchModifier * arpeggioTable[15 - (commandAmount & 0xf)]) >> 12;
	}
}












//let me scroll down please