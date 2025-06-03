#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <direct.h>
#include <string.h>
#include "converter.h"
#include "converter.cpp"

typedef float real32;
typedef double real64;

int    Global_CountOfUnimportantChunks = 0;
uint32 Global_BytesNeededForWav = 0;
int    Global_NumSampleLoops = 0;
chunk_finder *Global_UnChunkDirectory = 0;
global int64 Global_PerfCountFrequency;


extern "C"
{
    unsigned int
    GPerfHasher (register const char *str, register size_t len);

    const char *
    GPerfIDLookup (register const char *str, register size_t len);
}

/********************************************************************************************
 *
 *                                  README
 *
 * This program converts an .aif file to a .wav file, but unlike general-purpose
 * audio tools such as Audacity, it preserves important metadata that allow
 * samplers to identify a file's pitch, loop points, and other such information
 * critical for sampled audio to function as input into a digital instrument.
 *
 * .aif files and .wav files perform the same function, but have the following
 * critical differences:
 *
 *     - .aif files store data in Big-Endian byte ordering, while .wav
 *       files use Little-Endian byte ordering
 *
 *     - .aif and .wav files structure their metadata differently. Both adopt
 *       the notion of the "chunk" -- essentially, a C struct that collects
 *       related information -- but their chunks are not compatible in any way.
 *
 * Therefore, after some initialization, flow of the program follows this general
 * pattern:
 *
 *     1.) Scan the .aif file and identify the addresses of its metadata chunks
 *
 *     2.) Validate the .aif file by confirming that its metadata conform to
 *         the .aif standard defined by Apple
 *
 *     3.) Parse the .aif chunks and convert the Endianness of their numerical
 *         data from Big Endian to Little Endian
 *
 *     4.) Build a new .wav file from the parsed data
 *
 *     5.) Write the new .wav file to disk
 *
 * Because this program's work is overwhelmingly concerned with parsing metadata
 * chunks, the word "chunk" itself is used liberally to define variables
 * and functions. Any naming convention must clearly delineate when our concern
 * is the .aif file provided as input; vs. "converted" data waiting to be
 * formatted for inclusion into a .wav file; vs. data properly organized for
 * inclusion in a .wav file.
 *
 * The following conventions are observed:
 *
 *     - Variables concerning the .aif input file are prefixed with
 *       "Aif_", e.g.:
 *
 *             uint8 *Aif_FileIndex = Aif_FileStart;
 *             (represents a byte pointer into the .aif file provided as input)
 *
 *     - Any parsing functions are understood to be operating on .aif data,
 *       since parsing an .aif file and outputting a corresponding .wav file
 *       is the purpose of the program. E.g.:
 *
 *             ParseCommonChunk(uint8 *Aif_CommonChunk_Start,
 *                     conv_common_chunk *Conv_CommonChunk);
 *             (a function that parses a chunk in the .aif input file
 *              and converts its data)
 *
 *     - Variables prefixed with "Conv_" refer to those containing data that have
 *       been parsed and converted for .wav output, but whose data have not yet
 *       been organized in the manner a .wav file expects. E.g.:
 *
 *             conv_common_chunk *Conv_CommonChunk =
 *                 PushStruct(WorkingMem, conv_common_chunk);
 *             (a declaration of a struct into which the program writes
 *              converted data; here a function stores it in the
 *              program's "WorkingMem" memory arena)
 *
 *     - Variables whose prefix is "Wav_" are those whose data are written to disk
 *       as part of the output of the program. E.g.:
 *
 *             CopyIDToWav("RIFF", Wav_RiffChunk->ID);
 *             (A function that copies the string "RIFF" into a struct;
 *              the struct will be later written to the .wav file)
 *
 *     - Additionally, structure tags are always uncapitalized, while
 *       structure variables are capitalized:
 *
 *             wav_sampler_chunk *Wav_SamplerChunk =
 *                 PushStruct(WavBuffer, wav_sampler_chunk);
 *
 ********************************************************************************************/


