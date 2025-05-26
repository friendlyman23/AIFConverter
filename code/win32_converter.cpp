#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "converter.h"
#include "converter.cpp"

int WinMain(HINSTANCE Instance, 
        HINSTANCE PrevInstance, 
        PSTR CmdLine, 
        int CmdShow)
{
    // Load .aif file
    LPCWSTR Aif_Filename = L"SC88PR~1.AIF";
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
    arena *Scratchpad = ArenaAlloc(Megabytes(1));

    // Declare pointer into Aif file that we move around as we parse. Hereafter,
    //	  any variable prefixed "Aif_" refers to data in the .aif file that
    //	  we're converting
    uint8 *Aif_FileIndex = Aif_FileStart;

    // Every .aif file that meets spec starts with a Form chunk; if we don't
    //	  find it, the Form chunk parsing function exits the program
    converted_form_chunk *Converted_FormChunk = PushStruct(Scratchpad, converted_form_chunk);
    int64 Aif_FileSize_IntVersion = (int64) Aif_FileSize.QuadPart;
    ParseFormChunk(Aif_FileIndex, Converted_FormChunk, Aif_FileSize_IntVersion);
    Aif_FileIndex = Converted_FormChunk->Aif_SubChunksStart;

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
    converted_common_chunk *Converted_CommonChunk = PushStruct(Scratchpad, converted_common_chunk);
    converted_sound_data_chunk *Converted_SoundDataChunk = PushStruct(Scratchpad, converted_sound_data_chunk);
    
    // We'll likely find unimportant chunks too, but we don't parse them until
    //	  after we've done our initial scan and validated the file. So for now
    //	  we just keep a count of how many we find and store their addresses.
    chunk_finder *ChunkFinder = PushArray(Scratchpad, chunk_finder, MAX_UNIMPORTANT_CHUNKS);
    int CountOfUnimportantChunks = 0;
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
	    ParseCommonChunk(Aif_FileIndex, Converted_CommonChunk);
	}
	// If it's the Sound Data chunk, parse it
	else if(HashedID == SOUND_DATA_CHUNK)
	{
	    ParseSoundDataChunk(Aif_FileIndex, Converted_SoundDataChunk);
	}
	
	// Otherwise it's an unimportant chunk. For now we just store the
	//    address; we don't parse it until we've validated the file
	else if(GPerfIDLookup(Aif_ThisChunksHeader->ID, ID_WIDTH))
	{
	    if( CountOfUnimportantChunks < (MAX_UNIMPORTANT_CHUNKS - 1) )
	    {
		ChunkFinder[CountOfUnimportantChunks].HashedID = HashedID;
		ChunkFinder[CountOfUnimportantChunks].Aif_Chunk = Aif_FileIndex;
		CountOfUnimportantChunks++;
	    }
	    else
	    {
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		    "\nERROR:\n\t"
		    "\n\t\t.aif file contains more than %d unimportant"
		    "\n\t\tchunks, which is beyond what this program expected."
		    "\n\nThis program will now exit.", MAX_UNIMPORTANT_CHUNKS);
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
    ValidateAif(CountOfEachChunkType, Converted_CommonChunk, Converted_SoundDataChunk);
    
    // Loop through the array of unimportant chunk pointers we stored 
    //	  and convert the chunks' data
    for(int i = 0; i < CountOfUnimportantChunks; i++)
    {
	// Figure out which chunk we're looking at
	chunk_finder ThisChunk = ChunkFinder[i];

	switch(ThisChunk.HashedID)
	{
	    case MARKER_CHUNK:
	    {
		converted_marker_chunk *Converted_MarkerChunk = PushStruct(Scratchpad, converted_marker_chunk);
		ChunkFinder[i].Converted_Chunk = (uint8 *)Converted_MarkerChunk;
		ParseMarkerChunk(ThisChunk.Aif_Chunk, Converted_MarkerChunk);
		if(Converted_MarkerChunk->TotalMarkers > 0)
		{
		    Converted_MarkerChunk->Converted_MarkersStart = 
				PushArray(Scratchpad, converted_marker, 
					    Converted_MarkerChunk->TotalMarkers);
		    ParseMarkers(Converted_MarkerChunk, Scratchpad, Converted_CommonChunk->NumSampleFrames);
		}
	    } break;

	    case INSTRUMENT_CHUNK:
	    {
		converted_instrument_chunk *Converted_InstrumentChunk = 
				    PushStruct(Scratchpad, converted_instrument_chunk);
		ChunkFinder[i].Converted_Chunk = (uint8 *)Converted_InstrumentChunk;
		ParseInstrumentChunk(ThisChunk.Aif_Chunk, Converted_InstrumentChunk);
	    } break;

	    case FILLER_CHUNK: 
	    {
		filler_chunk *FillerChunk = PushStruct(Scratchpad, filler_chunk);
		ChunkFinder[i].Converted_Chunk = (uint8 *)FillerChunk;
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
    // stop: last step is to output .wav file

    



    




    return(0);
}






    













