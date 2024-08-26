#ifndef audio_engine_externalh
#define audio_engine_externalh

#include "audio_engine_settings.h"
#include "audio_assets_enum.h"

//processAudio must be ran once per frame, and must finish before the timer 1 interrupt fires.
extern void processAudio();

//before calling this function, set audioVcountISR() to run upon a vcount interrupt, and void timer1ISR() to run upon a timer 1 interrupt.
//this function will enable a vcount interrupt just once. Two frames after this function is invoked, it is safe to use Vcount interrupts again.
//Timer 0, Timer 1, DMA 1, DMA 2, and all the sound registers must not be touched from now until the engine is disabled.
extern void audioInitialize();

//will add a new song to the queue if it's priority is higher than another songs in the queue.
//Return value is the index into the current song array.
extern u8 playNewSound(u16 assetName);

//will remove an asset from the queue. Can be called at any time by the programmer, or called automatically after an asset reaches the end.
//returns the number of assets still playing after this one is removed
extern u8 endSound(u8 assetIndex);

//will remove all assets from the queue. Can be called at any time by the programmer, or called automatically after an asset reaches the end.
//returns the number of assets still playing after this one is removed (always 0)
extern u8 endAllSound();

//will temporarilly stop playing a song in the queue. It will still maintain it's position in the queue.
extern void pauseAsset(u8 assetIndex);

//will resume a paused asset.
extern void resumeAsset(u8 assetIndex);

//will gradually increase the volume of a currently playing asset
extern void volumeSlideAsset(u8 assetIndex, u8 volumePerTick, u8 finalVolume);

//sets the mixing volume of an asset 0-255
extern void setAssetVolume(u8 assetIndex, u8 volume);

//gets the default volume of an asset 0-255
extern u8 getAssetDefaultVolume(u8 soundIndex);

//gets the mix volume of an asset 0-255
extern u8 getAssetMixVolume(u8 soundIndex);

//skips asset1 to the same order, row, and tick of asset 2
extern void syncAsset(u8 assetIndex1, u8 assetIndex2);

//returns 1 if the specified asset is playing at the specified index
extern u8 isSoundPlaying(u16 assetName, u8 assetIndex);

//runs once upon engine initialization. 
extern void audioVcountISR();

//when audio engine is on, this must be ran within 1024 cycles of a timer1 interrupt firing
extern void audioTimer1ISR();
#endif