#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include "converter.h"
#include "converter.cpp"

/* TODO:
 *    1. For chunk types that should only appear once in a file, assert one doesn't already exist
 *	    when we start reading
 *
 *	    Chunk types that can only appear once, per the spec:
 *	      - Common chunk
 *	      - Marker chunk
 *	      - Sound Data chunk
 *	      - Instrument chunk
 *	      - Audio Recording chunk
 *	      - Comments chunk
 *	      - Text chunks:
 *		  - Name chunk
 *		  - Author chunk
 *		  - Copyright chunk
 *
 *	  Chunk types that must appear:
 *
 *	      - Common chunk
 *	      - Sound Data chunk, IF sampled sound > 0
 *    
 *    2. Use Windows data types in the platform layer
 *
 *    3. test InstrumentChunk loop parsers with nonzero data
 *    
 *    4. update ints for wav headers to be signed unless
 *	    they're 8 bits wide
 *
 *    5. Check to see that the functions we try to inline actually get inlined when
 *	  we turn on optimization
 *
 *    6. Update all calls to HeapAlloc to use Win32_AllocateMemory
 *
 *    7. Check the Marker function to see if I completely wasted my time. cool
 *
 *    8. Update all OutputDebugString calls to use the error-reporting functions
 *
 *    9. Decide whether to use strcmp or strncmp version of GPerf
 *
 *    10. Use max_string_len define to test for buffer overrun
 *
 *    11. Make ValidateAifFile checks more rigorous
 *
 *    12. Insert a check as we read each chunk field that the data is in the format
 *	  we expect; e.g., that chunk sizes are above the minimum possible number of
 *	  bytes for that chunk
 *
 *    13. Update ValidateInteger functions to print the name of the variable storing
 *	  the integer
 *
 *    14. Update ValidateAifFile to check for multiple Form chunks
 */

