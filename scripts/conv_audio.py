import os
print("imported OS")

def CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, data):
	if (currentProcessedAsset == None):
		exit
	if (currentAssetFileName != "__TemplateSong"):
		currentProcessedAsset.write(data)

def ProcessAssetList(assetList, processedAssetList, originalAssetDir, isBGM):
	for currentOriginalAssetFile in assetList:
		#create the path for this assets and the processed result
		currentOriginalAssetPath = os.path.join(originalAssetDir, currentOriginalAssetFile)
		currentAssetFileName = "_" + (os.path.splitext(currentOriginalAssetFile)[0]).replace(" ", "").replace("\0", "").replace(".", "") \
			.replace(",", "")
		currentProcessedAssetPath = os.path.join(processedAssetDir, currentAssetFileName + ".c")
		currentProcessedAssetFile = os.path.basename(currentProcessedAssetPath)
			
		#check if this file is a valid IT file
		if (os.path.splitext(currentOriginalAssetPath)[1]).lower() != ".it":
			#if this file is not an it file, move on to the next file
			print(currentOriginalAssetFile + " is not a valid .IT file.")
			print()
			continue
			
		#check if a priority file already exists for this asset
		if currentAssetFileName != "__TemplateSong":
			if not os.path.exists(os.path.join(assetPriorityDir, (currentAssetFileName + "_Priority.c"))):
				#write the priority file for this asset
				assetPriorityFile = open(os.path.join(assetPriorityDir, (currentAssetFileName + "_Priority.c")), 'w')
				assetPriorityFile.write("#include \"audio_engine_settings.h\"\n#include \"tonc.h\"\n\n//type in the priority of this asset. Higher number means higher priority\ncu8 ");
				
				if (isBGM):
					assetPriorityFile.write(currentAssetFileName + "_Priority = ALAYER_BGM_CHANNEL_PERSISTENT;\n\n//type in the priority of each channel in this asset. Leave unused channels as blank, or set to 0\ncu8 " + \
						currentAssetFileName + "_ChannelPriority[MAX_DMA_CHANNELS] = { ")
					assetPriorityFile.write("\n\tALAYER_BGM_CHANNEL_PERSISTENT,\t\t\t\t// Channel 1\n\tALAYER_BGM_CHANNEL_PERSISTENT - 1,\t\t\t// Channel 2\n\tALAYER_BGM_CHANNEL_PERSISTENT - 2,\t\t\t// Channel 3\n\tALAYER_BGM_CHANNEL_PERSISTENT - 3,\t\t\t// Channel 4\n\tALAYER_BGM_CHANNEL_PERSISTENT - 4,\t\t\t// Channel 5\n\tALAYER_BGM_CHANNEL_PERSISTENT - 5,\t\t\t// Channel 6\n\tALAYER_BGM_CHANNEL_PERSISTENT - 7,\t\t\t// Channel 7\n\tALAYER_BGM_CHANNEL_PERSISTENT - 8,\t\t\t// Channel 8\n\tALAYER_BGM_CUTOFF,\t\t\t\t\t\t\t// Channel 9\n\tALAYER_BGM_CUTOFF - 1\t\t\t\t\t\t// Channel 10\n")
				else:
					assetPriorityFile.write(currentAssetFileName + "_Priority = ALAYER_SFX;\n\n//type in the priority of each channel in this asset. Leave unused channels as blank, or set to 0\ncu8 " + \
						currentAssetFileName + "_ChannelPriority[MAX_DMA_CHANNELS] = { ")
					assetPriorityFile.write("\n\tALAYER_SFX,\t\t\t\t// Channel 1\n\tALAYER_UNALLOCATED,\t\t// Channel 2\n\tALAYER_UNALLOCATED,\t\t// Channel 3\n\tALAYER_UNALLOCATED,\t\t// Channel 4\n\tALAYER_UNALLOCATED,\t\t// Channel 5\n\tALAYER_UNALLOCATED,\t\t// Channel 6\n\tALAYER_UNALLOCATED,\t\t// Channel 7\n\tALAYER_UNALLOCATED,\t\t// Channel 8\n\tALAYER_UNALLOCATED,\t\t// Channel 9\n\tALAYER_UNALLOCATED\t\t// Channel 10\n")
				assetPriorityFile.write(" };")
		else:
			print("Skipping _Template.it Priority file generation...")

		#open this IT file
		currentOriginalAsset = open(currentOriginalAssetPath, "rb")
		print("Now working on " + currentOriginalAssetFile)
		print()
			
		#append this assets to the list of all assets
		if currentAssetFileName != "__TemplateSong":
			processedAssetList.append(currentAssetFileName)
			
		#check if this file already exists in the processed folder
		if os.path.exists(currentProcessedAssetPath):
			os.remove(currentProcessedAssetPath)
			
		if currentAssetFileName != "__TemplateSong":
			currentProcessedAsset = open(currentProcessedAssetPath, "w")
		else:
			currentProcessedAsset = None

		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "#include \"audio_engine_internal.h\"\n\n")
		
		if currentAssetFileName != "__TemplateSong":
			print("writing to file " + currentProcessedAssetFile)
			print() 
			
		#read the entire file
		currentAssetRawData = currentOriginalAsset.read()
		print("(" + str(len(currentAssetRawData)) + "/" + str(len(currentAssetRawData)) + ") bytes read successfully. Extracting data.")
		print()
			
		#extract the global data from the assets header
		currentAssetName = currentAssetRawData[4:30]
		print("Asset Name: " + currentAssetName.decode("ascii"))
		currentAssetOrdersNum = ((currentAssetRawData[0x21] << 8) | currentAssetRawData[0x20]) - 1
		currentAssetInstrumentsNum = (currentAssetRawData[0x23] << 8) | currentAssetRawData[0x22]
		currentAssetSamplesNum = (currentAssetRawData[0x25] << 8) | currentAssetRawData[0x24]
		currentAssetPatternsNum = (currentAssetRawData[0x27] << 8) | currentAssetRawData[0x26]
		currentAssetInitGlobalVol = currentAssetRawData[0x30]
		currentAssetInitTickSpeed = currentAssetRawData[0x32]
		currentAssetInitTempo = currentAssetRawData[0x33]
		currentAssetInitChannelPan = currentAssetRawData[0x40:0x4A]
		currentAssetInitChannelVol = currentAssetRawData[0x80:0x8A]
			
		#extract the sequenced pattern orders
		currentAssetOrders = [] 
		ordersIndex = 0xc0
		for i in range(currentAssetOrdersNum):
			currentAssetOrders.append(currentAssetRawData[ordersIndex + i])
			
		#extract the pointers to the instrument data
		currentAssetInstruments = []
		instrumentsIndex = 0xc0 + currentAssetOrdersNum + 1
		for i in range(currentAssetInstrumentsNum):
			currentAssetInstruments.append(currentAssetRawData[instrumentsIndex + (i * 4)] | (currentAssetRawData[instrumentsIndex + (i * 4) + 1] << 8) | \
			(currentAssetRawData[instrumentsIndex + (i * 4) + 2] << 16) + (currentAssetRawData[instrumentsIndex + (i * 4) + 3] << 24))
			
		#extract the pointers to the sample data
		currentAssetSamples = []
		samplesIndex = instrumentsIndex + (currentAssetInstrumentsNum * 4)
		for i in range(currentAssetSamplesNum):
			currentAssetSamples.append(currentAssetRawData[samplesIndex + (i * 4)] | (currentAssetRawData[samplesIndex + (i * 4) + 1] << 8) | \
			(currentAssetRawData[samplesIndex + (i * 4) + 2] << 16) + (currentAssetRawData[samplesIndex + (i * 4) + 3] << 24))
			
		#extract the pointers to the pattern data
		currentAssetPatterns = []
		patternsIndex = samplesIndex + (currentAssetSamplesNum * 4)
		for i in range(currentAssetPatternsNum):
			currentAssetPatterns.append(currentAssetRawData[patternsIndex + (i * 4)] | (currentAssetRawData[patternsIndex + (i * 4) + 1] << 8) | \
			(currentAssetRawData[patternsIndex + (i * 4) + 2] << 16) + (currentAssetRawData[patternsIndex + (i * 4) + 3] << 24))
			
		#write the orders for this assets
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "cu8 " + currentAssetFileName + "Orders[] = {")
		for i in range(currentAssetOrdersNum):
			if (i % 32) == 0:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n\t")
			if i != (currentAssetOrdersNum - 1):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentAssetOrders[i]) + ", ") 
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentAssetOrders[i]) + "\n};\n\n") 
		if currentAssetFileName != "__TemplateSong":
			print("(" + str(currentAssetOrdersNum) + "/" + str(currentAssetOrdersNum) + ") sequenced patterns successfully formatted.")
			
		#handle the instruments
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "InstrumentData " + currentAssetFileName + "Instruments[] = {\n")
		keyboardSample = []
		volEnvelopeNodes = []
		panEnvelopeNodes = []
		pitEnvelopeNodes = []
		for i in range(currentAssetInstrumentsNum):
			#extract the data from each instrument
			currentInstrumentIndex = currentAssetInstruments[i]
			fadeOut = currentAssetRawData[currentInstrumentIndex + 0x14] | (currentAssetRawData[currentInstrumentIndex + 0x15] << 8)
			pitPanSeparation = currentAssetRawData[currentInstrumentIndex + 0x16]
			pitPanCenter = currentAssetRawData[currentInstrumentIndex + 0x17]
			globalVolume = currentAssetRawData[currentInstrumentIndex + 0x18]
			defaultPan = currentAssetRawData[currentInstrumentIndex + 0x19]
			randomVolVariation = currentAssetRawData[currentInstrumentIndex + 0x1a]
			randomPanVariation = currentAssetRawData[currentInstrumentIndex + 0x1b]
			currentInstrumentIndex += 0x40
			keyboardSample.clear()
			for j in range(120):
				keyboardSample.append(currentAssetRawData[currentInstrumentIndex + (j*2) + 1] - 1)
			currentInstrumentIndex += 0xf0
			volEnvelopeFlag = currentAssetRawData[currentInstrumentIndex]
			volEnvelopeNodeCount = currentAssetRawData[currentInstrumentIndex + 0x1]
			volEnvelopeLoopBegin = currentAssetRawData[currentInstrumentIndex + 0x2]
			volEnvelopeLoopEnd = currentAssetRawData[currentInstrumentIndex + 0x3]
			volEnvelopeSustainBegin = currentAssetRawData[currentInstrumentIndex + 0x4]
			volEnvelopeSustainEnd = currentAssetRawData[currentInstrumentIndex + 0x5]
			currentInstrumentIndex += 0x6
			volEnvelopeNodes.clear()
			for j in range(25):
				volEnvelopeNodes.append(currentAssetRawData[currentInstrumentIndex + (j * 3)])
				volEnvelopeNodes.append(currentAssetRawData[currentInstrumentIndex + (j * 3) + 1] | (currentAssetRawData[currentInstrumentIndex + (j * 3) + 2] << 8))
			currentInstrumentIndex += 0x4c
			panEnvelopeFlag = currentAssetRawData[currentInstrumentIndex]
			panEnvelopeNodeCount = currentAssetRawData[currentInstrumentIndex + 0x1]
			panEnvelopeLoopBegin = currentAssetRawData[currentInstrumentIndex + 0x2]
			panEnvelopeLoopEnd = currentAssetRawData[currentInstrumentIndex + 0x3]
			panEnvelopeSustainBegin = currentAssetRawData[currentInstrumentIndex + 0x4]
			panEnvelopeSustainEnd = currentAssetRawData[currentInstrumentIndex + 0x5]
			currentInstrumentIndex += 0x6
			panEnvelopeNodes.clear()
			for j in range(25):
				panEnvelopeNodes.append(currentAssetRawData[currentInstrumentIndex + (j * 3)])
				panEnvelopeNodes.append(currentAssetRawData[currentInstrumentIndex + (j * 3) + 1] | (currentAssetRawData[currentInstrumentIndex + (j * 3) + 2] << 8))
			currentInstrumentIndex += 0x4c
			pitEnvelopeFlag = currentAssetRawData[currentInstrumentIndex]
			pitEnvelopeNodeCount = currentAssetRawData[currentInstrumentIndex + 0x1]
			pitEnvelopeLoopBegin = currentAssetRawData[currentInstrumentIndex + 0x2]
			pitEnvelopeLoopEnd = currentAssetRawData[currentInstrumentIndex + 0x3]
			pitEnvelopeSustainBegin = currentAssetRawData[currentInstrumentIndex + 0x4]
			pitEnvelopeSustainEnd = currentAssetRawData[currentInstrumentIndex + 0x5]
			currentInstrumentIndex += 0x6
			pitEnvelopeNodes.clear()
			for j in range(25):
				pitEnvelopeNodes.append(currentAssetRawData[currentInstrumentIndex + (j * 3)])
				pitEnvelopeNodes.append(currentAssetRawData[currentInstrumentIndex + (j * 3) + 1] | (currentAssetRawData[currentInstrumentIndex + (j * 3) + 2] << 8))
				
			#write this instrument's data structure into the file
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\t{.fadeOut = " + hex(fadeOut) + ", .pitPanSeparation = " + \
			hex(pitPanSeparation) + ", .pitPanCenter = " + hex(pitPanCenter) + ", .globalVolume = " + \
			hex(globalVolume) + ", .defaultPan = " + hex(defaultPan) + ", .randomVolVariation = " + \
			hex(randomVolVariation) + ", .randomPanVariation = " + hex(randomPanVariation) + ", .keyboardSample = {")
			for j in range(120):
				if (j % 30) == 0:
					CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n\t")
				if j != 119:
					CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(keyboardSample[j]) + ", ")
				else:
					CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(keyboardSample[j]) + "},\n\t")
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, ".volEnvelope = {.flags = " + hex(volEnvelopeFlag) + ", .nodeCount = " + \
			hex(volEnvelopeNodeCount) + ", .loopBegin = " + hex(volEnvelopeLoopBegin) + ", .loopEnd = " + \
			hex(volEnvelopeLoopEnd) + ", .sustainBegin = " + hex(volEnvelopeSustainBegin) + ", .sustainEnd = " + \
			hex(volEnvelopeSustainEnd))
			if (volEnvelopeNodeCount == 0):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "}}")
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, ",\n\t.envelopeNodes = {")
				for j in range(volEnvelopeNodeCount): 
					if ((j+5) % 10) == 0:
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n\t")
					if j != (volEnvelopeNodeCount - 1):
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "{" + hex(volEnvelopeNodes[j*2]) + ", " + hex(volEnvelopeNodes[(j*2) + 1]) + "}, ")
					else:
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "{" + hex(volEnvelopeNodes[j*2]) + ", " + hex(volEnvelopeNodes[(j*2) + 1]) + "}}},\n\t")
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, ".panEnvelope = {.flags = " + hex(panEnvelopeFlag) + ", .nodeCount = " + \
			hex(panEnvelopeNodeCount) + ", .loopBegin = " + hex(panEnvelopeLoopBegin) + ", .loopEnd = " + \
			hex(panEnvelopeLoopEnd) + ", .sustainBegin = " + hex(panEnvelopeSustainBegin) + ", .sustainEnd = " + \
			hex(panEnvelopeSustainEnd))
			if (panEnvelopeNodeCount == 0):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "}}")
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, ",\n\t.envelopeNodes = {")
				for j in range(panEnvelopeNodeCount): 
					if ((j+5) % 10) == 0:
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n\t")
					if j != (panEnvelopeNodeCount - 1):
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "{" + hex(panEnvelopeNodes[j*2]) + ", " + hex(panEnvelopeNodes[(j*2) + 1]) + "}, ")
					else:
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "{" + hex(panEnvelopeNodes[j*2]) + ", " + hex(panEnvelopeNodes[(j*2) + 1]) + "}}},\n\t")
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, ".pitchEnvelope = {.flags = " + hex(pitEnvelopeFlag) + ", .nodeCount = " + \
			hex(pitEnvelopeNodeCount) + ", .loopBegin = " + hex(pitEnvelopeLoopBegin) + ", .loopEnd = " + \
			hex(pitEnvelopeLoopEnd) + ", .sustainBegin = " + hex(pitEnvelopeSustainBegin) + ", .sustainEnd = " + \
			hex(pitEnvelopeSustainEnd))
			if (pitEnvelopeNodeCount == 0):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "}}")
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, ",\n\t.envelopeNodes = {")
				for j in range(pitEnvelopeNodeCount): 
					if ((j+5) % 10) == 0:
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n\t")
					if j != (pitEnvelopeNodeCount - 1):
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "{" + hex(pitEnvelopeNodes[j*2]) + ", " + hex(pitEnvelopeNodes[(j*2) + 1]) + "}, ")
					else:
						CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "{" + hex(pitEnvelopeNodes[j*2]) + ", " + hex(pitEnvelopeNodes[(j*2) + 1]) + "}}}}")
			if i != (currentAssetInstrumentsNum - 1):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, ",")
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n};")
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n\n")
		print("(" + str(currentAssetInstrumentsNum) + "/" + str(currentAssetInstrumentsNum) + ") instruments successfully formatted.")
			
		#extract and write each pattern's raw data
		for i in range(currentAssetPatternsNum):
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "cu8 " + currentAssetFileName + "Pattern" + str(i) + "[] = {")
			currentPatternIndex = currentAssetPatterns[i]
			#check if this is an empty pattern
			if(currentPatternIndex == 0):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n};\n\n")
				continue
			currentPatternLength = currentAssetRawData[currentPatternIndex] | (currentAssetRawData[currentPatternIndex + 1] << 8)
			currentPatternIndex += 8
			for j in range(currentPatternLength):
				if (j % 24) == 0:
					CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n\t")
				if j != (currentPatternLength - 1):
					CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentAssetRawData[currentPatternIndex + j]) + ", ")
				else:
					CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentAssetRawData[currentPatternIndex + j]) + "\n};\n\n")
			
		#write the data structure linking all of the patterns together
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "PatternData " + currentAssetFileName + "Patterns[] = {\n\t")
		for i in range(currentAssetPatternsNum):
			currentPatternIndex = currentAssetPatterns[i]
			currentPatternRowsNum = currentAssetRawData[currentPatternIndex + 2]
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "{.rowsNum = " + hex(currentPatternRowsNum) + ", .packedPatternData = " + \
			currentAssetFileName + "Pattern" + str(i))
			if i != (currentAssetPatternsNum - 1):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "},\n\t")
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "}\n};\n\n")
		if currentAssetFileName != "__TemplateSong":
			print("(" + str(currentAssetPatternsNum) + "/" + str(currentAssetPatternsNum) + ") patterns successfully formatted.")
			
		#extract the sample data
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "cu16 " + currentAssetFileName + "Samples[] = {\n\t")
		failedSamples = 0
		currentSampleRawData = []
		for i in range(currentAssetSamplesNum):
			currentSampleIndex = currentAssetSamples[i]
			currentOriginalSampleNameRaw = currentAssetRawData[(currentSampleIndex + 0x14):(currentSampleIndex + 0x2e)]
			currentOriginalSampleName = "_" + (currentOriginalSampleNameRaw.decode("ascii").replace(" ", "").replace("\0", "").replace(".", "") \
			.replace(",", "").replace("/", "").replace("\\", "").replace("-", "")).replace("(", "").replace(")", "").replace("!", "") \
			.replace(":", "").replace("@", "").replace("'", "").replace("#", "")
			currentOriginalSamplePath = os.path.join(originalSampleDir, currentOriginalSampleName + "_smp" + ".s")
			currentSampleFlags = currentAssetRawData[currentSampleIndex + 0x12]
			currentSampleFormatFlags = currentAssetRawData[currentSampleIndex + 0x2e]
			
			#if the sample is blank
			if currentOriginalSampleName == "_":
				#add this sample to the assets's sample list
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "0xffff, ")
				print("\t(" + str(i + 1) + "/" + str(currentAssetSamplesNum) + ") Sample: " + \
				currentOriginalSampleName + " is a blank sample, unnamed samples are not permitted.")
				failedSamples += 1
				continue
			
			#if this is a psg sample, 
			if currentOriginalSampleName[0:4] == "_PSG":
				#add this sample to the assets's sample list
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "0xffff, ")
				print("\t(" + str(i + 1) + "/" + str(currentAssetSamplesNum) + ") Sample: " + \
				currentOriginalSampleName + " is a PSG Sample, ignoring.")
				continue
				
			#check if this sample already exists, another assets already processed this sample. 
			if os.path.exists(currentOriginalSamplePath):
				#just add it to the pointers list, no other processing needed
				print("\t(" + str(i + 1) + "/" + str(currentAssetSamplesNum) + ") Sample: " + \
				currentOriginalSampleName + " already exists. Linking to the existing sample.")
				index = originalSamplesList.index(currentOriginalSampleName)
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(index) + ", ")
				continue
				
			#check if this sample is in a supported format
			if(currentSampleFlags & 0xC) or (currentSampleFormatFlags & 0x3c):
				print("\t(" + str(i + 1) + "/" + str(currentAssetSamplesNum) + ") Sample: " + \
				currentOriginalSampleName + " is in an unsupported format. Skipping.")
				failedSamples += 1
				continue
				
			#extracting the sample from the IT file.
			currentSampleGlobalVolume = currentAssetRawData[currentSampleIndex + 0x11]
			currentSampleDefaultVolume = currentAssetRawData[currentSampleIndex + 0x13]
			currentSampleDefaultPan = currentAssetRawData[currentSampleIndex + 0x2f]
			currentSampleLength = currentAssetRawData[currentSampleIndex + 0x30] | (currentAssetRawData[currentSampleIndex + 0x31] << 8) | \
			(currentAssetRawData[currentSampleIndex + 0x32] << 16) | (currentAssetRawData[currentSampleIndex + 0x33] << 24)
			currentSampleMiddleCPitch = currentAssetRawData[currentSampleIndex + 0x3c] | (currentAssetRawData[currentSampleIndex + 0x3d] << 8) | \
			(currentAssetRawData[currentSampleIndex + 0x3e] << 16) | (currentAssetRawData[currentSampleIndex + 0x3f] << 24)
			currentSampleDataIndex = currentAssetRawData[currentSampleIndex + 0x48] | (currentAssetRawData[currentSampleIndex + 0x49] << 8) | \
			(currentAssetRawData[currentSampleIndex + 0x4a] << 16) | (currentAssetRawData[currentSampleIndex + 0x4b] << 24)
			currentSampleVibratoSpeed = currentAssetRawData[currentSampleIndex + 0x4c]
			currentSampleVibratoDepth = currentAssetRawData[currentSampleIndex + 0x4d]
			currentSampleVibratoSweep = currentAssetRawData[currentSampleIndex + 0x4e]
			currentSampleVibratoWave = currentAssetRawData[currentSampleIndex + 0x4f]
				
			#create a raw file for this sample
			currentOriginalSample = open(currentOriginalSamplePath, "w")
			currentSampleLoopStart = currentAssetRawData[currentSampleIndex + 0x34] | (currentAssetRawData[currentSampleIndex + 0x35] << 8) | \
			(currentAssetRawData[currentSampleIndex + 0x36] << 16) | (currentAssetRawData[currentSampleIndex + 0x37] << 24)
			currentSampleLoopEnd = currentAssetRawData[currentSampleIndex + 0x38] | (currentAssetRawData[currentSampleIndex + 0x39] << 8) | \
			(currentAssetRawData[currentSampleIndex + 0x3a] << 16) | (currentAssetRawData[currentSampleIndex + 0x3b] << 24)
			currentSampleSustainLoopStart = currentAssetRawData[currentSampleIndex + 0x40] | (currentAssetRawData[currentSampleIndex + 0x41] << 8) | \
			(currentAssetRawData[currentSampleIndex + 0x42] << 16) | (currentAssetRawData[currentSampleIndex + 0x43] << 24)
			currentSampleSustainLoopEnd = currentAssetRawData[currentSampleIndex + 0x44] | (currentAssetRawData[currentSampleIndex + 0x45] << 8) | \
			(currentAssetRawData[currentSampleIndex + 0x46] << 16) | (currentAssetRawData[currentSampleIndex + 0x47] << 24)
			currentSampleLoopPong = currentSampleFlags & 0x40
			sampleIndex = 0;
			currentSampleRawData.clear()
			
			for each in range(currentSampleLength):
					
				#if 8 bits
				if(currentSampleFlags & 0x2) == 0:
					sampleData = currentAssetRawData[currentSampleDataIndex + sampleIndex]
				#if 16 bits little endian
				elif(currentSampleFormatFlags & 0x2) == 0:
					sampleData = currentAssetRawData[currentSampleDataIndex + (sampleIndex * 2) + 1]
				#if 16 bits big endian
				else:
					sampleData = currentAssetRawData[currentSampleDataIndex + (sampleIndex * 2)]
				#if samples are signed
				if(currentSampleFormatFlags & 0x1) == 1:
					if(sampleData >= 128):
						sampleData -= 128
					else:
						sampleData += 128
					
				sampleIndex += 1
				currentSampleRawData.append(sampleData)
					
			#write the data into the sample file
			currentOriginalSample.write(".section\t.rodata\n.global\t" + currentOriginalSampleName + "_smp" + "_data\n.align 2\n" + \
			currentOriginalSampleName + "_smp" + "_data:")
			for j in range(len(currentSampleRawData)):	
				if j == (len(currentSampleRawData) - 1):
					currentOriginalSample.write(", " + hex(currentSampleRawData[j]) + ", 0x80\n")
				elif(j % 32) == 0:
					delta = currentSampleRawData[j + 1] - currentSampleRawData[j]
					delta += 1
					delta >>= 1
					delta += 128
					if delta == 0x100:
						delta = 0xff
					currentOriginalSample.write("\n.byte\t" + hex(currentSampleRawData[j]))
					currentOriginalSample.write(", " + hex(delta))
				else:
					delta = currentSampleRawData[j + 1] - currentSampleRawData[j]
					delta += 1
					delta >>= 1
					delta += 128
					if delta == 0x100:
						delta = 0xff
					currentOriginalSample.write(", " + hex(currentSampleRawData[j]))
					currentOriginalSample.write(", " + hex(delta))
				
			#write the data into the audio list
			audioList.write("extern cu8 " + currentOriginalSampleName + "_smp" + "_data[];\nAudioSample " + \
			currentOriginalSampleName + "_smp" + " = {\n\t.sampleStart = " + currentOriginalSampleName + "_smp" + "_data, .sampleLength = " + str(currentSampleLength) + ", .loopEnd = " + \
			str(currentSampleLoopEnd) + ", .loopStart = " + str(currentSampleLoopStart) + ", .sustainLoopEnd = " + str(currentSampleSustainLoopStart) + \
			", .sustainLoopStart = " + str(currentSampleSustainLoopEnd) + ", .middleCPitch = " + str(currentSampleMiddleCPitch) + ",\n\t.sampleType = " + \
			str(currentSampleFlags >> 4) + ", .globalVolume = " + str(currentSampleGlobalVolume) + ", .defaultVolume = " + \
			str(currentSampleDefaultVolume) + ", .defaultPan = " + str(currentSampleDefaultPan) + ", .vibratoSpeed = " + \
			str(currentSampleVibratoSpeed) + ", .vibratoDepth = " + str(currentSampleVibratoDepth) + ", .vibratoWave = " + \
			str(currentSampleVibratoWave) + ", .vibratoSweep = " + str(currentSampleVibratoSweep) + "\n};\n\n")
			
			#add this sample to the sample list
			originalSamplesList.append(currentOriginalSampleName)
			CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(len(originalSamplesList) - 1) + ", ")
			
			print("\t(" + str(i + 1) + "/" + str(currentAssetSamplesNum) + ") Sample: " + currentOriginalSampleName + " successfully formatted.")
			currentOriginalSample.close()
			
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "\n};\n\n")
		print("(" + str(currentAssetSamplesNum - failedSamples) + "/" + str(currentAssetSamplesNum) + ") Samples successfully formatted")
			
		#write the AssetData struct for this assets
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "extern cu8 " + currentAssetFileName + "_Priority;\nextern cu8 " + \
		currentAssetFileName + "_ChannelPriority[MAX_DMA_CHANNELS];\nAssetData " + currentAssetFileName + "Asset = {\n\t.orders = " + \
		currentAssetFileName + "Orders, .instruments = " + currentAssetFileName + "Instruments, .samples = " + \
		currentAssetFileName + "Samples, .patterns = " + currentAssetFileName + "Patterns, .assetName = \"" + \
		(currentAssetName.decode("ascii")).strip("\0") + "\",\n\t.ordersNum = " + hex(currentAssetOrdersNum) + ", .instrumentsNum = " + \
		hex(currentAssetInstrumentsNum) + ", .samplesNum = " + hex(currentAssetSamplesNum) + ", .patternsNum = " + \
		hex(currentAssetPatternsNum) + ", .initGlobalVol = " + hex(currentAssetInitGlobalVol) + ", .initTickSpeed = " + \
		hex(currentAssetInitTickSpeed) + ", .initTempo = " + hex(currentAssetInitTempo) + ",\n\t.assetPriority = &" + \
		currentAssetFileName + "_Priority, .assetChannelPriority = " + currentAssetFileName + "_ChannelPriority,\n\t.initChannelVol = {")
		i = 0
		for currentChannelData in currentAssetInitChannelVol:
			i += 1
			if i != len(currentAssetInitChannelVol):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentChannelData) + ", ")
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentChannelData))
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "}, .initChannelPan = {")
		i = 0
		for currentChannelData in currentAssetInitChannelPan:
			i += 1
			if i != len(currentAssetInitChannelPan):
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentChannelData) + ", ")
			else:
				CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, hex(currentChannelData))
		CurrentProcessedAssetWrite(currentProcessedAsset, currentAssetFileName, "}\n\t};\n")
		
		if (failedSamples == 0):
			print("Asset: " + (currentAssetName.decode("ascii").strip("\0")) + " successfully formatted\n")
		else:
			print("Asset: " + (currentAssetName.decode("ascii").strip("\0")) + " completed with " + str(failedSamples) + " failed samples\n")
			
		#close the open files
		currentOriginalAsset.close()
		if currentProcessedAsset != None:
			currentProcessedAsset.close()



