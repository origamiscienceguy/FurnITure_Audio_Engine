#include "audio_engine_settings.h"
#include "tonc.h"

//type in the priority of this asset. Higher number means higher priority
cu8 _sfxShipExplodeM_Priority = ALAYER_SFX;

//type in the priority of each channel in this asset. Leave unused channels as blank, or set to 0
cu8 _sfxShipExplodeM_ChannelPriority[MAX_DMA_CHANNELS] = { 
	ALAYER_SFX,				// Channel 1
	ALAYER_UNALLOCATED,		// Channel 2
	ALAYER_UNALLOCATED,		// Channel 3
	ALAYER_UNALLOCATED,		// Channel 4
	ALAYER_UNALLOCATED,		// Channel 5
	ALAYER_UNALLOCATED,		// Channel 6
	ALAYER_UNALLOCATED,		// Channel 7
	ALAYER_UNALLOCATED,		// Channel 8
	ALAYER_UNALLOCATED,		// Channel 9
	ALAYER_UNALLOCATED		// Channel 10
 };