#include "converter.h"

void
DebugPrintInt(int i)
{
    char PrintBuffer[256];
    sprintf_s(PrintBuffer, sizeof(PrintBuffer), "%d\n", i);
    OutputDebugStringA(PrintBuffer);
}

void
DebugPrintString(char *s)
{
    char PrintBuffer[256];
    sprintf_s(PrintBuffer, sizeof(PrintBuffer), "%s\n", s);
    OutputDebugStringA(PrintBuffer);
}

void
DebugPrintDouble(double d)
{
    char PrintBuffer[256];
    sprintf_s(PrintBuffer, sizeof(PrintBuffer), "%a\n", d);
    OutputDebugStringA(PrintBuffer);
}

LPVOID
Win32_AllocateMemory(uint64 MemSize, char *CallingFunction)
{
    HANDLE HeapHandle = GetProcessHeap();

    LPVOID VoidPointer = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, MemSize);
    if(VoidPointer)
    {
        return(VoidPointer);
    }
    else
    {
        char DebugPrintStringBuffer[MAX_STRING_LEN];
        sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
                "\nERROR:\n\t"
                "In function"
                "\n\t\t%s"
                "\n\tmemory allocation failed",
                CallingFunction);
        OutputDebugStringA((char *)DebugPrintStringBuffer);
        exit(1);
    }
}

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
    if( (Arena->NextFreeByte + Size) > Arena->DoNotCrossThisLine )
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

#define MAX_UNIMPORTANT_CHUNKS 25

#define PushArray(Arena, Type, Count) ( (Type *)ArenaPush( (Arena), sizeof(Type)*(Count) ) )
#define PushStruct(Arena, Type) PushArray(Arena, Type, 1)

// Generic function that we use to advance a pointer through the
//    file by computing sum of the "StartingByte", for example, the
//    start of the .aif file or the start of a particular .aif
//    chunk, and an offset represented by "BytesRead"
inline uint8 *
AdvancePointer(uint8 *StartingByte, int32 BytesRead)
{
    uint8 *NewAddress = (uint8 *)(StartingByte + BytesRead);
    return(NewAddress);
}

void *
Win32_GetAifFilePointer(LPCSTR Filename, LARGE_INTEGER *Aif_FileSize)
{
    LPVOID FileAddress = 0;
    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, 
            OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        GetFileSizeEx(FileHandle, Aif_FileSize);
        if(Aif_FileSize->QuadPart)
        {
            Assert(Aif_FileSize->QuadPart <= 0xFFFFFFFF);

            HANDLE HeapHandle = GetProcessHeap();
            if(HeapHandle)
            {
                FileAddress = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, Aif_FileSize->QuadPart);
                if(FileAddress)
                {
                    DWORD NumBytesRead;
                    BOOL FileReadSuccessfully = false;
                    FileReadSuccessfully = ReadFile(FileHandle, FileAddress, 
			    Aif_FileSize->LowPart, &NumBytesRead, 0);
                    if(!FileReadSuccessfully)
                    {
                        OutputDebugStringA("failed to read file\n");

                        //free if we failed to read the file
                        HeapFree(HeapHandle, 0, FileAddress);
                    }
                }
                else
                {
                    OutputDebugStringA("failed to allocate memory\n");
		    exit(1);
                }
            }
        }
        else
        {
            OutputDebugStringA("failed to get .aif file size\n");
	    exit(1);
        }
    }
    else
    {
        OutputDebugStringA("failed to get .aif file handle\n");
	exit(1);
    }

    return(FileAddress);
}

inline int
SteenCopy(uint8 *MemToCopy, uint8 *MemDestination, int BytesToCopy)
{
    int BytesCopied = 0;
    for(BytesCopied; BytesCopied < BytesToCopy; BytesCopied++)
    {
        *MemDestination++ = *MemToCopy++;
    }
    return(BytesCopied);
}

inline void
CopyIDFromAif(char *StartOfIDToRead, char *ID_Destination)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
        char *Letter = (char *)(StartOfIDToRead + i);
        ID_Destination[i] = *Letter;
    }

    ID_Destination[ID_WIDTH] = '\0';
}

inline void
CopyIDToWav(char *StartOfIDToRead, char *ID_Destination)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
        char *Letter = (char *)(StartOfIDToRead + i);
        ID_Destination[i] = *Letter;
    }
}