int main(int argc, char *argv[])
{
    if(argc != 2)
    {
	printf("\n\n  ===== How to use this program =====  \n\n"
		"Type:\n\n"
		"  win32_converter.exe <InputFile.aif>\n\n"
		"where <InputFile.aif> represents the name of the .aif file\n"
		"you wish to convert to a .wav file.\n\n"
		"Just paste that dang filename directly after \"win32_converter.exe\",\n"
		"but put a space in between!!\n\n"
		"Note that if your filename contains spaces, it must be enclosed in "
		"quotation marks.\n\n");
	exit(1);
    }
    if(strlen(argv[1]) > (MAX_STRING_LEN + 1))
    {
	printf("\n\nThis program only accepts filenames whose lengths are 255 or fewer.\n\n");
    }
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    Global_PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    LARGE_INTEGER StopwatchStart; 
    QueryPerformanceCounter(&StopwatchStart);
    uint64 CountCyclesStart = __rdtsc();

    // Todo: Test that the input file extension is .aif

    // Load .aif file
    uint8 *Aif_FileStart;
    char *Aif_Filename = argv[1];

    HANDLE HeapHandle = GetProcessHeap();
    LARGE_INTEGER Aif_FileSize;
    Aif_FileSize.QuadPart = 0;



    if(HeapHandle)
    {
        Aif_FileStart = (uint8 *)Win32_GetAifFilePointer((LPCSTR)Aif_Filename, &Aif_FileSize);
    }
    else
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"At program start, failed to get HeapHandle, so exited");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }

    // Allocate memory
    arena *WorkingMem = ArenaAlloc(Megabytes(1));

    // Declare pointer into Aif file that we move around as we parse. Hereafter,
    //	  any variable prefixed "Aif_" refers to data in the .aif file that
    //	  we're converting
    uint8 *Aif_FileIndex = Aif_FileStart;

    // Every .aif file that meets spec starts with a Form chunk; if we don't
    //	  find it, the Form chunk parsing function exits the program
    conv_form_chunk *Conv_FormChunk = PushStruct(WorkingMem, conv_form_chunk);
    int64 Aif_FileSize_IntVersion = (int64) Aif_FileSize.QuadPart;
    ParseFormChunk(Aif_FileIndex, Conv_FormChunk, Aif_FileSize_IntVersion);
    Aif_FileIndex = Conv_FormChunk->Aif_SubChunksStart;

    // Excepting a very few edge cases, .aif files that meet spec will 
    //	  almost always have one Common chunk and one Sound Data chunk 
    //	  (hereafter referred to as "IMportant chunks").
    //
    //	  So we next look for these, and as we scan the file for them, store 
    //	  pointers to other chunks we find along the way (these are 
    //	  hereafter referred to as "UNimportant chunks").
    uint8 *LastByteInFile = (uint8 *)( (Aif_FileStart + Aif_FileSize_IntVersion) - 1);

    // We're gonna need the Common and Sound Data chunks if the .aif file meets 
    //	  spec, so just allocate for them here and then we can parse them as soon as
    //	  we find them in the while loop below
    conv_common_chunk *Conv_CommonChunk = PushStruct(WorkingMem, conv_common_chunk);
    conv_sound_data_chunk *Conv_SoundDataChunk = PushStruct(WorkingMem, conv_sound_data_chunk);
    
    // We'll likely find unimportant chunks too, but we don't parse them until
    //	  after we've done our initial scan and validated the file. So for now
    //	  we just keep a count of how many we find and store their addresses.
    Global_UnChunkDirectory = PushArray(WorkingMem, chunk_finder, MAX_UNIMPORTANT_CHUNKS);
    int CountOfEachChunkType[HASHED_CHUNK_ID_ARRAY_SIZE] = {0};


    // Scan the .aif file
    while(Aif_FileIndex <= LastByteInFile)
    {
	// Figure out which chunk we're looking at
	generic_chunk_header *Aif_ThisChunksHeader = (generic_chunk_header *)Aif_FileIndex;
	chunk HashedID = (chunk)GPerfHasher(Aif_ThisChunksHeader->ID, ID_WIDTH);

	// If we encounter a second Form chunk, the file is busted, so exit
	if(HashedID == FORM_CHUNK)
	{
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		    "\nERROR:\n\t"
		    "\n\t\tThis .aif file contains more than one Form chunk,"
		    "\n\t\twhich is not permitted by the .aif specification."
		    "\n\t\tTherefore, your .aif file appears to be corrupted."
		    "\n\nThis program will now exit.");
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);
	}

	// If it's the Common chunk, parse it
	else if(HashedID == COMMON_CHUNK)
	{
	    ParseCommonChunk(Aif_FileIndex, Conv_CommonChunk);
	}
	// If it's the Sound Data chunk, parse it
	else if(HashedID == SOUND_DATA_CHUNK)
	{
	    ParseSoundDataChunk(Aif_FileIndex, Conv_SoundDataChunk);
	}
	
	// Otherwise it's an unimportant chunk. For now we just store the
	//    address; we don't parse it until we've validated the file
	else if(GPerfIDLookup(Aif_ThisChunksHeader->ID, ID_WIDTH))
	{
	    if( Global_CountOfUnimportantChunks < (MAX_UNIMPORTANT_CHUNKS - 1) )
	    {
		Global_UnChunkDirectory[Global_CountOfUnimportantChunks].HashedID = HashedID;
		Global_UnChunkDirectory[Global_CountOfUnimportantChunks].Aif_Chunk = Aif_FileIndex;
		Global_CountOfUnimportantChunks++;
	    }
	    else
	    {
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		    "\nERROR:\n\t"
		    "\n\t\t.aif file contains more chunks than this program expected."
		    "\n\nThis program will now exit.");
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);
	    }
	}

	// If we enter this block, a chunk ID we read does not meet 
	//    .aif spec, so exit
	else
	{
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		    "\nERROR:\n\t"
		    "\n\t\tThis .aif file's metadata appears to be corrupted."
		    "\n\nThis program will now exit.");
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);

	}
	CountOfEachChunkType[HashedID]++;
	Aif_FileIndex = Aif_ThisChunksHeader->Data;
	Aif_FileIndex += FlipEndianness(Aif_ThisChunksHeader->ChunkSize);
    }

    // Validate file
    ValidateAif(CountOfEachChunkType, Conv_CommonChunk, Conv_SoundDataChunk);

    // We need to know the size of the buffer we'll need to write the .wav file.
    //	  We have some some of the information already, but some we'll need to get
    //	  from the .aif chunks we parse below in the loop
    Global_BytesNeededForWav += sizeof(wav_riff_chunk);
    Global_BytesNeededForWav += sizeof(wav_fmt_chunk);
    Global_BytesNeededForWav += sizeof(wav_data_chunk);
    Global_BytesNeededForWav += ( (Conv_CommonChunk->NumSampleFrames) *
			    (Conv_CommonChunk->NumChannels) *
			    (Conv_CommonChunk->SampleSize / BITS_IN_BYTE) );
    
    // All .wav chunks have to be even-byte aligned. If the .aif file contains
    //	  an odd number of sample frames, our Global_BytesNeededForWav count will
    //	  be odd at this point in execution. So we check for that and
    //	  account for a pad byte if necessary
    if( (Global_BytesNeededForWav % 2) != 0)
    {
	Global_BytesNeededForWav += 1;
    }

    // Loop through the array of unimportant chunk pointers we stored 
    //	  and convert the chunks' data, and finish computing how many
    //	  bytes we need for the .wav
    for(int UnChunkIdx = 0; UnChunkIdx < Global_CountOfUnimportantChunks; UnChunkIdx++)
    {
	// Figure out which chunk we're looking at
	chunk_finder ThisChunk = Global_UnChunkDirectory[UnChunkIdx];

	switch(ThisChunk.HashedID)
	{
	    case MARKER_CHUNK:
	    {
		conv_marker_chunk *Conv_MarkerChunk = 
				PushStruct(WorkingMem, conv_marker_chunk);
		Global_UnChunkDirectory[UnChunkIdx].Conv_Chunk = (uint8 *)Conv_MarkerChunk;
		ParseMarkerChunk(ThisChunk.Aif_Chunk, Conv_MarkerChunk);
		
		int TotalMarkers = (int)Conv_MarkerChunk->TotalMarkers;

		if(TotalMarkers > 0)
		{
		    Global_BytesNeededForWav += sizeof(wav_sampler_chunk);
		    Conv_MarkerChunk->Conv_MarkersStart = 
			PushArray(WorkingMem, conv_marker, TotalMarkers);
		    ParseMarkers(Conv_MarkerChunk, WorkingMem, 
			    Conv_CommonChunk->NumSampleFrames);
		}
	    } break;

	    case INSTRUMENT_CHUNK:
	    {
		conv_instrument_chunk *Conv_InstrumentChunk = 
				    PushStruct(WorkingMem, conv_instrument_chunk);
		Global_UnChunkDirectory[UnChunkIdx].Conv_Chunk = (uint8 *)Conv_InstrumentChunk;
		ParseInstrumentChunk(ThisChunk.Aif_Chunk, Conv_InstrumentChunk);
		Global_BytesNeededForWav += sizeof(wav_inst_chunk);

		// .wav Instrument chunks are 7 bytes, so we need to account
		//    for an extra byte here if we want our .wav file to be
		//    even-byte aligned
		Global_BytesNeededForWav += 1;

		// The Instrument chunk stores information about whether a sampler
		//    playing the .aif file needs to loop it. If there's loop info
		//    stored in the .aif, that means we need to allocate bytes 
		//    in the .wav to store that info
		if(Conv_InstrumentChunk->SustainLoop.BeginLoopMarker)
		{
		    Global_BytesNeededForWav += sizeof(wav_sample_loop);
		    Global_NumSampleLoops += 1;
		}
		// Todo: Have a test here for the weird situation where
		//    there's a BeginLoopMarker but no EndLoopMarker
		if(Conv_InstrumentChunk->ReleaseLoop.BeginLoopMarker)
		{
		    Global_BytesNeededForWav += sizeof(wav_sample_loop);
		    Global_NumSampleLoops += 1;
		}
	    } break;

	    case FILLER_CHUNK: 
	    {
		filler_chunk *FillerChunk = PushStruct(WorkingMem, filler_chunk);
		Global_UnChunkDirectory[UnChunkIdx].Conv_Chunk = (uint8 *)FillerChunk;
		ParseFillerChunk(ThisChunk.Aif_Chunk, FillerChunk);
	    } break;

	    default:
	    {
		char DebugPrintStringBuffer[MAX_STRING_LEN];
		sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
			"\nERROR:\n\t"
			"\n\t\tEncountered unexpected chunk when converting"
			"\n\t\t.aif chunk data."
			"\n\nThis program will now exit.");
		OutputDebugStringA((char *)DebugPrintStringBuffer);
		exit(1);

	    } break;
	}

    }

    // Organize the converted data into the format expected by .wav
    int WavBufferSize = ARENA_BOILERPLATE + Global_BytesNeededForWav;
    arena *WavBuffer = ArenaAlloc(WavBufferSize);
    // We have some meta info local at the start of the arena. When we write
    //	  the file, we need the address of the first byte after the
    //	  meta info, so we don't accidentally write it to the .wav file
    uint8 *WavFileStarts = (uint8 *)WavBuffer->NextFreeByte;

    wav_riff_chunk *Wav_RiffChunk = PushStruct(WavBuffer, wav_riff_chunk);
    CopyIDToWav("RIFF", Wav_RiffChunk->ID);
    uint32 WavRiffChunkSize = Global_BytesNeededForWav -
			      ( ID_WIDTH + sizeof(Wav_RiffChunk->ChunkSize) );
    Wav_RiffChunk->ChunkSize = WavRiffChunkSize;
    CopyIDToWav("WAVE", Wav_RiffChunk->Format);

    wav_fmt_chunk *Wav_FormatChunk = PushStruct(WavBuffer, wav_fmt_chunk);
    CopyIDToWav("fmt ", Wav_FormatChunk->ID);
    Wav_FormatChunk->ChunkSize = 16;      // Always 16 if PCM samples, which is what we support
    Wav_FormatChunk->AudioFormat = 1;    // Always 1 if PCM
    
    uint16 NumChannels = (uint16)Conv_CommonChunk->NumChannels;
    uint32 SampleRate = (uint32)Conv_CommonChunk->SampleRate;
    uint16 BytesPerSample = (Conv_CommonChunk->SampleSize / BITS_IN_BYTE);
    uint32 ByteRate = SampleRate * NumChannels * BytesPerSample;
    uint16 BlockAlign = (NumChannels * BytesPerSample);
    uint16 BitsPerSample = Conv_CommonChunk->SampleSize;

    Wav_FormatChunk->NumChannels = NumChannels;
    Wav_FormatChunk->SampleRate = SampleRate;    
    Wav_FormatChunk->ByteRate = ByteRate;       
    Wav_FormatChunk->BlockAlign = BlockAlign;   
    Wav_FormatChunk->BitsPerSample = BitsPerSample;

    // Todo: Pull some of the loop below out into functions 
    for(int UnChunkIdx = 0; UnChunkIdx < Global_CountOfUnimportantChunks; UnChunkIdx++)
    {
	// Loop through the unimportant chunks we found in the .aif again, 
	//    but now that we've converted their data, write the contents 
	//    to the .wav buffer
	chunk_finder ThisChunk = Global_UnChunkDirectory[UnChunkIdx];

	switch(ThisChunk.HashedID)
	{
	    case MARKER_CHUNK:
	    {
		wav_sampler_chunk *Wav_SamplerChunk = 
		    PushStruct(WavBuffer, wav_sampler_chunk);

		// There's a bunch of fields we don't care about here; set them to 0
		Wav_SamplerChunk->Manufacturer = 0;
		Wav_SamplerChunk->Product = 0;
		Wav_SamplerChunk->SmpteFormat = 0;
		Wav_SamplerChunk->SmpteOffset = 0;
		Wav_SamplerChunk->SamplerData = 0;

		// Fill out the rest of the chunk
		CopyIDToWav("smpl", Wav_SamplerChunk->ID);
		int SizeOfSampleLoopBytes = ( Global_NumSampleLoops * sizeof(wav_sample_loop) );

		// ChunkSize is always:
		//    (36 for the header) + number of bytes needed for sample loop chunks
		uint32 ChunkSize = (36 + SizeOfSampleLoopBytes); 

		// (in nanoseconds)
		uint32 SamplePeriod = (1000000000 / SampleRate);

		Wav_SamplerChunk->ChunkSize = ChunkSize;
		Wav_SamplerChunk->SamplePeriod = SamplePeriod;
		Wav_SamplerChunk->NumSampleLoops = Global_NumSampleLoops;

		// Copy some data from the converted .aif Instrument chunk 
		//    that belongs in the .wav Sampler chunk.
		//
		//    Todo: Test to see if we found a Marker chunk but not an
		//    Instrument chunk in the .aif, which would probably 
		//    mean a corrupted file. For now, we assume we found one:
		conv_instrument_chunk *Conv_InstrumentChunk = 
		    (conv_instrument_chunk *)GetConvertedChunkPointer(INSTRUMENT_CHUNK);

		    Wav_SamplerChunk->MidiUnityNote = 
				    (uint32)Conv_InstrumentChunk->BaseNote;
		    Wav_SamplerChunk->MidiPitchFraction =
				    (uint32)Conv_InstrumentChunk->Detune;
		
		// Write the loop info we found in the .aif into the .wav
		conv_loop Conv_SustainLoop = Conv_InstrumentChunk->SustainLoop;
		if(Conv_SustainLoop.PlayMode)
		{
		    wav_sample_loop *Wav_Loop = PushStruct(WavBuffer, wav_sample_loop);
		    ValidateIntegerRange(Conv_SustainLoop.PlayMode,
					    AIF_FWD_LOOP, AIF_FWD_BACK_LOOP,
					    Stringize(SustainLoop.PlayMode),
					    __func__);

		    // Wav_Loop type can only be one of two types:
		    if(Conv_SustainLoop.PlayMode == AIF_FWD_LOOP)
		    {
			Wav_Loop->Type = WAV_FWD_LOOP;
		    }
		    else
		    {
			Wav_Loop->Type = WAV_FWD_BACK_LOOP;
		    }
		
		    // The .wav file now wants some information from the .aif 
		    //	  Marker chunk, not the .aif Instrument chunk we've been using.
		    //	  First, get the SampleFrame where the SustainLoop starts
		    int16 MarkerWeNeed = 
			Conv_InstrumentChunk->SustainLoop.BeginLoopMarker;
		    conv_marker_chunk *Conv_MarkerChunk =
			(conv_marker_chunk *)GetConvertedChunkPointer(MARKER_CHUNK);
		    uint32 Wav_LoopStart = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    Wav_Loop->Start = (int32)Wav_LoopStart;

		    // Do the same, but this time, for where the SustainLoop ends
		    MarkerWeNeed = 
			Conv_InstrumentChunk->SustainLoop.EndLoopMarker;
		    uint32 Wav_LoopEnd = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    Wav_Loop->End = (int32)Wav_LoopEnd;

		    Wav_Loop->Fraction = 0;
		    Wav_Loop->PlayCount = 0; // 0 here means loop until interrupted, 
					    //	  not null.............btw
		}
		
		// If there's a Release loop
		conv_loop Conv_ReleaseLoop = Conv_InstrumentChunk->ReleaseLoop;
		if(Conv_ReleaseLoop.PlayMode)
		{
		    wav_sample_loop *Wav_Loop = PushStruct(WavBuffer, wav_sample_loop);
		    ValidateIntegerRange(Conv_ReleaseLoop.PlayMode,
					    AIF_FWD_LOOP, AIF_FWD_BACK_LOOP,
					    Stringize(ReleaseLoop.PlayMode),
					    __func__);

		    // Wav_Loop type can only be one of two types:
		    if(Conv_ReleaseLoop.PlayMode == AIF_FWD_LOOP)
		    {
			Wav_Loop->Type = WAV_FWD_LOOP;
		    }
		    else
		    {
			Wav_Loop->Type = WAV_FWD_BACK_LOOP;
		    }

		    int16 MarkerWeNeed = 
			Conv_InstrumentChunk->ReleaseLoop.BeginLoopMarker;
		    conv_marker_chunk *Conv_MarkerChunk =
			(conv_marker_chunk *)GetConvertedChunkPointer(MARKER_CHUNK);
		    uint32 Wav_LoopStart = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    Wav_Loop->Start = (int32)Wav_LoopStart;

		    MarkerWeNeed = 
			Conv_InstrumentChunk->ReleaseLoop.EndLoopMarker;
		    uint32 Wav_LoopEnd = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    Wav_Loop->End = (int32)Wav_LoopEnd;

		    Wav_Loop->Fraction = 0;
		    Wav_Loop->PlayCount = 0; 
		}
	    } break;

	    // todo: fix the nomenclature here: instrument/inst
	    case INSTRUMENT_CHUNK:
	    {
		wav_inst_chunk *Wav_InstChunk = PushStruct(WavBuffer, wav_inst_chunk);
		conv_instrument_chunk *Conv_InstrumentChunk = 
		    (conv_instrument_chunk *)GetConvertedChunkPointer(INSTRUMENT_CHUNK);

		CopyIDToWav("inst", Wav_InstChunk->ID);
		Wav_InstChunk->ChunkSize = 7; // Always 7
		Wav_InstChunk->UnshiftedNote = (uint8)Conv_InstrumentChunk->BaseNote;
		Wav_InstChunk->FineTune = Conv_InstrumentChunk->Detune;
		Wav_InstChunk->Gain = (int8)Conv_InstrumentChunk->Gain;
		Wav_InstChunk->LowNote = (uint8)Conv_InstrumentChunk->LowNote;
		Wav_InstChunk->HighNote = (uint8)Conv_InstrumentChunk->HighNote;
		Wav_InstChunk->LowVelocity = (uint8)Conv_InstrumentChunk->LowVelocity;
		Wav_InstChunk->HighVelocity = (uint8)Conv_InstrumentChunk->HighVelocity;
		// Needs a pad byte for even-byte alignment
		Wav_InstChunk->PadByte = 0;

	    } break;

	    default:
	    {
		// We only handle the two chunks critical for sampler playback
		//    for now
		continue;
	    } break;
	}
    }

    // .wav Data chunk that contains the actual samples
    wav_data_chunk *Wav_DataChunk = PushStruct(WavBuffer, wav_data_chunk);
    CopyIDToWav("data", Wav_DataChunk->ID);
    int BytesToWrite = (Conv_CommonChunk->NumSampleFrames * 
			Conv_CommonChunk->NumChannels * 
			(Conv_CommonChunk->SampleSize / BITS_IN_BYTE));

    Wav_DataChunk->ChunkSize = (uint32)BytesToWrite;

    // Flip the endianness of the samples and copy them into the .wav file
    int BytesWritten = 0;
    switch(Conv_CommonChunk->SampleSize)
    {
	// For now we only support the two most common bit-depths:
	//	  16-bit and 24-bit
	case 16:
	{
	    BytesWritten = 
		FlipSampleEndianness16Bits(Conv_CommonChunk, Wav_DataChunk->Samples, 
					    Conv_SoundDataChunk->Aif_SamplesStart);
	} break;

	case 24:
	{
	    BytesWritten = 
		FlipSampleEndianness24Bits(Conv_CommonChunk, Wav_DataChunk->Samples, 
					    Conv_SoundDataChunk->Aif_SamplesStart);
	} break;

	default:
	{
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		    "\nERROR:\n\t"
		    "In function"
		    "\n\t\t%s"
		    "\n\tExpected sample size of either 16 or 24"
		    "but read"
		    "\n\t\t%d"
		    "\n\tinstead",
		    __func__, Conv_CommonChunk->SampleSize);
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);

	} break;
    }

    Assert(BytesWritten == BytesToWrite);
    
    // Now write WavBuffer to disk.
    //	  First, change the input file's extension to .wav
    char WavFilename[MAX_STRING_LEN + 1];
    strncpy_s(WavFilename, Aif_Filename, (MAX_STRING_LEN + 1));
    char *Dot = strrchr(WavFilename, '.');
    char *FileExtension = "wav";
    strncpy_s((Dot + 1), (MAX_STRING_LEN + 1), FileExtension, strlen(FileExtension));
    
    // Write the file
    HANDLE WavFileHandle = CreateFileA(WavFilename, 
					(FILE_GENERIC_READ | FILE_APPEND_DATA),
					(FILE_SHARE_READ | FILE_SHARE_WRITE), 
					0, CREATE_ALWAYS, 0, 0); 
    if(WavFileHandle != INVALID_HANDLE_VALUE)
    {
	// Todo: Error handling
        bool32 WriteResult = WriteFile(WavFileHandle, WavFileStarts, 
					Global_BytesNeededForWav, 
					0, 0);
	if(!WriteResult)
	{
	    OutputDebugStringA("failed to write samples\n");
	    exit(1);
	}
    }
    else
    {
	OutputDebugStringA("failed to create wav file\n");
	exit(1);
    }

    printf("Conversion successful. Please find the .wav file\n\n"
	    "  %s\n\n"
	    "in the directory named \"data\".\n\n", WavFilename);

     LARGE_INTEGER StopwatchEnd;
     QueryPerformanceCounter(&StopwatchEnd);   
     real32 LapTime = ((real32)(StopwatchEnd.QuadPart - StopwatchStart.QuadPart) /
			(real32)Global_PerfCountFrequency);
     LapTime = LapTime * 1000.0f;

     uint64 CountCyclesEnd = __rdtsc();
     uint64 CyclesElapsed = CountCyclesEnd - CountCyclesStart;
     real64 MillionsOfCyclesElapsed = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

     char LapTimePrintBuffer[MAX_STRING_LEN];
     sprintf_s((char *)LapTimePrintBuffer, sizeof(LapTimePrintBuffer),
		"\n\nTotal wall clock time passed: %.02f milliseconds\n"
		"Cycles elapsed: %.02f million\n\n", LapTime, MillionsOfCyclesElapsed);

     OutputDebugStringA(LapTimePrintBuffer);

    return(0);
}
