#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include "converter.h"
#include "converter.cpp"

int    Global_CountOfUnimportantChunks = 0;
uint32 Global_BytesNeededForWav = 0;
int    Global_NumSampleLoops = 0;
chunk_finder *Global_UnChunkDirectory = 0;

#define AIF_NO_LOOP 0
#define AIF_FWD_LOOP 1
#define AIF_FWD_BACK_LOOP 2
#define WAV_FWD_LOOP 0
#define WAV_FWD_BACK_LOOP 1

extern "C"
{
    unsigned int
    GPerfHasher (register const char *str, register size_t len);

    const char *
    GPerfIDLookup (register const char *str, register size_t len);
}

#pragma pack(push, 1)
struct wav_riff_chunk
{
    char ID[4];          // 'RIFF'
    uint32 ChunkSize;    // Size of the entire file minus 8 bytes
    char Format[4];      // 'WAVE'
};

struct wav_fmt_chunk
{
    char ID[4];            // 'fmt '
    uint32 ChunkSize;      // Size of the rest of this chunk (typically 16 for PCM)
    uint16 AudioFormat;    // PCM = 1, others for compression
    uint16 NumChannels;    // Mono = 1, Stereo = 2, etc.
    uint32 SampleRate;     // 44100, 48000, etc.
    uint32 ByteRate;       // SampleRate * NumChannels * BitsPerSample / 8
    uint16 BlockAlign;     // NumChannels * BitsPerSample / 8
    uint16 BitsPerSample;  // 8, 16, 24, etc.
};

struct wav_data_chunk
{
    char ID[4];          // 'data'
    uint32 ChunkSize;    // Number of bytes of sample data
    uint8 Samples[];
};

struct wav_inst_chunk
{
    char ID[4];               // 'inst'
    uint32 ChunkSize;         // Should be 7 bytes
    uint8 UnshiftedNote;      // MIDI root note
    int8 FineTune;            // -50 to +50 cents
    int8 Gain;                // -64 to +64 dB
    uint8 LowNote;            // Lowest note played
    uint8 HighNote;           // Highest note played
    uint8 LowVelocity;        // Lowest velocity played
    uint8 HighVelocity;       // Highest velocity played
    uint8 PadByte;
};

struct wav_sampler_chunk 
{
    char ID[ID_WIDTH];
    uint32 ChunkSize;
    uint32 Manufacturer;
    uint32 Product;
    uint32 SamplePeriod;
    uint32 MidiUnityNote;
    uint32 MidiPitchFraction;
    uint32 SmpteFormat;
    uint32 SmpteOffset;
    uint32 NumSampleLoops;
    uint32 SamplerData;
};

struct wav_sample_loop 
{
    uint32 CuePointID;
    uint32 Type;          
    uint32 Start;         
    uint32 End;           
    uint32 Fraction;
    uint32 PlayCount;     
};
#pragma pack(pop)