inline bool32 
AreIDsTheSame(char *IDToCheck, char *IDToCheckAgainst)
{
    for(int LetterIndex = 0; LetterIndex < ID_WIDTH; LetterIndex++)
    {
        char *LetterToCheck = (IDToCheck + LetterIndex);
        char *LetterToCheckAgainst = (IDToCheckAgainst + LetterIndex);
        if(*LetterToCheck != *LetterToCheckAgainst)
        {
            return(false);
        }
    }
    return(true);
}

inline void
ValidateID(char *IDToCheck, char *IDToCheckAgainst, char *CallingFunction)
{
    if(!AreIDsTheSame(IDToCheck, IDToCheckAgainst))
    {
        char DebugPrintStringBuffer[MAX_STRING_LEN];
        sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
                "\nERROR:\n\t"
                "In function"
                "\n\t\t%s"
                "\n\texpected to read ID" 
                "\n\t\t%s"
                "\n\tbut instead read\n\t\t%s\n", 
                CallingFunction, IDToCheckAgainst, IDToCheck);
        OutputDebugStringA((char *)DebugPrintStringBuffer);
        exit(1);
    }
}

inline bool32 
AreIntsTheSame(int IntToCheck, int IntToCheckAgainst)
{
    // this is stupid
    return(IntToCheck == IntToCheckAgainst);
}

inline void
ValidateInteger(int IntToCheck, 
	    	int IntToCheckAgainst, 
		char *CallingFunction, 
		char *Variable)
{
    if(!AreIntsTheSame(IntToCheck, IntToCheckAgainst))
    {
        char DebugPrintStringBuffer[MAX_STRING_LEN];
        sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
                "\nERROR:\n\t"
                "In function"
                "\n\t\t%s"
                "for variable"
                "\n\t\t%s"
                "\n\texpected to read integer" 
                "\n\t\t%d"
                "\n\tbut instead read\n\t\t%d\n", 
                CallingFunction, Variable, IntToCheck, IntToCheckAgainst);
        OutputDebugStringA((char *)DebugPrintStringBuffer);
        exit(1);
    }
}

inline void
ValidateIntegerRange(int IntToCheck, int LowerBound, int UpperBound, char *Variable, char *CallingFunction)
{
    if( !( (LowerBound <= IntToCheck) && (IntToCheck <= UpperBound) ) )
    {
        char DebugPrintStringBuffer[MAX_STRING_LEN];
        sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
                "\nERROR:\n\t"
                "In function"
                "\n\t\t%s"
                "\n\tfor integer variable" 
                "\n\t\t%s"
                "\n\tvalue of" 
                "\n\t\t%d"
                "\n\twas not within expected bounds of"
                "\n\t\t%d"
                "\n\tand" 
                "\n\t\t%d", 
                CallingFunction, Variable, IntToCheck, LowerBound, UpperBound);
        OutputDebugStringA((char *)DebugPrintStringBuffer);
        exit(1);
    }
}

// Checks that a pointer is actually pointing at data.
//    Right now we only use this once, and it does what 
//    we need, but if we use it more often, may need to tune 
//    it up because sometimes data that reads as 0 is 
//    actually valid data
inline void
ValidatePointer(uint8 *PointerToCheck, char *CallingFunction)
{
    if(*PointerToCheck == 0)
    {
        char DebugPrintStringBuffer[MAX_STRING_LEN];
        sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
                "\nERROR:\n\t"
                "In function"
                "\n\t\t%s"
                "\n\tpointer" 
                "\n\t\t%s"
                "\n\tdereferences to"
                "\n\t\t0"
                "\n\twhen nonzero data was expected",
                CallingFunction, Stringize(PointerToCheck));
        OutputDebugStringA((char *)DebugPrintStringBuffer);
        exit(1);
    }
}