print()

#get the directory of this script
cwd = os.getcwd()
print("current working directory is " + cwd)

#figure out and store the name of this script's filename
scriptFileName = os.path.basename(__file__)

#navigate to the directory containing the original BGM asset data
originalBGMAssetDir = os.path.join(cwd, os.pardir, 'resources', 'sounds')
print("the original BGM assets directory is " + originalBGMAssetDir)

#navigate to the directory containing the original SFX asset data
originalSFXAssetDir = os.path.join(cwd, os.pardir, 'resources', 'sfx')
print("the original SFX assets directory is " + originalSFXAssetDir)

#navigate to the directory containing the processed assets data
processedAssetDir = os.path.join(cwd, os.pardir, 'data', 'audio', 'assets_processed')
print("the processed assets directory is " + processedAssetDir)

#navigate to the directory containing the processed samples data
originalSampleDir = os.path.join(cwd, os.pardir, 'data', 'audio', 'samples_processed')
print("the processed sample directory is " + originalSampleDir)

#navigate to the directory containing the asset priority files
assetPriorityDir = os.path.join(cwd, os.pardir, 'data', 'audio', 'assets_priorities')
print("the asset priority directory is " + assetPriorityDir)

#open the audio_list file
audioListPath = os.path.join(cwd, os.pardir, 'source', 'audio', 'audio_list.c')
print("the global audio list file is " + audioListPath)