int WinMain(HINSTANCE Instance, 
        HINSTANCE PrevInstance, 
        PSTR CmdLine, 
        int CmdShow)
{
    LPCWSTR Aif_Filename = L"SC88PR~1.AIF";
    uint8 *Aif_FileStart;

    //INIT
    //	  Load .aif file
    //	  Allocate memory
    //	  Structure memory
    //	  Declare count array
    //	  Declare Aif_Index pointer

    //	  Load .aif file
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

    //	  Allocate 1 mb of memory
    int Aif_SpaceForImportantChunkAddresses = sizeof(aif_important_chunk_addresses);
    int AmountOfChunkMemory = ( 
				    Aif_SpaceForImportantChunkAddresses + 
				    ( Megabytes(1) - Aif_SpaceForImportantChunkAddresses )
			      );

    uint8 *ChunkMemoryStart = (uint8 *)Win32_AllocateMemory(AmountOfChunkMemory, __func__);

    //	  Structure chunk memory
    /* TODO: PROFILE AND SEE IF SHRINKING THE CHUNK MEMORY ALLOC TO FIT IN CACHE MAKES
     *	  IT FASTER */

    aif_important_chunk_addresses *Aif_ImportantChunkAddresses = 
				    (aif_important_chunk_addresses *)ChunkMemoryStart;

    uint8 **Aif_UnimportantChunks = (uint8 **)( ChunkMemoryStart + ( sizeof(aif_important_chunk_addresses) ) );
    
    //	  Declare count array
    int CountOfEachChunkType[HASHED_CHUNK_ID_ARRAY_SIZE] = {0};

    //PREPROCESS
    //	  Declare Aif_Index pointer
    //	  Process the Form chunk
    //	  Scan for the other chunks and store their pointers

    uint8 *Aif_FileIndex = Aif_FileStart;
    int Aif_FileBytesRead = 0;

    form_chunk FormChunk = {};
    uint8 *Aif_FormChunk_Start = (uint8 *)Aif_FileIndex;
    Aif_ImportantChunkAddresses->FormChunkAddress = Aif_FormChunk_Start;
    int Aif_FormChunk_BytesRead = App_Parse_Aif_Chunk(Aif_FormChunk_Start, &FormChunk);
    Aif_FileBytesRead += Aif_FormChunk_BytesRead;
    Aif_FileIndex = AdvancePointer(Aif_FileStart, Aif_FileBytesRead);

    uint8 *LastByteInFile = (Aif_FileStart + CHUNK_HEADER_BOILERPLATE + FormChunk.ChunkSize);

    int CountOfUnimportantChunks = 0;
    while(Aif_FileIndex < LastByteInFile)
    {
	aif_generic_chunk_header *ThisChunksHeader = (aif_generic_chunk_header *)Aif_FileIndex;
	unsigned int HashedChunkID = GPerfHasher(ThisChunksHeader->ID, ID_WIDTH);
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
	else if(HashedChunkID == COMMON_CHUNK)
	{
	    Aif_ImportantChunkAddresses->CommonChunkAddress = Aif_FileIndex;
	}
	else if(HashedChunkID == SOUND_DATA_CHUNK)
	{
	    Aif_ImportantChunkAddresses->SoundDataChunkAddress = Aif_FileIndex;
	}
	else
	{
	    Aif_UnimportantChunks[CountOfUnimportantChunks] = Aif_FileIndex;
	    CountOfUnimportantChunks++;
	}
	    CountOfEachChunkType[HashedChunkID]++;
	    // Skip over the rest of the chunk
	    Aif_FileIndex = ThisChunksHeader->Data;
	    Aif_FileIndex += FlipEndianness(ThisChunksHeader->ChunkSize);
    }

    // Check COMM appears once and only once
    // If COMM.NumSampleFrames > 0
    //	  Check SSND appears once and only once
    //	  and check SSND.ChunkSize > 8

    if(CountOfEachChunkType[COMMON_CHUNK] == 0)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"\n\t\tThis .aif file does not contain a Common chunk."
		"\n\t\tSince all .aif files must have one,"
		"\n\t\tyour .aif file appears to be corrupted."
		"\n\nThis program will now exit.");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
    if(CountOfEachChunkType[COMMON_CHUNK] > 1)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"\n\t\tThis .aif file contains more than one Common chunk,"
		"\n\t\twhich is not permitted by the .aif specification."
		"\n\t\tTherefore, your .aif file appears to be corrupted."
		"\n\nThis program will now exit.");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
    // STOP: Midstream on validation code

    // If we make it here, there must be exactly one Common chunk

    // If the Common Chunk says the file has sample frames, then there must be
    //	  a Sound Data chunk
#if 0
    if( (CommonChunk->NumSampleFrames > 0) && (TimesChunkAppears[SOUND_DATA_CHUNK] == 0) )
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"\n\t\tYour .aif file reports that it contains sound samples,"
		"\n\t\tbut this program was unable to locate the Sound Data chunk"
		"\n\t\twhere sound samples are stored."
		"\n\t\tTherefore, your .aif file appears to be corrupted."
		"\n\nThis program will now exit.");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }

    // There can only be one Sound Data chunk if one exists at all
    else if(CountOfEachChunkType[SOUND_DATA_CHUNK] > 1)
    { 
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"\n\t\tThis .aif file contains more than one Sound Data chunk,"
		"\n\t\twhich is not permitted by the .aif specification."
		"\n\t\tTherefore, your .aif file appears to be corrupted."
		"\n\nThis program will now exit.");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