// A bunch of tests to determine whether the file submitted for conversion
//    meets the .aif spec
void
ValidateAif(int CountOfEachChunkType[], 
            conv_common_chunk *CommonChunk, 
            conv_sound_data_chunk *SoundDataChunk)
{
    
    // There must be one and only one Common chunk
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

    // If we make it here, there must be exactly one Common chunk.
    //    Next, if the Common Chunk says the file has sample frames, then there 
    //    must be one and only one Sound Data chunk
    if( (CommonChunk->NumSampleFrames > 0) && (CountOfEachChunkType[SOUND_DATA_CHUNK] == 0) )
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
    if(CountOfEachChunkType[SOUND_DATA_CHUNK] > 1)
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

    // Finally, the Sound Data and Common chunks need to agree on the number of bytes
    //	    of samples the .aif file contains. 
    //
    //	    First we compute the number of bytes of samples the Sound Data chunk 
    //	    should have, based on some metadata the Common Chunk reports...
    int CommonChunk_BytesPerSample = (CommonChunk->SampleSize / BITS_IN_BYTE);
    uint64 ExpectedSoundDataChunkSize = ( CommonChunk->NumSampleFrames * 
					    CommonChunk->NumChannels * 
					    CommonChunk_BytesPerSample );

    // The Sound Data chunk's ChunkSize field accounts for some boilerplate that 
    //	  we don't want to include here, so subtract it
    uint64 SoundDataChunk_ChunkSize_WithoutBoilerplate = (
                                                            SoundDataChunk->ChunkSize - 
                                                            sizeof(SoundDataChunk->Offset) - 
                                                            sizeof(SoundDataChunk->BlockSize)
						         );

    if(ExpectedSoundDataChunkSize != SoundDataChunk_ChunkSize_WithoutBoilerplate)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"\n\t\tThe metadata reported by this .aif file's Common chunk"
		"\n\t\tfor the number of sample bytes the file contains"
		"\n\t\tis inaccurate."
		"\n\t\tTherefore, your .aif file appears to be corrupted."
		"\n\nThis program will now exit.");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }

    if(CountOfEachChunkType[INSTRUMENT_CHUNK] > 1)
    { 
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"\n\t\tThis .aif file contains more than one Instrument chunk,"
		"\n\t\twhich is not permitted by the .aif specification."
		"\n\t\tTherefore, your .aif file appears to be corrupted."
		"\n\nThis program will now exit.");
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
}

inline int16
FlipEndianness(int16 IntToFlip)
{
    int16 IntToWrite;

    uint8 IntToFlipByteZero = IntToFlip & 255;
    uint8 IntToFlipByteOne = (IntToFlip >> 8) & 255;

    uint16 IntToWriteByteOne = (uint16) IntToFlipByteZero << 8;
    uint16 IntToWriteByteZero = (uint16) IntToFlipByteOne;

    IntToWrite = (int16) (IntToWriteByteOne | IntToWriteByteZero);

    return(IntToWrite);
}

inline uint16
FlipEndianness(uint16 IntToFlip)
{
    uint16 IntToWrite;

    uint8 IntToFlipByteZero = IntToFlip & 255;
    uint8 IntToFlipByteOne = (IntToFlip >> 8) & 255;

    uint16 IntToWriteByteOne = (uint16) IntToFlipByteZero << 8;
    uint16 IntToWriteByteZero = (uint16) IntToFlipByteOne;

    IntToWrite = (uint16) (IntToWriteByteOne | IntToWriteByteZero);

    return(IntToWrite);
}

inline int32 
FlipEndianness(int32 IntToFlip)
{
    int32 IntToWrite;

    uint8 IntToFlipByteZero = IntToFlip & 255;
    uint8 IntToFlipByteOne = (IntToFlip >> 8) & 255;
    uint8 IntToFlipByteTwo = (IntToFlip >> 16) & 255;
    uint8 IntToFlipByteThree = (IntToFlip >> 24) & 255;

    uint32 IntToWriteByteThree = (uint32) IntToFlipByteZero << 24;
    uint32 IntToWriteByteTwo = (uint32) IntToFlipByteOne << 16;
    uint32 IntToWriteByteOne = (uint32) IntToFlipByteTwo << 8;
    uint32 IntToWriteByteZero = (uint32) IntToFlipByteThree;

    IntToWrite = (int32) (IntToWriteByteThree | 
            IntToWriteByteTwo |
            IntToWriteByteOne |
            IntToWriteByteZero);

    return(IntToWrite);
}

inline uint32 
FlipEndianness(uint32 IntToFlip)
{
    uint32 IntToWrite;

    uint8 IntToFlipByteZero = IntToFlip & 255;
    uint8 IntToFlipByteOne = (IntToFlip >> 8) & 255;
    uint8 IntToFlipByteTwo = (IntToFlip >> 16) & 255;
    uint8 IntToFlipByteThree = (IntToFlip >> 24) & 255;

    uint32 IntToWriteByteThree = (uint32) IntToFlipByteZero << 24;
    uint32 IntToWriteByteTwo = (uint32) IntToFlipByteOne << 16;
    uint32 IntToWriteByteOne = (uint32) IntToFlipByteTwo << 8;
    uint32 IntToWriteByteZero = (uint32) IntToFlipByteThree;

    IntToWrite = (uint32) (IntToWriteByteThree | 
            IntToWriteByteTwo |
            IntToWriteByteOne |
            IntToWriteByteZero);

    return(IntToWrite);
}

