#ifndef audio_engine_internalh
#define audio_engine_internalh

//includes
#include "tonc.h"
#include "audio_engine_external.h"

//constants

#define MAX_SAMPLES_IN_ONE_FRAME 560
#define SYNC_ERROR_PER_FRAME 37
#define SYNC_PERIOD_FRAMES 128
//Sound DMA's per frame = 34.2890625 or (34 + 37/128) or 548.625 samples
//in the case where 35 sound DMA's happen in a frame, that is 16 * 25 = 560 samples transferred in one frame
#define CYCLES_PER_SAMPLE 512
#define PITCH_FP 12
#define VOLUME_FP 7
#define SAMPLE_FP 8
#define INTERPOLATION_FP 8
#define MAX_SAMPLES_AT_ONCE (MAX_SAMPLE_MIXED_AT_ONCE & ~3)

//enums
enum NoteState{
	NO_NOTE, TRIGGER_TICK_NOTE, SUSTAIN_NOTE, PLAY_NOTE, FADEOUT_NOTE
};

enum TriggerState{
	NO_TRIGGER, TRIGGER, DELAY_TRIGGER, CANCEL_TRIGGER
};

enum PitchState{
	NO_PITCH, NEW_PITCH
};

enum EffectState{
	NO_EFFECT, TRIGGER_TICK_EFFECT, EVERY_TICK_EFFECT, COUNTER_EFFECT
};

enum VolumeState{
	NO_VOLUME, TRIGGER_TICK_VOLUME, EVERY_TICK_VOLUME, PROCESSED_VOLUME
};

enum VibratoState{
	NO_VIBRATO, TRIGGER_VIBRATO, PLAY_VIBRATO, PROCESSED_VIBRATO
};

enum PeriodicEffectState{
	NO_PERIODIC_EFFECT, TREMOR_ON, TREMOR_OFF, ARPEGGIO_0, ARPEGGIO_1, ARPEGGIO_2
};

//structs

typedef const struct AudioSample{
	cu8 *sampleStart; //a pointer to the raw audio data of this sample. 
	cu32 sampleLength; //the index of the last sample
	cu32 loopEnd; //the index where this sample needs to loop + 1.
	cu32 loopStart; //the index where the looping sample needs to resume from
	cu32 sustainLoopEnd; //the index where the sustain loop ends + 1
	cu32 sustainLoopStart; //the index where the sustain loop begins
	cu8 sampleType; //bit 1: loop, bit 2: sustain loop, bit 3: ping-pong loop, bit 4: ping-pong sustain loop,
	cu8 globalVolume; //the global volume of this sample
	cu8 defaultVolume; //the default volume of this sample
	cu8 defaultPan; //default pan of this sample
	cu32 middleCPitch; //the frequency where this sample plays a middle C
	cu8 vibratoSpeed; 
	cu8 vibratoDepth;
	cu8 vibratoWave;
	cu8 vibratoSweep;
}AudioSample;

typedef struct ChannelData{
	AudioSample *samplePtr;//a pointer to the struct of the currently-playing sample
	u32 sampleIndex;//20.12 bit fixed point index into sample.
	u32 pitch;//20.12 fixed point
	u8 state;//0: disabled, 1: no loop, moving right, until sampleLength 2: no loop, moving left until sustain loop start,
	//3: looping, moving right, until loop end 4: ping-pong looping, moving right until loop end, 5: ping-pong looping, moving left until loop start
	//6: looping, moving left until sustain loop start, 7: sustain looping, moving right, until sustain loop end,
	//8: sustain ping-pong looping, moving right, until sustain loop end, 9: sustain ping-pong looping, moving left, until sustain loop start
	u8 leftVolume;//0.8 fixed point volume control for left channel. 0xff is highest value.
	u8 rightVolume;//0.8 fixed point volume control for right channel. 0xff is highest value.
	u8 padding;
}ChannelData;

typedef const struct EnvelopeNode{
	cs8 nodeYPosition;
	cu16 nodeXPosition;
}EnvelopeNode;

typedef const struct EnvelopeData{
	cu8 flags;
	cu8 nodeCount;
	cu8 loopBegin;
	cu8 loopEnd;
	cu8 sustainBegin;
	cu8 sustainEnd;
	EnvelopeNode envelopeNodes[25];
}EnvelopeData;

typedef const struct InstrumentData{
	cu8 fadeOut;
	cs8 pitPanSeparation;
	cu8 pitPanCenter;
	cu8 globalVolume;
	cu8 defaultPan;
	cu8 randomVolVariation;
	cu8 randomPanVariation;
	cu8 keyboardSample[120];
	EnvelopeData volEnvelope;
	EnvelopeData pitchEnvelope;
	EnvelopeData panEnvelope;
}InstrumentData;