int WinMain(HINSTANCE Instance, 
        HINSTANCE PrevInstance, 
        PSTR CmdLine, 
        int CmdShow)
{
    (void)Instance; (void)PrevInstance; (void)CmdLine; (void)CmdShow;

    // Load .aif file
    LPCWSTR Aif_Filename = L"AIF_SC~1.AIF";
    uint8 *Aif_FileStart;

    HANDLE HeapHandle = GetProcessHeap();
    LARGE_INTEGER Aif_FileSize;
    Aif_FileSize.QuadPart = 0;

    if(HeapHandle)
    {
        Aif_FileStart = (uint8 *)Win32_GetAifFilePointer(Aif_Filename, &Aif_FileSize);
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
    //	  (hereafter referred to as "important chunks").
    //
    //	  So we next look for these, and as we scan the file for them, store 
    //	  pointers to other chunks we find along the way (these are 
    //	  hereafter referred to as "UNimportant chunks").

    // 
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

    // Output .wav
    int WavBufferSize = ARENA_BOILERPLATE + Global_BytesNeededForWav;
    arena *WavBuffer = ArenaAlloc(WavBufferSize);
    // We have some meta info at the start of the arena. When we write
    //	  the file, we need the address of the first byte after the
    //	  meta info -- that's the address we pass into WriteFile
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
		    wav_sample_loop *WavLoop = PushStruct(WavBuffer, wav_sample_loop);
		    ValidateIntegerRange(Conv_SustainLoop.PlayMode,
					    AIF_FWD_LOOP, AIF_FWD_BACK_LOOP,
					    Stringize(SustainLoop.PlayMode),
					    __func__);

		    // WavLoop type can only be one of two types:
		    if(Conv_SustainLoop.PlayMode == AIF_FWD_LOOP)
		    {
			WavLoop->Type = WAV_FWD_LOOP;
		    }
		    else
		    {
			WavLoop->Type = WAV_FWD_BACK_LOOP;
		    }
		

		    // The .wav file now wants some information from the .aif 
		    //	  Marker chunk, not the .aif Instrument chunk we've been using.
		    //	  First, get the SampleFrame where the SustainLoop starts
		    int16 MarkerWeNeed = 
			Conv_InstrumentChunk->SustainLoop.BeginLoopMarker;
		    conv_marker_chunk *Conv_MarkerChunk =
			(conv_marker_chunk *)GetConvertedChunkPointer(MARKER_CHUNK);
		    uint32 WavLoopStart = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    WavLoop->Start = (int32)WavLoopStart;

		    // Do the same, but this time, for where the SustainLoop ends
		    MarkerWeNeed = 
			Conv_InstrumentChunk->SustainLoop.EndLoopMarker;
		    uint32 WavLoopEnd = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    WavLoop->End = (int32)WavLoopEnd;

		    WavLoop->Fraction = 0;
		    WavLoop->PlayCount = 0; // 0 here means loop until interrupted, 
					    //	  not null.............btw
		}
		
		// If there's a Release loop
		conv_loop Conv_ReleaseLoop = Conv_InstrumentChunk->ReleaseLoop;
		if(Conv_ReleaseLoop.PlayMode)
		{
		    wav_sample_loop *WavLoop = PushStruct(WavBuffer, wav_sample_loop);
		    ValidateIntegerRange(Conv_ReleaseLoop.PlayMode,
					    AIF_FWD_LOOP, AIF_FWD_BACK_LOOP,
					    Stringize(ReleaseLoop.PlayMode),
					    __func__);

		    // WavLoop type can only be one of two types:
		    if(Conv_ReleaseLoop.PlayMode == AIF_FWD_LOOP)
		    {
			WavLoop->Type = WAV_FWD_LOOP;
		    }
		    else
		    {
			WavLoop->Type = WAV_FWD_BACK_LOOP;
		    }

		    int16 MarkerWeNeed = 
			Conv_InstrumentChunk->ReleaseLoop.BeginLoopMarker;
		    conv_marker_chunk *Conv_MarkerChunk =
			(conv_marker_chunk *)GetConvertedChunkPointer(MARKER_CHUNK);
		    uint32 WavLoopStart = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    WavLoop->Start = (int32)WavLoopStart;

		    // Do the same, but this time, for where the ReleaseLoop ends
		    MarkerWeNeed = 
			Conv_InstrumentChunk->ReleaseLoop.EndLoopMarker;
		    uint32 WavLoopEnd = 
			GetSampleFramePosition(MarkerWeNeed, Conv_MarkerChunk);
		    WavLoop->End = (int32)WavLoopEnd;

		    WavLoop->Fraction = 0;
		    WavLoop->PlayCount = 0; 
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
		// We only handle the two chunks above for now
		continue;
	    } break;
	}
    }

    // Finally, write the .wav Data chunk
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
    
    // Write WavBuffer to disk
    LPCWSTR WavFileName = L"WAVTEST.WAV";
    HANDLE WavFileHandle = CreateFileW(WavFileName, //lpFilename
            (FILE_GENERIC_READ | FILE_APPEND_DATA), //dwDesiredAccess
            (FILE_SHARE_READ | FILE_SHARE_WRITE), //dwShareMode
            0, //lpSecurityAttributes
            CREATE_ALWAYS, //dwCreationDisposition
            0, //dwFlagsAndAttributes
            0); //htemplate file
    if(WavFileHandle != INVALID_HANDLE_VALUE)
    {
        bool32 WriteResult = WriteFile(WavFileHandle, WavFileStarts, 
					Global_BytesNeededForWav, 
					0, 0);
	if(!WriteResult)
	{
	    OutputDebugStringA("failed to write samples\n");
	    /*DWORD LastError = GetLastError();*/
	    /*OutputDebugStringA("failed to write samples\n\nError: %d\n\n", LastError);*/
	    
	}
    }
    else
    {
	OutputDebugStringA("failed to create wav file\n");
    }

    return(0);
}






    