// This function parses the 10-byte, "extended-width" floating point
//    number that .aif files use to encode sample rate. It returns
//    the integer value of the floating point number. This is because
//    we want to output a .wav file, and .wav files (sensibly) encode 
//    sample rates with integers, not floats. I have never seen a fractional
//    sample rate for audio.
//    
//    Since Microsoft's C++ compiler does not even recognize 10-byte
//    floats, we can only pass a pointer to the first byte,
//    then parse its components byte-by-byte, ultimately
//    converting the value to an integer with arithmetic.
inline uint32
GetSampleRate(uint8 *FirstByteOfExtendedPrecisionFloat)
{

    uint8 *Aif_Index = FirstByteOfExtendedPrecisionFloat;

    // The first bit of the 10-byte float is the sign bit. We
    //	  assume the sign bit is not set (i.e., we assume a 
    //	  positive sample rate) since we can't imagine what point
    //	  there would be to a sample rate that is less than zero.
    //
    //	  The 15 bits that follow the sign bit represent the 
    //	  exponent field.
    //
    //	  So we take the sign bit and the exponent bits together as
    //	  a 16-bit unsigned int, assuming that the sign bit is not set.
    uint16 ExponentBytesToFlip = *(uint16 *)Aif_Index; 
    int32 Exponent = 0;

    // Flip the endianness of the two exponent bytes
    Exponent = (
            (uint16)((ExponentBytesToFlip & 255) << 8) |
            (uint16)((ExponentBytesToFlip >> 8) & 255)
            );

    // This is the bias value for the exponent for 10 byte floats
    int32 ExtendedPrecisionFloatingPointExponentBias = -16383;
    // Add it to the encoded value to arrive at the unbiased exponent
    Exponent += ExtendedPrecisionFloatingPointExponentBias;

    // Move the pointer beyond the exponent field so that it points at
    //	  the first byte of the fraction field
    Aif_Index += 2;
    uint64 FractionBytesToFlip = *(uint64 *)Aif_Index;
    uint64 Fraction = 0;

    //todo: do we need to cast every 8-bit portion to a uint64

    // Flip the endianness of the fraction bytes
    Fraction = (
            (uint64)((FractionBytesToFlip & 255) << 56) |
            (uint64)(((FractionBytesToFlip >> 8) & 255) << 48) |
            (uint64)(((FractionBytesToFlip >> 16) & 255) << 40) |
            (uint64)(((FractionBytesToFlip >> 24) & 255) << 32) |
            (uint64)(((FractionBytesToFlip >> 32) & 255) << 24) |
            (uint64)(((FractionBytesToFlip >> 40) & 255) << 16) |
            (uint64)(((FractionBytesToFlip >> 48) & 255) << 8) |
            (uint64)((FractionBytesToFlip >> 56) & 255)
            );

    // Shift the fraction bits to the right in accordance with the
    //	  exponent value
    int32 TimesToShiftRight = 63 - Exponent;
    Fraction >>= TimesToShiftRight;

    return((uint32)Fraction);
}

// Todo: Currently we have to cast the return value; we should do some macro 
//    gymnastics to automatically cast it for us like we did the arena
//    allocator.
//
// Todo: Error handling if we call this function and can't find
//    the target chunk (currently none)
void *
GetConvertedChunkPointer(chunk Chunk)
{
    void *DidntFind = 0;
    for(int i = 0; i < Global_CountOfUnimportantChunks; i++)
    {
	chunk_finder ThisChunk = Global_UnChunkDirectory[i];

	if(ThisChunk.HashedID == Chunk)
	{
	    return((void *)ThisChunk.Conv_Chunk);
	}
    }
    return(DidntFind);
}


uint32
GetSampleFramePosition(int16 IDForDesiredMarker, conv_marker_chunk *Conv_MarkerChunk)
{
    int16 IDAsArrayIdx = IDForDesiredMarker - 1;
    conv_marker MarkerWeWant = Conv_MarkerChunk->Conv_MarkersStart[IDAsArrayIdx];

    return(MarkerWeWant.Position);
}