typedef const struct PatternData{
	cu16 rowsNum; //how many rows are in the pattern once it is unpacked
	cu8 *packedPatternData; //the packed data of this pattern
}PatternData;

typedef const struct AssetData{
	cu8 *orders;
	InstrumentData *instruments;
	cu16 *samples;
	PatternData *patterns;
	cu16 ordersNum;
	cu16 instrumentsNum;
	cu16 samplesNum;
	cu16 patternsNum;
	cu8 assetName[27];
	cu8 initGlobalVol;
	cu8 initTickSpeed;
	cu8 initTempo;
	cu8 *assetPriority;
	cu8 *assetChannelPriority;
	cu8 initChannelVol[MAX_DMA_CHANNELS];
	cu8 initChannelPan[MAX_DMA_CHANNELS];
}AssetData;

typedef struct AssetGlobalEffects{
	u8 A; //the command value of a speed change, if any
	u8 B; //the command value of a position jump, if any
	u8 C; //the command value of a pattern break, if any
	u8 SE; //the counter for how many times to repeat row
}AssetGlobalEffects;

typedef struct ChannelBasicSettings{
	u8 instrument; //ID of an instrument
	u8 volume; //volume setting
	u8 note; //note setting, 0 - 119
	u8 effect; //effect letter setting
	u8 effectValue; //effect number setting
}ChannelBasicSettings;

typedef struct ChannelEnvelopeSettings{
	EnvelopeNode *currentNode; //pointer to the current node
	EnvelopeNode *nextNode; //pointer to the next node
	u8 currentNodeIndex; //the index of the current node
	u16 currentXPos; //the current x position within the envelope
	u8 currentYPos; //stores the current Y position in case the DDA method is used
	u8 YPosError; //the accumulated error of the DDA algorithm
	u8 enabled; //bit one if the instrument has the envelope enabled, bit 2 is enable override, bit 3 is disable override
}ChannelEnvelopeSettings;

typedef struct MemorySettings{
	u8 D;
	u8 EeFf;
	u8 Gg;
	u8 HUh;
	u8 I;
	u8 J;
	u8 K;
	u8 L;
	u8 N;
	u8 O;
	u8 P;
	u8 Q;
	u8 R;
	u8 SB0; //the row number of a pattern loop
	u8 SBx; //the counter for how many times to repeat loop
	u8 T;
	u8 W;
	u8 Y;
	u8 abcd;
}MemorySettings;

typedef struct VibratoSettings{
	u8 waveformType;
	u8 depth;
	u8 speed;
	u8 sweep;
	u16 sweepProgress;
	u8 phase;
	enum VibratoState state; 
}VibratoSettings;

typedef struct CurrentChannelSettings{
	ChannelBasicSettings previousBasicSettings; //the settings that will be used if the pattern says to reuse them.
	ChannelBasicSettings currentBasicSettings; //the settings that currently control the sound output
	ChannelEnvelopeSettings volumeEnvelope; //the state of the volume envelope
	ChannelEnvelopeSettings pitchEnvelope; //the state of the pitch envelope
	ChannelEnvelopeSettings panningEnvelope; //the state of the panning envelope
	VibratoSettings vibrato; //the state of the vibrato
	VibratoSettings tremolo; //the state of the tremolo
	VibratoSettings panbrello; //the state of the panbrello
	VibratoSettings autoVibrato; //the state of the sample's auto-vibrato
	MemorySettings effectMemory; //the previously-used settings for each individual effect.
	InstrumentData *instrumentPointer; //pointer to the instrument struct that is currently playing
	AudioSample *samplePointer; //pointer to the sample that is currently playing
	u8 currentVolume; //the volume remembered for next frame
	s16 noteFadeComponent; //the current fadeout component of this note
	u8 channelVolume; //the volume of this channel
	u8 currentPanning; //the panning remembered for next frame
	u8 channelPan; //the pan setting of this channel
	s16 currentPitch; //the pitch remembered for next frame
	u16 notePitch; //the pitch associated with the note currently played
	u16 pitchModifier; //the multiplier of pitch from any portamento effects.
	u8 maskVariable; //last mask variable used for this channel
	u16 offset; //the offset of a note played this tick
	u8 periodicEffectTimer; //the timer for an effect currently running on this channel (tremor, arpeggio, retrigger, note cut, note delay)
	enum PeriodicEffectState periodicEffectState; //state of the current periodic effect on this channel (if any)
	enum NoteState noteState; //state of the note currently playing
	enum VolumeState volumeState; //state of the volume command on this row
	enum EffectState effectState; //state of the effect command on this row
	enum TriggerState triggerState; //weather a note is triggered this tick
	enum PitchState pitchState; //weather a new pitch command was on this row
	u8 priority; //the priority of this channel. Higher number means higher priority.
	u8 channelIndex; //the index that this channel's results get sent to. 0xff means it is currently not allocated a channel
}CurrentChannelSettings;

