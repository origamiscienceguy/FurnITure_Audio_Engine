#include "audio_engine_settings.h"
#include "tonc.h"

//type in the priority of this asset. Higher number means higher priority
cu8 _musTitle_Priority = ALAYER_BGM_CHANNEL_PERSISTENT;

//type in the priority of each channel in this asset. Leave unused channels as blank, or set to 0
cu8 _musTitle_ChannelPriority[MAX_DMA_CHANNELS] = { 
	ALAYER_BGM_CHANNEL_PERSISTENT,				// Channel 1
	ALAYER_BGM_CHANNEL_PERSISTENT - 1,			// Channel 2
	ALAYER_BGM_CHANNEL_PERSISTENT - 2,			// Channel 3
	ALAYER_BGM_CHANNEL_PERSISTENT - 3,			// Channel 4
	ALAYER_BGM_CHANNEL_PERSISTENT - 4,			// Channel 5
	ALAYER_BGM_CHANNEL_PERSISTENT - 5,			// Channel 6
	ALAYER_BGM_CHANNEL_PERSISTENT - 7,			// Channel 7
	ALAYER_BGM_CHANNEL_PERSISTENT - 8,			// Channel 8
	ALAYER_BGM_CUTOFF,							// Channel 9
	ALAYER_BGM_CUTOFF - 1						// Channel 10
 };