.section .iwram,"ax", %progbits
    .align  2
    .code   32
	.global mixAudio
	.type mixAudio STT_FUNC

#include "audio_engine_settings.h"

@register usage on entry:
	@r0: pointer to array of structs containing data for each channel
	@r1: pointer to audio buffer to write to
	@r2: number of samples needed this frame
	@r3: left volume accumulator / number of channels
	@r4: right volume accumulator
	@r5: channel counter
	@r6: 
	@r7: 
	@r8:
	@r9: 
	@r10: scratch
	@r11: scratch
	@r12: scratch
	@r14: scratch
	
@channel data struct:
	@4 bytes: sample struct pointer
	@4 bytes: current sample index
	@4 bytes: pitch
	@1 byte: state
	@1 byte: left volume
	@1 byte: right volume
	@1 bytes: padding
	
@sample struct
	@4 bytes: pointer to start of sample data
	@4 bytes: sample length
	@4 bytes: sample after loop sample end
	@4 bytes: sample at loop start
	@4 bytes: sample after sustain loop sample end
	@4 bytes: sample at sustain loop start
	
mixAudio:
		@entry point is here, initialize values, save some values to the stack
	cmp		r2, #0								@check if zero samples are requested
	bxeq	lr									@leave early if either is the case
	cmp		r3, #0								@check if zero channels are enabled
	beq		nothingPlaying						@branch to code that zeros out the buffer
	push	{r4-r11, r14}						@save all the needed registers
	mul		r6, r3, r2							@multiply the number of samples needed by the number of channels needed
	sub		r13, r13, r6, lsl #1				@allocate space on the stack for the needed memory. numsamples * numchannels * 2 (stereo)
	push	{r1-r3}								@save the write pointer, the number of samples, and the number of channels
	sub		r5, r3, #1							@set up the channel counter register
	mov		r3, #0								@initialize the left volume accumulator to 0
	mov		r4, #0								@initialize the right volume accumulator to 0
	
