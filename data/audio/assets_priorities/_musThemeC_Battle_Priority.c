#include "audio_engine_settings.h"
#include "tonc.h"

//type in the priority of this asset. Higher number means higher priority
cu8 _musThemeC_Battle_Priority = ALAYER_BGM_CHANNEL_PERSISTENT;

//type in the priority of each channel in this asset. Leave unused channels as blank, or set to 0
cu8 _musThemeC_Battle_ChannelPriority[MAX_DMA_CHANNELS] = { 
	ALAYER_BGM_CHANNEL_PERSISTENT - 4,			// Channel 1
	ALAYER_BGM_CHANNEL_PERSISTENT - 5,			// Channel 2
	ALAYER_BGM_CHANNEL_PERSISTENT - 6,			// Channel 3
	ALAYER_BGM_CHANNEL_PERSISTENT - 7,			// Channel 4
	ALAYER_UNALLOCATED,							// Channel 5
	ALAYER_UNALLOCATED,							// Channel 6
	ALAYER_UNALLOCATED,							// Channel 7
	ALAYER_UNALLOCATED,							// Channel 8
	ALAYER_UNALLOCATED,							// Channel 9
	ALAYER_UNALLOCATED,							// Channel 10
 };