void
ParseFormChunk(uint8 *Aif_FormChunk_Start, 
		conv_form_chunk *Conv_FormChunk, 
		int64 Aif_FileSize)
{
    int BytesRead = 0;
    aif_form_chunk *Aif_FormChunk = (aif_form_chunk *)Aif_FormChunk_Start;

    // Read the Form chunk ID
    CopyIDFromAif(Aif_FormChunk->ID, Conv_FormChunk->ID);
    BytesRead += sizeof(ID_WIDTH);

    // Read the size of the Form chunk and flip endianness
    Conv_FormChunk->ChunkSize = FlipEndianness(Aif_FormChunk->ChunkSize);
    BytesRead += sizeof(Conv_FormChunk->ChunkSize);

    // The ChunkSize of the Form chunk should be equal to the total size of the
    //	  file, less the number of bytes we've read so far
    ValidateInteger(Conv_FormChunk->ChunkSize, 
		    ((int)Aif_FileSize - BytesRead), __func__,
		    Stringize(Conv_FormChunk->ChunkSize));

    // Read the FormType field
    CopyIDFromAif(Aif_FormChunk->FormType, Conv_FormChunk->FormType);

    // Pointer to the where the file's data actually begins
    Conv_FormChunk->Aif_SubChunksStart = &Aif_FormChunk->SubChunksStart;

    // Per .aif spec ID must be FORM or file is invalid
    ValidateID(Conv_FormChunk->ID, "FORM", __func__);

    // Per .aif spec FormType must be AIFF or file is invalid
    ValidateID(Conv_FormChunk->FormType, "AIFF", __func__);
}

void
ParseCommonChunk(uint8 *Aif_CommonChunk_Start, conv_common_chunk *Conv_CommonChunk)
{
    aif_common_chunk *Aif_CommonChunk = (aif_common_chunk *)Aif_CommonChunk_Start;
    CopyIDFromAif(Aif_CommonChunk->ID, Conv_CommonChunk->ID);

    Conv_CommonChunk->ChunkSize = FlipEndianness(Aif_CommonChunk->ChunkSize);
    Conv_CommonChunk->NumChannels = FlipEndianness(Aif_CommonChunk->NumChannels);
    Conv_CommonChunk->NumSampleFrames = FlipEndianness(Aif_CommonChunk->NumSampleFrames);
    Conv_CommonChunk->SampleSize = FlipEndianness(Aif_CommonChunk->SampleSize);
    Conv_CommonChunk->SampleRate = GetSampleRate((uint8 *)&Aif_CommonChunk->SampleRate);

    // We only support sample rates that are multiples of 100
    ValidateInteger((Conv_CommonChunk->SampleRate % 100), 
		    0, __func__, 
		    Stringize(Conv_CommonChunk->SampleRate % 100));

    // SampleSize can be any number between 1 and 32 per .aif spec
    ValidateIntegerRange(Conv_CommonChunk->SampleSize, 
			1, 32, __func__, 
			Stringize(Conv_CommonChunk->SampleSize));
}

// .aif files may need to specify the position of certain samples in the file
//    for use by software. They do so using Marker chunks. For example, samplers
//    look for Marker chunks to determine whether a portion of an .aif file is
//    meant to be looped when the user holds down a key.
//    
//    Note that Markers are not required. 
//
//    For readability, we first parse the header portion of the Marker chunk,
//    then parse the actual Marker data in a separate function.
void
ParseMarkerChunk(uint8 *Aif_MarkerChunk_Start, conv_marker_chunk *Conv_MarkerChunk)
{
    aif_marker_chunk *Aif_MarkerChunk = (aif_marker_chunk *)Aif_MarkerChunk_Start;
    CopyIDFromAif(Aif_MarkerChunk->ID, Conv_MarkerChunk->ID);

    Conv_MarkerChunk->ChunkSize = FlipEndianness(Aif_MarkerChunk->ChunkSize);
    Conv_MarkerChunk->TotalMarkers = FlipEndianness(Aif_MarkerChunk->TotalMarkers);
    Conv_MarkerChunk->Aif_MarkersStart = &Aif_MarkerChunk->MarkersStart;

    // May as well initialize this to 0 here
    Conv_MarkerChunk->Conv_MarkersStart = 0;
}