processOneChannel:
		@load channel data, handle write pointer, and check if channel is enabled
	ldmia	r0!, {r6-r9}						@load all the data for this channel
	ldr		r14, [r13, #8]						@load the number of channels
	mul		r1, r2, r14							@multiply the number of channels by the number of samples needed
	add		r1, r13, r1, lsl #1					@set the write pointer to the top of the allocated stack space
	add		r1, r1, #12							@correct for the initial stack position
	sub		r1, r1, r14, lsl #2					@enter the bottom of these 4 samples
	add		r1, r1, r5, lsl #2					@arrive at the correct initial address
	ands	r12, r9, #0xff						@check if this channel is enabled, and move the state into r12
	beq		disabledChannel						@branch to the code that handles disabled channels
		@handle volume and volume accumulators, and sample starting pointer
	and		r10, r9, #0xff00					@isolate the left volume
	add		r3, r3, r10, lsr #8					@add the left volume to the left volume accumulator
	and		r11, r9, #0xff0000					@isolate the right volume
	add		r4, r4, r11, lsr #16				@add the right volume to the right volume accumulator
	push	{r0, r3-r6}							@push the channel pointer, volume accumulators, channel counter, and sample pointer
	mov		r3, r10, lsr #8						@put the left volume into register 3
	mov		r4, r11, lsr #16					@put the right volume into register 4
	ldr		r0, [r6]							@load the sample start ptr into r0
		@manage the state and set the correct target address
	adr		r10, stateMachine					@get the address for the state machine control
	mov		r12, r12, lsl #1					@prepare to load the state data
	ldrh	r10, [r10, r12]						@load the data for the current state
	orr		r10, r10, r12, lsl #15				@combine the current state data and the current state code
	push	{r0, r6, r10}						@store the sample start pointer, the sample pointer, and the current state data
	tst		r10, #0x8000						@check if we are moving left or right
	rsbne	r8, r8, #0							@negate the pitch if moving left
	and		r9, r10, #0x7000					@isolate the loop index
	ldr		r11, [r6, r9, lsr #10]				@load the loop index based on the state data
	add		r2, r0, r11, lsl #1					@add the loop index to the sample start ptr to get the target address
	addne	r2, r2, #0x80000000					@flip the top bit of the target address if moving left
	
		@setup the rest of the registers, pitch, accumulatr, etc.
	mov		r5, r7, lsl #20						@put the decimal portion of the current index at the top of the register
	mov		r7, r7, lsr #12						@isolate the whole number portion of the index
	add		r0, r0, r7, lsl #1					@add the starting index to the sample pointer
	mov		r6, r8, lsl #20						@put the decimal portion of pitch into the top of the register
	mov		r6, r6, lsr #20						@move it back to the bottom of the register
	orr		r6, r6, r14, lsl #24				@add the number of channels to the top of the same register
	mov		r8, r8, asr #12						@isolate the whole number portion of pitch
	movs	r7, r8, lsl #1						@put the whole number portion of pitch into the correct register
	mov		r8, #0xff							@setup the first part of the magic constant
	orr		r8, r8, #0xff0000					@complete the magic constant
	
pitchAndVolumeChannelNonUnity:
@register usage during pitchAndVolumeChannelNonUnityPitch:	
	@r0: pointer to sample data
	@r1: write pointer
	@r2: ending sample pointer
	@r3: left volume
	@r4: right volume
	@r5: decimal increment accumulator (top 12 bits) 
	@r6: write pointer update(top 8 bits) / index decimal increment (1.12 fixed point, positive or negative)
	@r7: index whole number increment (positive or negative)
	@r8: constant 0x00ff00ff
	@r9: scratch
	@r10: scratch
	@r11: scratch
	@r12: scratch
	@r14: scratch
	
pitchAndVolume4SamplesNonUnity:	
		@first sample 
	ldrh	r14, [r0], r7						@load the FIRST data from ROM, increment (or decrement) the pointer
	mov		r12, r5, lsr #24					@move the upper 8 bits of the decimal accumulator to the bottom of the register
	mov		r11, r14, lsr #8					@isolate the delta
	mul		r11, r12, r11						@multuply the delta by the decimal accumulator to interpolate
	add		r14, r14, r11, lsr #7				@add the interpolated amount to the sample value
	sub		r9, r14, r12						@undo the bias from using unsigned delta
	adds	r5, r5, r6, lsl #20					@add the pitch to the decimal accumulator
	addcs	r0, r0, #2							@if decimal accumulator overflowed, add 2 to the pointer
	subs	r12, r0, r2							@check if we reached a loop point
	blpl	loopHit								@branch to the loop-handling code if we have
firstSampleDone:
		@second sample
	ldrh	r14, [r0], r7						@load the SECOND data from ROM, increment (or decrement) the pointer
	mov		r12, r5, lsr #24					@move the upper 8 bits of the decimal accumulator to the bottom of the register
	mov		r11, r14, lsr #8					@isolate the delta
	mul		r11, r12, r11						@multuply the delta by the decimal accumulator to interpolate
	add		r14, r14, r11, lsr #7				@add the interpolated amount to the sample value
	sub		r10, r14, r12						@undo the bias from using unsigned delta
	adds	r5, r5, r6, lsl #20					@add the pitch to the decimal accumulator
	addcs	r0, r0, #2							@if decimal accumulator overflowed, add (or subtract) 2 from the pointer
	subs	r12, r0, r2							@check if we reached a loop point
	blpl	loopHit								@branch to the loop-handling code if we have
secondSampleDone:
		@third sample
	ldrh	r14, [r0], r7						@load the THIRD data from ROM, increment (or decrement) the pointer
	mov		r12, r5, lsr #24					@move the upper 8 bits of the decimal accumulator to the bottom of the register
	mov		r11, r14, lsr #8					@isolate the delta
	mul		r11, r12, r11						@multuply the delta by the decimal accumulator to interpolate
	add		r14, r14, r11, lsr #7				@add the interpolated amount to the sample value
	sub		r14, r14, r12						@undo the bias from using unsigned delta
	orr		r9, r9, r14, lsl #16				@combine the FIRST and THIRD results
	and		r9, r9, r8							@clear out the unwanted bits
	adds	r5, r5, r6, lsl #20					@add the pitch to the decimal accumulator
	addcs	r0, r0, #2							@if decimal accumulator overflowed, add (or subtract) 2 from the pointer
	subs	r12, r0, r2							@check if we reached a loop point
	blpl	loopHit								@branch to the loop-handling code if we have
thirdSampleDone:
		@fourth sample
	ldrh	r14, [r0], r7						@load the FOURTH data from ROM, increment (or decrement) the pointer
	mov		r12, r5, lsr #24					@move the upper 8 bits of the decimal accumulator to the bottom of the register
	mov		r11, r14, lsr #8					@isolate the delta
	mul		r11, r12, r11						@multuply the delta by the decimal accumulator to interpolate
	add		r14, r14, r11, lsr #7				@add the interpolated amount to the sample value
	sub		r14, r14, r12						@undo the bias from using unsigned delta
	orr		r10, r10, r14, lsl #16				@combine the SECOND and FOURTH results
	and		r10, r10, r8						@clear out the unwanted bits
	adds	r5, r5, r6, lsl #20					@add the pitch to the decimal accumulator
	addcs	r0, r0, #2							@if decimal accumulator overflowed, add (or subtract) 2 from the pointer
	subs	r12, r0, r2							@check if we reached a loop point
	blpl	loopHit								@branch to the loop-handling code if we have
fourthSampleDone:
		@apply left volume
	mul		r14, r9, r3							@multiply the FIRST and THIRD entries by the LEFT volume
	and		r14, r8, r14, lsr #8				@clear out the unwanted bits
	mul		r12, r10, r3						@multiply the SECOND and FOURTH entries by the LEFT volume
	and		r12, r8, r12, lsr #8				@clear out the unwanted bits
	orr		r14, r14, r12, lsl #8				@combine all four entries into one register
	str		r14, [r1], -r6, lsr #22				@store the four LEFT samples. Move the pointer to the RIGHT pointer
		@apply right volume
	mul		r14, r9, r4							@multiply the FIRST and THIRD entries by the RIGHT volume
	and		r14, r8, r14, lsr #8				@clear out the unwanted bits
	mul		r12, r10, r4						@multiply the SECOND and FOURTH entries by the RIGHT volume
	and		r12, r8, r12, lsr #8				@clear out the unwanted bits
	orr		r14, r14, r12, lsl #8				@combine all four entries into one register
	str		r14, [r1], -r6, lsr #22				@store the four RIGHT samples. Move the pointer to the next LEFT pointer
		@check if all samples are done
	add		r14, r13, #0x2c						@get the address of the start of the channel buffer
	cmp		r1, r14								@check if the current write pointer has passed through the channel buffer
	bhs		pitchAndVolume4SamplesNonUnity		@exit the loop if this channel is done being processed	
channelDone:
		@once we reach here, this channel is done, and it is time to move on to the next one
	mov		r1, r0								@save the load pointer
	mov		r6, r5								@save the decimal accumulator
	pop		{r0, r2, r10}						@load the state data the channel ended up on
	mov		r2, r0								@switch positions
	mov		r10, r10, lsr #16					@isolate the current state of this channel
	pop		{r0, r3-r5}							@load the channel pointer, the volume accumulators, and the channel counter
	strb	r10, [r0, #-4]						@store the current state into the channel data
	sub		r1, r1, r2							@get the number of samples we have traversed through
	mov		r1, r1, lsl #11						@move the whole number portion into position
	add		r1, r1, r6, lsr #20					@combine the index into one number
	str		r1, [r0, #-12]						@save the combined index
	add		r13, r13, #4						@move the stack pointer past the data that is no longer needed
	ldr		r2, [r13, #4]						@load the number of samples needed this frame
	subs	r5, r5, #1							@decrement the channel counter
	bpl		processOneChannel					@if there are still channels left to do, work on those
	
mixChannels:
@register usage during mix8Channels:	
	@r0: pointer to processed sample data
	@r1: write pointer
	@r2: counter
	@r3: bias correction value
	@r4: constant 0x80000000
	@r5: constant 0x00ff00ff
	@r6: other bias correction value
	@r7: number of channels
	@r8: odd samples accumulator
	@r9: even samples accumulator
	@r10: channel counter
	@r11: scratch
	@r12: scratch
	@r14: scratch
	
	ldr		r1, [r13]							@otherwise, prepare for mixing. Load the write pointer.
	ldr		r7, [r13, #8]						@load the number of channels
	mul		r11, r2, r7							@multiply the number of channels by the number of samples needed
	add		r11, r13, r11, lsl #1				@set the write pointer to the top of the allocated stack space
	add		r0, r11, #8							@correct for the initial stack position
	mov		r6, r4, lsl #15						@store the right bias correction value
	mov		r3, r3, lsl #15						@move the left bias correction to the correct place
	mov		r5, #0xff							@start a magic constant
	orr		r5, r5, #0xff0000					@finish the magic constant
	mov		r4, #0x80000000						@setup the other magic constant.
	
mix4Samples:
	mov		r8, #0								@reset the ODD sample accumulator
	mov		r9, #0								@reset the EVEN sample accumulator
	mov		r10, r7								@reset the number of channels counter
sum4Samples:
		@load and add together 4 samples from all channels
	ldr		r14, [r0], #-4						@load 4 samples from the current channel
	and		r11, r5, r14						@isolate the ODD samples
	add		r8, r8, r11							@add the ODD samples to the total
	and		r11, r5, r14, lsr #8				@isolate the EVEN samples
	add		r9, r9, r11							@add the EVEN samples to the total
	subs	r10, r10, #1						@check if all channels have been processed
	bne		sum4Samples							@repeat until all channels have been processed
	
		@correct the bias, handle clipping, and store the samples
	rsb		r14, r3, r8, lsl #16				@subtract the bias from the FIRST sample
	mov		r10, r14, lsl #8					@clear any potential overflow bits
	rsbs	r14, r14, r10, asr #8				@check if the result is the same when overflow bits are removed
	eorne	r10, r4, r14, asr #8				@clamp the result if it is out of bounds
	rsb		r14, r3, r8							@subtract the bias from the THIRD sample
	mov		r11, r14, lsl #8					@clear any potential overflow bits
	rsbs	r14, r14, r11, asr #8				@check if the result is the same when overflow bits are removed
	eorne	r11, r4, r14, asr #8				@clamp the result if it is out of bounds
	orr		r10, r10, r11, lsr #16				@combine the FIRST and THIRD samples
	and		r8, r5, r10, ror #24				@isolate the result and correct the order of the samples
	rsb		r14, r3, r9, lsl #16				@subtract the bias from the SECOND sample
	mov		r10, r14, lsl #8					@clear any potential overflow bits
	rsbs	r14, r14, r10, asr #8				@check if the result is the same when overflow bits are removed
	eorne	r10, r4, r14, asr #8				@clamp the result if it is out of bounds
	rsb		r14, r3, r9							@subtract the bias from the FOURTH sample
	mov		r11, r14, lsl #8					@clear any potential overflow bits
	rsbs	r14, r14, r11, asr #8				@check if the result is the same when overflow bits are removed
	eorne	r11, r4, r14, asr #8				@clamp the result if it is out of bounds
	orr		r10, r10, r11, lsr #16				@combine the SECOND and FOURTH samples
	and		r9, r5, r10, ror #24				@isolate the result and correct the order of the samples
	orr		r8, r8, r9, lsl #8					@combine all four samples
	str		r8, [r1], #4						@store the result

		@prepare for the next iteration
	sub		r0, r0, r7, lsl #2					@move the pointer to the next value
	subs	r2, r2, #4							@decrement the counter
	bne		mix4Samples							@repeat until all samples on this channel are done.
	movs	r3, r6								@switch out the right bias correction value
	bmi		mixingDone							@leave if we have done both channels
	mov		r6, #-1								@set the flag that next time we need to leave
	ldr		r1, [r13], #4						@set the write pointer
	add		r1, r1, #0x230						@move it to the other channels buffer
	ldr		r2, [r13]							@load the number of samples needed
	mul		r11, r2, r7							@multiply the number of channels by the number of samples needed
	add		r11, r13, r11, lsl #1				@set the write pointer to the top of the allocated stack space
	add		r0, r11, #4							@correct for the initial stack position
	sub		r0, r0, r7, lsl #2					@move the pointer to the other channel
	b		mix4Samples							@branch back in

mixingDone:
exitAudioMixer:
	ldr		r2, [r13], #8						@load the number of samples
	mul		r3, r2, r7							@number of channels * number of samples
	add		r13, r13, r3, lsl #1				@deallocate the buffer
	pop		{r4-r11, r14}						@restore all the registers that were saved when we entered this function
	bx		lr									@return execution
	
stateMachine:
@current section moving right[0] or left[1] (1 bit)
@current section target address (3 bits)
@padding (2 bits)
@overflow[0] or ping-pong[1] loop (1 bit)
@next section moving right[0] or left[1] (1 bit)
@next section target address (3 bits)
@next section state (5 bits)
.hword	0x0, 0x1000, 0xD221, 0x2043, 0x2365, 0xb244, 0xd243, 0x4087, 0x43a9, 0xd288

.align 2
disabledChannel:
@register usage during disabledChannel
	@r0: do not touch
	@r1: write pointer
	@r2: counter
	@r2-r9: do not touch
	@r10: constant 0
	@r11-r12: unused
	@r14: unused
	
	mov		r10, #0								@zero out r10
	mov		r11, r2								@move the number of samples into r11
zeroOut4Samples:	
	str		r10, [r1], -r14, lsl #2				@store zero into the LEFT buffer, and advance the pointer to the RIGHT buffer
	str		r10, [r1], -r14, lsl #2				@store zero into the RIGHT buffer, and advance the pointer to the next LEFT buffer
	subs	r11, r11, #4						@reduce the counter by 16
	bne		zeroOut4Samples						@repeat the loop until the counter becomes zero  20 cycles per loop, 280 loops, 2% of a frame
	subs	r5, r5, #1							@reduce the channel count
	bmi		mixChannels							@if there are no more channels left, move on to the miexer
	b		processOneChannel	
	
loopHit:
@register usage different from normal loop:
	@r2: target address of last segment
	@r12: whole number difference between current address and target address
	@r14: return address
		@this is the code that runs when a loop address has been hit
	ldr		r11, [r13, #8]						@load the current state and the state data
	tst		r11, #0x1f							@check if this sample should stop playing
	beq		sampleEndEarly						@branch to a zero-filling loop to finish this channel
	tst		r11, #0x200							@check if this is an overflow loop or a ping pong loop
	beq		overflowLoop						@branch if this is an overflow loop we encountered
pingPongLoop:
	tst		r11, #0x100							@check if the next section is moving left or right
	rsb		r7, r7, #0x0						@invert the whole number pitch
	sub		r7, r7, #0x2						@subtract two from the whole number pitch
	subne	r0, r0, #0x2						@subtract two from the sample pointer if we are moving left
	addeq	r0, r0, #0x2						@add two to the sample pointer if we are moving right
	sub		r0, r0, r12							@subtract any whole number overflow from the sample pointer
	rsb		r6, r6, #0x1000						@invert the decimal pitch
	eor		r6, r6, #0xff000000					@un-invert the channel count
	add		r6, r6, #0x1000000					@finish un-inverting the channel count
	bic		r0, r0, #0x80000000					@clear the top bit of the sample pointer
	rsb		r5, r5, #0x0						@invert the decimal sample pointer accumulator
	sub		r5, r5, #0x100000					@subtract one from the sample pointer accumulator
	and		r2, r11, #0xe0						@isolate the next target address index
	ldr		r12, [r13, #4]						@load the sample pointer
	ldr		r2, [r12, r2, lsr #3]				@load the relevant loop index
	ldr		r12, [r13]							@load the sample start address
	add		r2, r12, r2, lsl #1					@add the loop index to the sample start address
	orrne	r2, r2, #0x80000000					@set the top bit of the target address if we are moving left
	and		r11, r11, #0xf						@isolate the next state
	mov		r11, r11, lsl #1					@shift the next state up one
	adr		r12, stateMachine					@get the starting address of the state machine table
	ldrh	r12, [r12, r11]						@load the data for the next state
	orr		r12, r12, r11, lsl #15				@include the now current state into the data
	str		r12, [r13, #8]						@store the data for this new state
	mov		r15, r14							@return to main execution
	
overflowLoop:
	and		r11, r11, #0x7000					@isolate the target address index
	add		r11, r11, #0x1000					@add one to get the loop start index
	ldr		r0, [r13, #4]						@load the sample pointer
	ldr		r11, [r0, r11, lsr #10]				@load the loop start index
	ldr		r0, [r13]							@load the sample start address
	add		r0, r0, r11, lsl #1					@add the loop start to the sample start index
	add		r0, r0, r12							@add any potential extra overflow to the sample address
	mov		r15, r14							@return to main execution
	
sampleEndEarly:	@complete the last 4 potential samples
	adr		r11, fourthSampleDone
	subs	r11, r11, r14
	beq		applyFinalVolume
	subs	r11, r11, #(fourthSampleDone - thirdSampleDone)
	beq		zeroOutOne
	subs	r11, r11, #(thirdSampleDone - secondSampleDone)
	beq		zeroOutTwo
zeroOutThree:
	mov		r10, #0x80							@set the SECOND entry to zero
zeroOutTwo:
	orr		r9, r9, #0x800000					@set the THIRD entry to zero
zeroOutOne:
	orr		r10, r10, #0x800000					@set the FOURTH entry to zero	
applyFinalVolume:
	and		r9, r9, r8							@discard the unwanted bits
	and		r10, r10, r8						@discard the unwanted bits
		@apply left volume
	mul		r14, r9, r3							@multiply the FIRST and THIRD entries by the LEFT volume
	and		r14, r8, r14, lsr #8				@clear out the unwanted bits
	mul		r12, r10, r3						@multiply the SECOND and FOURTH entries by the LEFT volume
	and		r12, r8, r12, lsr #8				@clear out the unwanted bits
	orr		r14, r14, r12, lsl #8				@combine all four entries into one register
	str		r14, [r1], -r6, lsr #22				@store the four LEFT samples. Move the pointer to the RIGHT pointer
		@apply right volume
	mul		r14, r9, r4							@multiply the FIRST and THIRD entries by the RIGHT volume
	and		r14, r8, r14, lsr #8				@clear out the unwanted bits
	mul		r12, r10, r4						@multiply the SECOND and FOURTH entries by the RIGHT volume
	and		r12, r8, r12, lsr #8				@clear out the unwanted bits
	orr		r14, r14, r12, lsl #8				@combine all four entries into one register
	str		r14, [r1], -r6, lsr #22				@store the four RIGHT samples. Move the pointer to the next LEFT pointer
	add		r14, r13, #0x2c						@get the address of the start of the channel buffer
	cmp		r1, r14								@check if the current write pointer has passed through the channel buffer
	blo		remainderZeroed						@if we are done, exit
	mov		r12, #0x80							@fill r12 with 0x80
	orr		r12, r12, lsl #16					@cont.
	mul		r11, r12, r3						@multiply by the left volume
	and		r11, r8, r11, lsr #8				@clear out the unwanted bits
	orr		r11, r11, r11, lsl #8				@duplicate the results
	mul		r10, r12, r4						@multiply by the right volume
	and		r10, r8, r10, lsr #8				@clear out the unwanted bits
	orr		r10, r10, r10, lsl #8				@duplicate the results
writeZero:
	str		r11, [r1], -r6, lsr #22				@store zero into the LEFT buffer
	str		r10, [r1], -r6, lsr #22				@store zero into the RIGHT buffer
	@add		r14, r13, #0x4c						@get the address of the start of the channel buffer
	cmp		r1, r14								@check if the current write pointer has passed through the channel buffer
	bhs		writeZero							@repeat until buffer is filled with zero
remainderZeroed:
	add		r13, r13, #12						@deallocate the stack (this channel is done, we dont need this info)
	pop		{r0, r3-r5}							@load the channel pointer, the volume accumulators, and the channel counter
	mov		r10, #0								@set the state to 0
	strb	r10, [r0, #-4]						@store the current state into the channel data
	add		r13, r13, #4						@move the stack pointer past the data that is no longer needed
	ldr		r2, [r13, #4]						@load the number of samples needed this frame
	subs	r5, r5, #1							@decrement the channel counter
	bpl		processOneChannel					@if there are still channels left to do, work on those
	b		mixChannels							@otherwise, branch to the mixing portion

nothingPlaying:
	mov		r3, #0								@prepare the constant 0
zeroOut4SamplesNothingPlaying:
	str		r3, [r1], #4						@write zero to the buffer
	subs	r2, #4								@decrement the counter
	bne		zeroOut4SamplesNothingPlaying		@repeat until the requested number of samples has been zeroed out
	bx		lr									@leave the function