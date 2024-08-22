# FurnITure_Audio_Engine
 Furnace Tracker and Impulse Tracker audio driver and library for the GameBoy Advance


Musician instructions can be found here: 
https://docs.google.com/document/d/1ZT2lGmSP19G2NgaRjupNCrc74Q_20axVvvTIgWhEo5I/edit?usp=sharing


Programmer instructions:
place any .IT files into resources->sounds, then run scripts->conv_audio. Processed 
sound data will be found in the three folders within data->audio. These files need to be
compiled into your project.

Any file that uses the audio engine will need to #include "audio_engine_external.h"

Read the comments in that file to learn how to initialize the engine and how to play sounds with it.


Logo and sample audio by PotatoTeto