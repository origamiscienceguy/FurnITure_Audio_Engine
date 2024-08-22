#include "audio_engine_settings.h"
#include "tonc.h"

//type in the priority of this asset. Higher number means higher priority
cu8 _sfxMenuConfirmC_Priority = ALAYER_SFX;

//type in the priority of each channel in this asset. Leave unused channels as blank, or set to 0
cu8 _sfxMenuConfirmC_ChannelPriority[MAX_DMA_CHANNELS] = { 
	ALAYER_SFX,				// Channel 1
	ALAYER_SFX - 1,			// Channel 2
	ALAYER_SFX - 2,			// Channel 3
	ALAYER_SFX - 3,			// Channel 4
	ALAYER_SFX - 4,			// Channel 5
	ALAYER_SFX - 5,			// Channel 6
	ALAYER_UNALLOCATED,		// Channel 7
	ALAYER_UNALLOCATED,		// Channel 8
	ALAYER_UNALLOCATED,		// Channel 9
	ALAYER_UNALLOCATED		// Channel 10
 };