#endif
    
    // Finally, some other checks to affirm that the data reported by the
    //	  Common and Sound Data chunks are in agreement














    // todo: free the other allocs
    HeapFree(HeapHandle, 0, ChunkMemoryStart);

    return(0);
}
#if 0
    LPCWSTR Aif_Filename = L"SC88PR~1.AIF";
    uint8 *Aif_FileStart;

    // Form chunk
    // Common chunk		    Highest precedence
    // Sound Data chunk
    // Marker chunk
    // Instrument chunk
    // Comment chunk
    // Name chunk
    // Author chunk
    // Copyright chunk
    // Annotation chunk
    // Audio Recording chunk
    // MIDI Data chunk
    // Application Specific chunk   Lowest precedence

    if(HeapHandle)
    {
        Aif_FileStart = (uint8 *)Win32_GetFilePointer(Aif_Filename);
    }
    else
    {
	// if we somehow didn't get the heap handle...
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"At program start, failed to get HeapHandle, so exited");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }

    uint64 Aif_FileBytesRead = 0;
    uint8 *Aif_FileIndex = Aif_FileStart;

    // Read Form chunk.
    //	  Per the .aif spec, all .aif files must begin with the Form chunk.
    //	  The parse function for the Form chunk called below prints an 
    //	  error message and exits the program if the first four bytes
    //	  of the .aif file do not contain the string "FORM".
    //
    //	  i.e., if we don't get the Form chunk at the very start here, we 
    //	  exit right away.
    uint8 *Aif_FormChunk_Start = (uint8 *)Aif_FileIndex;
    form_chunk FormChunk = {};
    int Aif_FormChunk_BytesRead = App_Parse_Aif_Chunk(Aif_FormChunk_Start, &FormChunk);
    Aif_FileBytesRead += Aif_FormChunk_BytesRead;
    Aif_FileIndex = AdvancePointer(Aif_FileStart, Aif_FileBytesRead);

    // This array keeps track of how many times each chunk appears in the .aif file.
    //	  Some must appear; others are optional but may only appear once
    //	  if they show up at all.
    int TimesChunkAppears[CHUNK_ID_HASH_ARRAY_SIZE] = {0};

    // If there's a Form chunk, we next scan the file and try to find the Common Chunk
    //	  and the Sound Data chunk, which must be in the correct format for the 
    //	  .aif file to be valid.
    common_chunk CommonChunk = {};
    sound_data_chunk_header SoundDataChunkHeader = {};
    ValidateAifFile(&FormChunk, &CommonChunk, &SoundDataChunkHeader, Aif_FileStart, TimesChunkAppears);
    
    // Read Marker chunk header
    Aif_FileIndex = AdvancePointer(Aif_FileStart, Aif_FileBytesRead);
    marker_chunk_header MarkerChunk_Header = {};
    uint8 *Aif_MarkerChunk_HeaderStart = (uint8 *)Aif_FileIndex;
    int Aif_MarkerChunk_HeaderBytesRead = App_Parse_Aif_Chunk(Aif_MarkerChunk_HeaderStart, 
            &MarkerChunk_Header);
    Aif_FileBytesRead += Aif_MarkerChunk_HeaderBytesRead;

    // Get the lengths of the strings for each Marker so we know how much
    //	  space to put on the heap
    Aif_FileIndex = AdvancePointer(Aif_FileStart, Aif_FileBytesRead);
    uint8 *Aif_MarkerChunk_DataStart = (uint8 *)Aif_FileIndex;
    int BytesNeededToStoreMarkers = App_Parse_Aif_Chunk(Aif_MarkerChunk_DataStart, 
            MarkerChunk_Header.TotalMarkers);

    // Read the Marker chunk data and store it on the heap
    int Aif_MarkerChunkData_BytesRead = App_Parse_Aif_Chunk(Aif_MarkerChunk_DataStart, 
            &MarkerChunk_Header,
            BytesNeededToStoreMarkers);
    Aif_FileBytesRead += Aif_MarkerChunkData_BytesRead;

    // Read the Instrument chunk
    Aif_FileIndex = AdvancePointer(Aif_FileStart, Aif_FileBytesRead);
    instrument_chunk InstrumentChunk = {};
    uint8 *Aif_InstrumentChunk_Start = (uint8 *)Aif_FileIndex;
    int Aif_InstrumentChunk_BytesRead = App_Parse_Aif_Chunk(Aif_InstrumentChunk_Start, &InstrumentChunk);
    Aif_FileBytesRead += Aif_InstrumentChunk_BytesRead;

    // Read the Filler chunk header
    Aif_FileIndex = AdvancePointer(Aif_FileStart, Aif_FileBytesRead);
    filler_chunk_header FillerChunk = {};
    uint8 *Aif_FillerChunk_Start = (uint8 *)Aif_FileIndex;
    int Aif_FillerChunk_HeaderBytesRead = App_Parse_Aif_Chunk(Aif_FileIndex, &FillerChunk);
    Aif_FileBytesRead += Aif_FillerChunk_HeaderBytesRead;

    // Skip the bytes we read in the Filler chunk header
    Aif_FileIndex = AdvancePointer(Aif_FileStart, Aif_FileBytesRead);
    // Now we're pointing at the actual filler bytes. Skip them:
    Aif_FileIndex += FillerChunk.TotalFillerBytes;
    // We may have just skipped thousands of bytes so assert
    //	  we aren't off in no man's land
    ValidatePointer(Aif_FileIndex, __func__);

    sound_data_chunk_header SoundDataChunk_Header = {};
    uint8 *Aif_SoundDataChunk_HeaderStart = Aif_FileIndex;
    int Aif_SoundDataChunk_HeaderBytesRead = App_Parse_Aif_Chunk(Aif_FileIndex, &SoundDataChunk_Header);

    // The rest of the program constructs a new .wav file from what we
    //	  copied from the .aif chunks we parsed. The largest part of the 
    //	  job is flipping the big endian samples in the .aif file to
    //	  the little endian samples required by .wav files.
    //
    //	  Since there may be millions of bytes to flip, they may not fit
    //	  on the stack, so we put them on the heap. First we compute
    //	  how much memory we'll need:
    int32 CommonChunk_BytesPerSample = (CommonChunk.SampleSize / BITS_IN_BYTE);
    int32 LittleEndianSamplesMemorySize = (CommonChunk.NumSampleFrames * 
            CommonChunk.NumChannels * 
            CommonChunk_BytesPerSample);

    // Allocate the memory for the little endian samples
    uint8 *LittleEndianSamplesStart = 
		    (uint8 *)Win32_AllocateMemory(LittleEndianSamplesMemorySize, __func__);
    // Declare a pointer to the big endian samples
    uint8 *BigEndianSamplesStart = SoundDataChunk_Header.SamplesStart;

    /**********************************************************
     *                                                        *
     *  .aif sample data is organized by "Sample Frame."      *
     *     For a typical stereo signal, sampled at 24-bit     *  
     *     resolution, a sample frame might be visually       *
     *     represented like this:                             *
     *  ____________________________________________________  *
     * |                                                    | *
     * |                SAMPLE FRAME (6 bytes)              | *
     * |____________________________________________________| *
     * |                         |                          | *
     * | LEFT CHANNEL (3 bytes)  | RIGHT CHANNEL (3 bytes)  | *    
     * |_________________________|__________________________| *
     *							      *                                                        
     **********************************************************/
    int LittleEndianBytesWritten; 

    switch(CommonChunk.SampleSize)
    {
	// For now we only support the two most common bit-depths:
	//	  16-bit and 24-bit
	case 16:
	{
	    LittleEndianBytesWritten = 
		FlipSampleEndianness16Bits(&CommonChunk, LittleEndianSamplesStart, 
					    BigEndianSamplesStart);
	} break;

	case 24:
	{
	    LittleEndianBytesWritten = 
		FlipSampleEndianness24Bits(&CommonChunk, LittleEndianSamplesStart, 
					    BigEndianSamplesStart);
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
		    __func__, CommonChunk.SampleSize);
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);

	} break;
    }

    /******************************
     * wav file size calculation, bytes:
     *	  	  
     *	  Riff chunk:
     *	    RiffType, 4
     *
     *	  Format chunk:
     *	    FormatChunk_ID, 4    
     *      FormatChunk_ChunkSize, 4
     *      FormatTag, 2
     *	    NumChannels, 2                                                       
     *	    SampleRate, 4
     *	    AvgBytesPerSec, 4
     *	    BlockAlign, 2
     *	    BitsPerSample, 2  
     *
     *	  Data chunk:
     *	    DataChunk_ID, 4
     *	    DataChunk_ChunkSize, (FlippedSamplesByteCount)
     *	  
     ******************************/

    wav_header WavHeader = {};
    SteenCopy((uint8 *)"RIFF", (uint8 *)WavHeader.GroupID, ID_WIDTH);
    SteenCopy((uint8 *)"WAVE", (uint8 *)WavHeader.RiffType, ID_WIDTH);
    SteenCopy((uint8 *)"fmt ", (uint8 *)WavHeader.FormatChunkID, ID_WIDTH);
    SteenCopy((uint8 *)"data", (uint8 *)WavHeader.DataChunkID, ID_WIDTH);
    WavHeader.WavFileSize = HEADER_SIZE_FOR_FILE_SIZE_CALC + LittleEndianSamplesMemorySize;
    WavHeader.NumChannels = CommonChunk.NumChannels;
    WavHeader.SampleRate = CommonChunk.SampleRate;
    WavHeader.BitsPerSample = CommonChunk.SampleSize;

    uint32 WavHeaderBytesPerSample = (WavHeader.BitsPerSample / BITS_IN_BYTE); // for convenience!
    WavHeader.AvgBytesPerSec = WavHeader.SampleRate * 
        WavHeader.NumChannels * 
        WavHeaderBytesPerSample;

    WavHeader.BlockAlign = WavHeader.NumChannels * WavHeaderBytesPerSample;
    WavHeader.DataChunk_ChunkSize = CommonChunk.NumSampleFrames * 
        WavHeader.NumChannels * 
        WavHeaderBytesPerSample;
    Assert(WavHeader.DataChunk_ChunkSize == LittleEndianBytesWritten);

    bool32 HeaderWriteResult = false;  
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
        DWORD HeaderBytesWritten;
        HeaderWriteResult = WriteFile(WavFileHandle, &WavHeader, 44, &HeaderBytesWritten, 0); // write the header
        if(HeaderWriteResult)
        {
            bool32 SamplesWriteResult = false;
            DWORD SampleBytesWritten;
            SamplesWriteResult = WriteFile(WavFileHandle, LittleEndianSamplesStart, 
                    WavHeader.DataChunk_ChunkSize, &SampleBytesWritten, 0);
            DWORD LastError = GetLastError();
            if(!SamplesWriteResult)
            {
                OutputDebugStringA("failed to write samples\n");
            }
        }
        else
        {
            OutputDebugStringA("failed to write wav file header\n");
        }
    }
    else
    {
        OutputDebugStringA("failed to create wav file\n");
    }
    DWORD LastError = GetLastError();

    LARGE_INTEGER WavFileSize;
    LPVOID WavFileVoid;
    GetFileSizeEx(WavFileHandle, &WavFileSize);
    if(WavFileSize.QuadPart)
    {
        Assert(WavFileSize.QuadPart <= 0xFFFFFFFF);

        if(HeapHandle)
        {
            WavFileVoid = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, WavFileSize.QuadPart);
            if(WavFileVoid)
            {
                DWORD NumBytesRead;
                BOOL FileReadSuccessfully = false;
                FileReadSuccessfully = ReadFile(WavFileHandle, WavFileVoid, WavFileSize.QuadPart, &NumBytesRead, 0);
                if(!FileReadSuccessfully)
                {
                    OutputDebugStringA("failed to read wav file\n");
                    LastError = GetLastError();

                    //free if we failed to read the file
                    HeapFree(HeapHandle, 0, WavFileVoid);
                }
            }
            else
            {
                OutputDebugStringA("failed to allocate memory\n");
            }
        }
        else
        {
            OutputDebugStringA("Could not reference HeapHandle when attempting\nto allocate memory for wav file\n");
        }
    }
    else
    {
        OutputDebugStringA("failed to get .aif file size\n");
    }

    uint8 *WavFileAddress = (uint8 *)WavFileVoid;

    CloseHandle(WavFileHandle);
    HeapFree(HeapHandle, 0, LittleEndianSamplesStart);
    HeapFree(HeapHandle, 0, (uint8 *)MarkerChunk_Header.Markers);
    HeapFree(HeapHandle, 0, LittleEndianSamplesStart);

    return(0);
}
#endif
