#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "converter.h"
#include "converter.cpp"

#define MAX_UNIMPORTANT_CHUNKS 25

struct arena
{
    uint64 ArenaSize;
    uint8 *DoNotCrossThisLine;
    uint8 *NextFreeByte;
    uint8 ArenaStart[];
};

arena *
ArenaAlloc(uint64 Size)
{
    arena *Arena = (arena *)Win32_AllocateMemory(Size, __func__);
    Arena->ArenaSize = Size;
    Arena->DoNotCrossThisLine = ( ((uint8 *)Arena + Arena->ArenaSize) - 1);
    Arena->NextFreeByte = Arena->ArenaStart;
    return(Arena);
}

void *
ArenaPush(arena *Arena, uint64 Size)
{
    if( (Arena->NextFreeByte + Size) > Arena->DoNotCrossThisLine)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"Push of %llu bytes to arena %s\n\t"
		"exceeds %s's bounds\n",
		Size, Stringize(Arena), Stringize(Arena)
		);
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
    else
    {
	void *TheBytesMyLordHathRequested = (void *)Arena->NextFreeByte;
	Arena->NextFreeByte += Size;
	return(TheBytesMyLordHathRequested);
    }
} 

#define PushArray(Arena, Type, Count) ((Type *)ArenaPush( (Arena), sizeof(Type)*(Count)))
#define PushStruct(Arena, Type) PushArray( Arena, Type, 1 )

int WinMain(HINSTANCE Instance, 
        HINSTANCE PrevInstance, 
        PSTR CmdLine, 
        int CmdShow)
{
    // Load .aif file
    LPCWSTR Aif_Filename = L"SC88PR~1.AIF";
    uint8 *Aif_FileStart;

    HANDLE HeapHandle = GetProcessHeap();

    if(HeapHandle)
    {
        Aif_FileStart = (uint8 *)Win32_GetAifFilePointer(Aif_Filename);
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
    form_chunk *FormChunk = PushStruct(Scratchpad, form_chunk);
    int BytesReadInFormChunk = ParseFormChunk(Aif_FileIndex, FormChunk);
    Aif_FileIndex = AdvancePointer(Aif_FileStart, BytesReadInFormChunk);

    // Excepting a very few edge cases, .aif files that meet spec will 
    //	  almost always have one Common chunk and one Sound Data chunk 
    //	  (hereafter referred to as "important chunks").
    //
    //	  So we next look for these, and as we scan the file for them, store 
    //	  pointers to other chunks we find along the way (these are 
    //	  hereafter referred to as "UNimportant chunks").
    uint8 *LastByteInFile = (Aif_FileStart + CHUNK_HEADER_BOILERPLATE + FormChunk->ChunkSize);

    // We're gonna need the Common and Sound Data chunks if the .aif file meets 
    //	  spec, so just allocate for them here and then we can parse them as soon as
    //	  we find them in the while loop below
    common_chunk *CommonChunk = PushStruct(Scratchpad, common_chunk);
    sound_data_chunk *SoundDataChunk = PushStruct(Scratchpad, sound_data_chunk);
    
    // We'll likely find unimportant chunks too, but we don't parse them until
    //	  after we've done our initial scan and validated the file. So for now
    //	  we just keep a count of how many we find and store their addresses.
    uint8 **Aif_UnimportantChunkAddresses = PushArray(Scratchpad, uint8*, MAX_UNIMPORTANT_CHUNKS);
    int CountOfUnimportantChunks = 0;
    int CountOfEachChunkType[HASHED_CHUNK_ID_ARRAY_SIZE] = {0};


    while(Aif_FileIndex < LastByteInFile)
    {
	// Figure out which chunk we're looking at
	generic_chunk_header *Aif_ThisChunksHeader = (generic_chunk_header *)Aif_FileIndex;
	chunk HashedChunkID = (chunk)GPerfHasher(Aif_ThisChunksHeader->ID, ID_WIDTH);

	// If we encounter a second Form chunk, the file is busted, so exit
	if(HashedChunkID == FORM_CHUNK)
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

	// If it's a Common or Sound Data chunk, parse it
	else if(HashedChunkID == COMMON_CHUNK)
	{
	    ParseCommonChunk(Aif_FileIndex, CommonChunk);
	}
	else if(HashedChunkID == SOUND_DATA_CHUNK)
	{
	    ParseSoundDataChunk(Aif_FileIndex, SoundDataChunk);
	}
	
	// Otherwise it's an unimportant chunk, so store the address so we can
	//    parse it after validating the file
	else
	{
	    if( CountOfUnimportantChunks < (MAX_UNIMPORTANT_CHUNKS - 1) )
	    {
		Aif_UnimportantChunkAddresses[CountOfUnimportantChunks] = Aif_FileIndex;
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
	CountOfEachChunkType[HashedChunkID]++;
	Aif_FileIndex = Aif_ThisChunksHeader->Data;
	Aif_FileIndex += FlipEndianness(Aif_ThisChunksHeader->ChunkSize);
    }


    // Validate file
    ValidateAif(CountOfEachChunkType, CommonChunk, SoundDataChunk);
    
    // Store a pointer to the array we're about to fill in
    uint8 **ConvertedUnimportantChunks = PushArray(Scratchpad, uint8*, CountOfUnimportantChunks);

    // Loop through the array of unimportant chunk pointers we stored and convert them
    for(int i = 0; i < CountOfUnimportantChunks; i++)
    {
	// Figure out which chunk we're looking at
	generic_chunk_header *Aif_ThisChunksHeader = 
				(generic_chunk_header *)Aif_UnimportantChunkAddresses[i];
	chunk HashedChunkID = (chunk)GPerfHasher(Aif_ThisChunksHeader->ID, ID_WIDTH);
	
	switch(HashedChunkID)
	{
	    case MARKER_CHUNK:
	    {
		marker_chunk *Aif_MarkerChunk = (marker_chunk *)Aif_ThisChunksHeader;
		marker_chunk *MarkerChunk = PushStruct(Scratchpad, marker_chunk);
		ParseMarkerChunk(Aif_MarkerChunk, MarkerChunk);
	    }
	}



	
    }







    // Output .wav

    



    




    return(0);
}






    