#open the audio_assets_enum file
audioAssetsPath = os.path.join(cwd, os.pardir, 'source', 'audio', 'audio_assets_enum.h')
print("the audio assets enum file is " + audioAssetsPath + "\n")

#open the audio_volumes file
audioVolumesPath = os.path.join(cwd, os.pardir, assetPriorityDir, '_audio_volumes.c')
print("the audio volumes file is " + audioAssetsPath + "\n")

#count how many files are in the raw sample folder
bgmList = os.listdir(originalBGMAssetDir)
print("There are " + str(len(bgmList)) + " files in the BGM directory.")
print()

#sfxList = os.listdir(originalSFXAssetDir)
#print("There are " + str(len(sfxList)) + " files in the SFX directory.")
#print()

# If it exists, move _TemplateSong.it to the very front of the processing list

try:
	index_to_move = bgmList.index('_TemplateSong.it')
	musTemplate = bgmList.pop(index_to_move)
	bgmList.insert(1, musTemplate)
except ValueError:
	pass
#try:
#	index_to_move2 = sfxList.index('_TemplateSong.it')
#	musTemplate = sfxList.pop(index_to_move2)
#	sfxList.insert(1, musTemplate)
#except ValueError:
#	pass

#setup the global lists
processedSoundAssetList = []
originalSamplesList = []