void
ParseMarkers(conv_marker_chunk *Conv_MarkerChunk, 
		arena *Arena, uint32 NumSampleFrames)
{
    uint8 *Aif_MarkerPointer = Conv_MarkerChunk->Aif_MarkersStart;
    conv_marker *Conv_Markers = Conv_MarkerChunk->Conv_MarkersStart;

    int16 *MarkerIDsSeen = PushArray(Arena, int16, Conv_MarkerChunk->TotalMarkers);

    for(int i = 0; i < Conv_MarkerChunk->TotalMarkers; i++)
    {
	aif_marker *ThisAif_Marker = (aif_marker *)Aif_MarkerPointer;
	conv_marker ThisConv_Marker = Conv_Markers[i];

	ThisConv_Marker.ID = FlipEndianness(ThisAif_Marker->ID);
	MarkerIDsSeen[i] = ThisConv_Marker.ID;

	// Per .aif spec, ID must be a "positive, nonzero" (??) integer,
	//    even though the spec defines the ID field's type to be
	//    a SIGNED int16. We do our best to validate, anyway...
	int Int16UpperBound = 0x7FFF;
	ValidateIntegerRange((int)ThisConv_Marker.ID, 1, Int16UpperBound,
				Stringize(ThisConv_Marker.ID), __func__);

	ThisConv_Marker.Position = FlipEndianness(ThisAif_Marker->Position);
	
	// ThisConv_Marker.Position is an offset into the array of sample 
	//     frames in the .aif file that comprise the actual sound to be played, 
	//     so logically, it should never be higher than the number of total
	//     sample frames the file declares in its Common chunk
	ValidateIntegerRange((int)ThisConv_Marker.Position, 0, 
				(NumSampleFrames),
				Stringize(ThisConv_Marker.ID), __func__);

	ThisConv_Marker.MarkerNameLen = ThisAif_Marker->MarkerNameLen;

	ThisConv_Marker.MarkerNameStart = PushArray(Arena, uint8, 
						(ThisConv_Marker.MarkerNameLen + 1) );
	SteenCopy(&ThisAif_Marker->MarkerNameStart, 
		    ThisConv_Marker.MarkerNameStart, 
		    (int)ThisConv_Marker.MarkerNameLen);

	// Add null char
	ThisConv_Marker.MarkerNameStart[(int)ThisConv_Marker.MarkerNameLen] = '\0';

	// Write the Marker back out to memory
	Conv_Markers[i] = ThisConv_Marker;

	// Skip to the next Marker in the .aif file
	Aif_MarkerPointer = (uint8 *)(&ThisAif_Marker->MarkerNameStart + 
				ThisAif_Marker->MarkerNameLen);

    }
    
    // We verify that each Marker's ID was unique to confirm that 
    //	  the file meets spec. This loop is O(n^2) but it should never 
    //	  run more than a dozen or so times ... nerd
    for(int i = 0; i < Conv_MarkerChunk->TotalMarkers; i++)
    {
	int ThisMarkerID = (int)MarkerIDsSeen[i];
	for(int j = (i+1); j < Conv_MarkerChunk->TotalMarkers; j++)
	{
	    int IDToCompare = (int)MarkerIDsSeen[j];
	    if(IDToCompare == ThisMarkerID)
	    {
		char DebugPrintStringBuffer[MAX_STRING_LEN];
		sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"\nERROR:\n\t"
		"\n\t\tThis .aif file's Markers do not have unique IDs,"
		"\n\t\twhich violates the .aif specification."
		"\n\nThis program will now exit.");
		OutputDebugStringA((char *)DebugPrintStringBuffer);
		exit(1);
	    }
	}
    }
}