typedef struct CurrentSoundSettings{
	AssetData *asset; //pointer to the asset that is currently playing
	AssetGlobalEffects globalEffects; //memory for any global effects that need it
	u8 globalVolume; //the current global volume of the asset
	u16 assetID; //the index of the asset that is currently playing
	u8 rowNum; //the current row being processed
	u16 patternOffset; //the current offset into the current pattern being played
	u8 currentTickSpeed; //the number of ticks per row
	u8 tickCounter; //the number of ticks we have completed in this row
	u8 orderIndex; //the ID of the current order being processed
	u8 enabled; //0 if no asset is playing. 1 if a asset should be playing, 2 if a asset is paused
	u8 currentTempo; //the tempo of the asset
	u16 delayTicks; //the number of extra ticks to play this row
	u16 leftoverSamples; //the number of samples leftover in this tick after a frame is done processing
	u8 priority; //the priority of this asset. higher number means higher priority.
	u8 mixVolume; //program-controlled mix volume of each asset
	u8 volumeSlideAmount; //the magnitude to change volume by each tick
	u8 finalVolume; //the desired volume after a volume slide has concluded
	CurrentChannelSettings channelSettings[MAX_DMA_CHANNELS]; //the data for the channels of this asset
}CurrentSoundSettings;

//global variables
extern s8 soundBuffer1[MAX_SAMPLES_IN_ONE_FRAME * 2];
extern s8 soundBuffer2[MAX_SAMPLES_IN_ONE_FRAME * 2];
extern u8 audioTimer;
extern s8 audioError;
extern u8 audioProgress;

//external data
extern AudioSample *sampleList[];
extern AssetData *soundList[];
extern AssetData *sfxList[];
extern cu8 _sndAssetDefaultVol[];
extern cu16 numSamples;
extern cu16 numSounds;
extern cu16 pitchTable[];
extern cu16 envelopeInverseTable[];
extern cu16 tempoTable[];
extern cu8 tonePortamentoLUT[];
extern cu16 extraFinePortamentoTable[];
extern cu16 portamentoTable[];
extern cu8 sinWaveform[];
extern cu8 rampDownWaveform[];
extern cu8 squareWaveform[];
extern cu8 randomWaveform[];
extern cu8 rampUpWaveform[];
extern cu16 vibratoTable[];
extern cu16 arpeggioTable[];


//external functions
extern void mixAudio(ChannelData *, s8 *, u32, u32);


//internal functions
void processAssetTick(CurrentSoundSettings *, u8);
void nextRow(PatternData *, u16 *, CurrentSoundSettings *);
void applySettings(ChannelData *, AudioSample *, u32, u8, s8, enum NoteState *, u16);
void processEffects(CurrentChannelSettings *, CurrentSoundSettings *);
void processNote(CurrentChannelSettings *, CurrentSoundSettings *);
void processVolume(CurrentChannelSettings *);
void processTrigger(CurrentChannelSettings *, CurrentSoundSettings *);
s8 processEnvelope(ChannelEnvelopeSettings *, EnvelopeData *, enum NoteState);
void volumeSlideUp(CurrentChannelSettings *, u8);
void volumeSlideDown(CurrentChannelSettings *, u8);
void portamentoDown(CurrentChannelSettings *, u8);
void portamentoUp(CurrentChannelSettings *, u8);
u8 portamentoCheck(u8);
void initVibrato(VibratoSettings *, u8*, u8, u8, u8);
s8 processVibrato(VibratoSettings *);
void tonePortamento(CurrentChannelSettings *, u8);
void positionJump(CurrentSoundSettings *);
void increaseTempo(u8, CurrentSoundSettings *);
void decreaseTempo(u8, CurrentSoundSettings *);
void patternLoop(CurrentChannelSettings *, u8);
void volumeSlide(u8 *, u8 *, u8, u8);
u8 slideCheck(u8 *, u8);
void tremor(CurrentChannelSettings *, u8);
void processSampledChannel(CurrentChannelSettings *, ChannelData *, CurrentSoundSettings *);
void panningSlide(u8 *, u8 *, u8);
void retrigger(CurrentChannelSettings *, u8);
void arpeggio(CurrentChannelSettings *, u8);
void allocateChannels();

#endif