#delete and recreate audio_list.c
if os.path.exists(audioListPath):
	os.remove(audioListPath)
audioList = open(audioListPath, "w")
audioList.write("//This file is automatically generated by [" + scriptFileName + "]. Do not edit manually.\n#include \"audio_engine_internal.h\"\n\n")

#delete and recreate audio_volumes.h
audioVolumesExists = False
if os.path.exists(audioVolumesPath):
	audioVolumesExists = True

if not audioVolumesExists:
	audioVolumes = open(audioVolumesPath, "w")

#delete and recreate audio_assets_enum.h
if os.path.exists(audioAssetsPath):
	os.remove(audioAssetsPath)
audioAssets = open(audioAssetsPath, "w")
audioAssets.write("//This file is automatically generated by [" + scriptFileName + "]. Do not edit manually.\n#ifndef audioAssetEnumh\n#define audioAssetEnumh\n\nenum AudioAssetName{\n\t")

#delete every sample file
originalSampleList = os.listdir(originalSampleDir)
for currentOriginalSampleFile in originalSampleList:
	os.remove(os.path.join(originalSampleDir, currentOriginalSampleFile))
gitignore = open(os.path.join(originalSampleDir, '.gitkeep'), "w")
gitignore.close()

#delete every assets file
processedAssetListDelete = os.listdir(processedAssetDir)
for currentProcessedAssetFile in processedAssetListDelete:
	os.remove(os.path.join(processedAssetDir, currentProcessedAssetFile))