int
ParseInstrumentChunk(uint8 *Aif_InstrumentChunk_Start, conv_instrument_chunk *Conv_InstrumentChunk)
{
    int BytesRead = 0;
    aif_instrument_chunk *Aif_InstrumentChunk = (aif_instrument_chunk *)Aif_InstrumentChunk_Start;

    CopyIDFromAif(Aif_InstrumentChunk->ID, Conv_InstrumentChunk->ID);
    Conv_InstrumentChunk->ChunkSize = FlipEndianness(Aif_InstrumentChunk->ChunkSize);
    Conv_InstrumentChunk->BaseNote = Aif_InstrumentChunk->BaseNote;
    Conv_InstrumentChunk->Detune = Aif_InstrumentChunk->Detune;
    Conv_InstrumentChunk->LowNote = Aif_InstrumentChunk->LowNote;
    Conv_InstrumentChunk->HighNote = Aif_InstrumentChunk->HighNote;
    Conv_InstrumentChunk->LowVelocity = Aif_InstrumentChunk->LowVelocity;
    Conv_InstrumentChunk->HighVelocity = Aif_InstrumentChunk->HighVelocity;
    Conv_InstrumentChunk->Gain = FlipEndianness(Aif_InstrumentChunk->Gain);

    Conv_InstrumentChunk->SustainLoop.PlayMode = 
		FlipEndianness(Aif_InstrumentChunk->SustainLoop.PlayMode);
    Conv_InstrumentChunk->SustainLoop.BeginLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->SustainLoop.BeginLoopMarker);
    Conv_InstrumentChunk->SustainLoop.EndLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->SustainLoop.EndLoopMarker);
    Conv_InstrumentChunk->ReleaseLoop.PlayMode = 
		FlipEndianness(Aif_InstrumentChunk->ReleaseLoop.PlayMode);
    Conv_InstrumentChunk->ReleaseLoop.BeginLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->ReleaseLoop.BeginLoopMarker);
    Conv_InstrumentChunk->ReleaseLoop.EndLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->ReleaseLoop.EndLoopMarker);

    // Chunk size is always 20 per .aif spec
    ValidateInteger(Conv_InstrumentChunk->ChunkSize, 
		    20, __func__,
		    Stringize(Conv_InstrumentChunk->ChunkSize));

    // Detune value is between -50 and 50 per .aif spec
    ValidateIntegerRange(Conv_InstrumentChunk->Detune, 
			-50, 50, 
			Stringize(Conv_InstrumentChunk->Detune), __func__);

    // LowNote, HighNote, LowVelocity, HighVelocity are MIDI note values and 
    //	  must be between 1 and 127 per .aif spec
    ValidateIntegerRange(Conv_InstrumentChunk->LowNote, 
			1, 127, 
			Stringize(Conv_InstrumentChunk->LowNote), 
			__func__);
    ValidateIntegerRange(Conv_InstrumentChunk->HighNote, 
			1, 127, 
			Stringize(Conv_InstrumentChunk->HighNote),
			__func__);
    ValidateIntegerRange(Conv_InstrumentChunk->LowVelocity, 
			1, 127, 
			Stringize(Conv_InstrumentChunk->LowVelocity),
			__func__);
    ValidateIntegerRange(Conv_InstrumentChunk->HighVelocity, 
			1, 127, 
			Stringize(Conv_InstrumentChunk->HighVelocity),
			__func__);
    return(BytesRead);
}

// Some .aif files include a chunk with the ID "FLLR." This
//    "filler" chunk is not documented in the .aif spec. 
//
//    It appears that the purpose of the chunk is merely 
//    to specify the size of an array of empty bytes,
//    immediately following the chunk, that software reading 
//    the file should skip over.
//
//    As far as we can tell, a Filler chunk defined as a C struct
//    would look like this:
//
//    struct filler_chunk
//    {
//	  char ID[ID_WIDTH + 1];
//	  uint32 TotalFillerBytes;
//    };
//
//    Hence this function, which works in our testing:
int
ParseFillerChunk(uint8 *Aif_FillerChunk_Start, filler_chunk *FillerChunk)
{
    int32 BytesRead = 0;

    // Define an pointer for indexing into the chunk
    uint8 *FillerChunk_HeaderIndex = Aif_FillerChunk_Start;

    char *FillerChunk_HeaderIDStart = (char *)FillerChunk_HeaderIndex;
    CopyIDFromAif(FillerChunk_HeaderIDStart, FillerChunk->ID);
    BytesRead += ID_WIDTH;

    FillerChunk_HeaderIndex = AdvancePointer(Aif_FillerChunk_Start, BytesRead);
    uint32 *TotalFillerBytes = (uint32 *)FillerChunk_HeaderIndex;
    FillerChunk->TotalFillerBytes = FlipEndianness(*TotalFillerBytes);
    BytesRead += sizeof(FillerChunk->TotalFillerBytes);

    return(BytesRead);
}

void 
ParseSoundDataChunk(uint8 *Aif_SoundDataChunk_Start, conv_sound_data_chunk *Conv_SoundDataChunk)
{
    aif_sound_data_chunk *Aif_SoundDataChunk = (aif_sound_data_chunk *)Aif_SoundDataChunk_Start;
    CopyIDFromAif(Aif_SoundDataChunk->ID, Conv_SoundDataChunk->ID);

    Conv_SoundDataChunk->ChunkSize = FlipEndianness(Aif_SoundDataChunk->ChunkSize);
    Conv_SoundDataChunk->Offset = FlipEndianness(Aif_SoundDataChunk->Offset);
    Conv_SoundDataChunk->BlockSize = FlipEndianness(Aif_SoundDataChunk->BlockSize);
    Conv_SoundDataChunk->Aif_SamplesStart = &Aif_SoundDataChunk->SamplesStart;

    // Per .aif spec, 
    //	  "most applications won't use Offset and should set it to zero", 
    //	  so that's what we assume
    ValidateInteger(Conv_SoundDataChunk->Offset, 
		    0, __func__, 
		    Stringize(Conv_SoundDataChunk->Offset));

    // Same as Offset
    ValidateInteger(Conv_SoundDataChunk->BlockSize, 
		    0, __func__, 
		    Stringize(Conv_SoundDataChunk->BlockSize));
}