gitignore = open(os.path.join(processedAssetDir, '.gitkeep'), "w")
gitignore.close()

ProcessAssetList(bgmList, processedSoundAssetList, originalBGMAssetDir, True)
#ProcessAssetList(sfxList, processedSoundAssetList, originalSFXAssetDir, False)


#write the list of every sample to the audio_list file
audioList.write("AudioSample *sampleList[] = {\n\t")
for i in range(len(originalSamplesList)):
	if i != len(originalSamplesList) - 1:
		audioList.write("&" + originalSamplesList[i] + "_smp" + ", ")
	else:
		audioList.write("&" + originalSamplesList[i] + "_smp" + "\n};\n\n")

audioList.write("cu16 numSamples = " + str(len(originalSamplesList)) + ";\n\n")

if not audioVolumesExists:
	#write the list of every sound asset volume to the audio_volumes file
	audioVolumes.write("#include \"audio_engine_settings.h\"\n#include \"tonc.h\"\n\ncu8 *_sndAssetDefaultVol[" + str(len(processedSoundAssetList)) + "] = {\n")
	for i in range(len(processedSoundAssetList)):
		if i != len(processedSoundAssetList) - 1:
			audioVolumes.write("\t255,\t\t\t// " + processedSoundAssetList[i] + "\n")
		else:
			audioVolumes.write("\t255\t\t\t\t// " + processedSoundAssetList[i] + "\n};\n\n")

#write the list of every sound asset to the audio_list file
for i in range(len(processedSoundAssetList)):
	audioList.write("extern AssetData " + processedSoundAssetList[i] + "Asset;\n")
audioList.write("\nAssetData *soundList[] = {\n\t")
for i in range(len(processedSoundAssetList)):
	if i != len(processedSoundAssetList) - 1:
		audioList.write("&" + processedSoundAssetList[i] + "Asset, ")
	else:
		audioList.write("&" + processedSoundAssetList[i] + "Asset\n};\n\n")
	
audioList.write("cu16 numSounds = " + str(len(processedSoundAssetList)) + ";\n\n")

#write an enum for every Sound asset
for i in range(len(processedSoundAssetList)):
	audioAssets.write(processedSoundAssetList[i] + ", \t\t\t// " + str(i) + "\n\t")
audioAssets.write("\n};\n\n#endif")

audioAssets.close()
audioList.close()

if not audioVolumesExists:
	audioVolumes.close()

#wait for the user to press any key, but only if they're not running this Python script in a VSCode Build task
#if os.getenv('RUN_INTERACTIVELY') == 'true':
print("All audio assets processed. Press enter to close.")
input()
#else:
#	print("All audio assets processed.")