int
FlipSampleEndianness16Bits(conv_common_chunk *CommonChunk, uint8 *LittleEndianSamplesStart, 
			    uint8 *BigEndianSamplesStart)
{
    // Variables we use to keep track of where we are in the loop
    int BytesPerSample = (CommonChunk->SampleSize / BITS_IN_BYTE);
    int BytesInSampleFrame = BytesPerSample * CommonChunk->NumChannels;
    int HowManyBytesToFlip = BytesInSampleFrame * CommonChunk->NumSampleFrames;

    int BytesWritten = 0;

    for(int SampleFrame = 0;
            SampleFrame < HowManyBytesToFlip; 
            SampleFrame += BytesInSampleFrame)
    {
        for(int Channel = 0;
                Channel < CommonChunk->NumChannels;
                Channel++)
        {
            int SampleFrameOffset = Channel * BytesPerSample;

            uint8 FirstByteOfSamplePoint = 
		BigEndianSamplesStart[SampleFrame + SampleFrameOffset];

            uint8 SecondByteOfSamplePoint = 
		BigEndianSamplesStart[SampleFrame + SampleFrameOffset + 1];

	    // Flip the two bytes in the sample by XORing them with a bitmask
            uint8 Bitmask = (FirstByteOfSamplePoint) ^ (SecondByteOfSamplePoint);

	    // Flip the first byte
            LittleEndianSamplesStart[SampleFrame + SampleFrameOffset] = (FirstByteOfSamplePoint) ^ Bitmask;
            BytesWritten++;

	    // Flip the third byte
            LittleEndianSamplesStart[SampleFrame + SampleFrameOffset + 1] = (SecondByteOfSamplePoint) ^ Bitmask;
            BytesWritten++;
        }

    }

    return(BytesWritten);
}

int
FlipSampleEndianness24Bits(conv_common_chunk *CommonChunk, uint8 *LittleEndianSamplesStart, 
			    uint8 *BigEndianSamplesStart)
{
    // Variables we use to keep track of where we are in the loop
    int BytesPerSample = (CommonChunk->SampleSize / BITS_IN_BYTE);
    int BytesInSampleFrame = BytesPerSample * CommonChunk->NumChannels;
    int HowManyBytesToFlip = BytesInSampleFrame * CommonChunk->NumSampleFrames;

    int BytesWritten = 0;

    for(int SampleFrame = 0;
            SampleFrame < HowManyBytesToFlip; 
            SampleFrame += BytesInSampleFrame)
    {
        for(int Channel = 0;
                Channel < CommonChunk->NumChannels;
                Channel++)
        {
            int SampleFrameOffset = Channel * BytesPerSample;

            uint8 FirstByteOfSamplePoint = 
		BigEndianSamplesStart[SampleFrame + SampleFrameOffset];

            uint8 SecondByteOfSamplePoint = 
		BigEndianSamplesStart[SampleFrame + SampleFrameOffset + 1];

            uint8 ThirdByteOfSamplePoint = 
		BigEndianSamplesStart[SampleFrame + SampleFrameOffset + 2];	    

	    // Flip the first and third bytes of the 3-byte sample by
	    //	  XORing the first and third bytes
            uint8 Bitmask = FirstByteOfSamplePoint ^ ThirdByteOfSamplePoint;

	    // Flip the first byte
            LittleEndianSamplesStart[SampleFrame + SampleFrameOffset] = 
		FirstByteOfSamplePoint ^ Bitmask;
            BytesWritten++;

	    // The second byte is just written as it is
            LittleEndianSamplesStart[SampleFrame + SampleFrameOffset + 1] = 
		SecondByteOfSamplePoint;
            BytesWritten++;

	    // Flip the third byte
            LittleEndianSamplesStart[SampleFrame + SampleFrameOffset + 2] = 
		ThirdByteOfSamplePoint ^ Bitmask;
            BytesWritten++;
        }
    }

    return(BytesWritten);
}
