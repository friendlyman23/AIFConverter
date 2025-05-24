#include "converter.h"
#include "GPerfHash.c"

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
Win32_AllocateMemory(int MemSize, char *CallingFunction)
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
Win32_GetAifFilePointer(LPCWSTR Filename, LARGE_INTEGER *Aif_FileSize)
{
    LPVOID FileAddress = 0;
    HANDLE FileHandle = CreateFileW(Filename, GENERIC_READ, FILE_SHARE_READ, 0, 
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
                    FileReadSuccessfully = ReadFile(FileHandle, FileAddress, Aif_FileSize->QuadPart, &NumBytesRead, 0);
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
                }
            }
        }
        else
        {
            OutputDebugStringA("failed to get .aif file size\n");
        }
    }
    else
    {
        OutputDebugStringA("failed to get .aif file handle\n");
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
ReadID(char *StartOfIDToRead, char *ID_Destination)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
        char *Letter = (char *)(StartOfIDToRead + i);
        ID_Destination[i] = *Letter;
    }

    ID_Destination[ID_WIDTH] = '\0';
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
    return(IntToCheck == IntToCheckAgainst);
}

inline void
ValidateInteger(int IntToCheck, int IntToCheckAgainst, char *CallingFunction)
{
    if(!AreIntsTheSame(IntToCheck, IntToCheckAgainst))
    {
        char DebugPrintStringBuffer[MAX_STRING_LEN];
        sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
                "\nERROR:\n\t"
                "In function"
                "\n\t\t%s"
                "\n\texpected to read integer" 
                "\n\t\t%d"
                "\n\tbut instead read\n\t\t%d\n", 
                CallingFunction, IntToCheck, IntToCheckAgainst);
        OutputDebugStringA((char *)DebugPrintStringBuffer);
        exit(1);
    }
}

inline void
ValidateIntegerRange(int IntToCheck, int LowerBound, int UpperBound, char *CallingFunction)
{
    if( !( (LowerBound <= IntToCheck) && (IntToCheck <= UpperBound) ) )
    {
        char DebugPrintStringBuffer[MAX_STRING_LEN];
        sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
                "\nERROR:\n\t"
                "In function"
                "\n\t\t%s"
                "\n\tinteger" 
                "\n\t\t%d"
                "\n\twas not within expected bounds of"
                "\n\t\t%d"
                "\n\tand" 
                "\n\t\t%d", 
                CallingFunction, IntToCheck, LowerBound, UpperBound);
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
            common_chunk *CommonChunk, 
            sound_data_chunk *SoundDataChunk)
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
    uint16 Exponent = 0;

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

    return(Fraction);
}

// Functions named with the pattern "App_Parse_Aif_Chunk" read the 
//    header information of the .aif file we put on the heap 
//    and store its relevant data in structs on the stack.
//
//    We later use these data to output a .wav file.
//    
//    The Form Chunk parsing function is the first one 
//    to be called because, per the spec, all .aif files 
//    begin with Form Chunks. 
//
//    It is heavily annotated for clarity; 
//    other "App_Parse_Aif_Chunk" functions are not because they 
//    generally follow the patttern established here.

int
ParseFormChunk(uint8 *Aif_FormChunk_Start, 
		form_chunk *Converted_FormChunk, 
		int64 Aif_FileSize)
{
    int BytesRead = 0;
    aif_form_chunk *Aif_FormChunk = (aif_form_chunk *)Aif_FormChunk_Start;

    // Read the Form chunk ID
    ReadID(Aif_FormChunk->ID, Converted_FormChunk->ID);
    BytesRead += sizeof(ID_WIDTH);

    // Read the size of the Form chunk and flip endianness
    Converted_FormChunk->ChunkSize = FlipEndianness(Aif_FormChunk->ChunkSize);
    BytesRead += sizeof(Converted_FormChunk->ChunkSize);

    // The ChunkSize of the Form chunk should be equal to the total size of the
    //	  file, less the number of bytes we've read so far
    ValidateInteger(Converted_FormChunk->ChunkSize, 
		    (Aif_FileSize - BytesRead),
		    __func__);

    // Read the FormType field
    ReadID(Aif_FormChunk->FormType, Converted_FormChunk->FormType);
    BytesRead += sizeof(ID_WIDTH);

    // Per .aif spec ID must be FORM or file is invalid
    ValidateID(Converted_FormChunk->ID, "FORM", __func__);

    // Per .aif spec FormType must be AIFF or file is invalid
    ValidateID(Converted_FormChunk->FormType, "AIFF", __func__);

    return(BytesRead);
}

#if 0
int
ParseFormChunk(uint8 *Aif_FormChunk_Start, form_chunk *FormChunk)
{
    // We return the number of bytes this function read from the 
    //	  .aif file so main() knows how many bytes to skip to get to 
    //	  the next chunk in the .aif file
    int32 BytesRead = 0;

    // Declare a pointer to a single byte that we use to advance 
    //	  through the chunk and parse its data
    uint8 *Aif_Index = Aif_FormChunk_Start;

    // These App_Parse_Aif_...Chunk functions do not represent a large
    //	  share of the program's workload, so for readability,
    //	  we always declare a new pointer to each header item
    //	  as we encounter it, that explicitly names what it points to...
    char *Aif_FormChunk_IDStart = (char *)Aif_Index;

    // ...and pass this pointer to a corresponding function
    //	  that parses it and stores its relevant data on the stack.
    ReadID(Aif_FormChunk_IDStart, (char *)(&FormChunk->ID));

    // Since all ID fields for a given chunk are known 
    //	  at compile time, we check all IDs to confirm
    //	  that we read what we expected to read.
    // 
    //	  Per .aif spec, Form Chunk ID is always "FORM"
    ValidateID(FormChunk->ID, "FORM", __func__);

    // After confirming that we read valid data, we compute the number
    //	  of bytes read from this chunk so far...
    BytesRead += ID_WIDTH;


    // ...and pass this total, along with the address of the first byte
    //	  of the chunk, to compute the address of the next datum
    //	  to be read.
    Aif_Index = AdvancePointer(Aif_FormChunk_Start, BytesRead);

    // and we keep doing this until we've finished parsing the chunk!
    int32 *Aif_FormChunk_ChunkSizeAddress = (int32 *)Aif_Index;
    FormChunk->ChunkSize = FlipEndianness(*Aif_FormChunk_ChunkSizeAddress);
    BytesRead += sizeof(FormChunk->ChunkSize);

    Aif_Index = AdvancePointer(Aif_FormChunk_Start, BytesRead);
    char *Aif_FormChunk_FormType_Start = (char *)Aif_Index;
    ReadID(Aif_FormChunk_FormType_Start, (char *)(FormChunk->FormType));

    // Per .aif spec, Form Chunk Type is always "AIFF"
    ValidateID(FormChunk->FormType, "AIFF", __func__);
    BytesRead += ID_WIDTH;

    // The "data" of the Form Chunk is everything else
    //	  in the entire aif file.
    Aif_Index = AdvancePointer(Aif_FormChunk_Start, BytesRead);
    uint8 *Aif_FormChunk_DataStart = Aif_Index;
    FormChunk->SubChunksStart;

    return(BytesRead);
}
#endif

int
ParseCommonChunk(uint8 *Aif_CommonChunk_Start, common_chunk *CommonChunk)
{

    // Keep track of number of bytes we've read so that
    //	  we can compute necessary offsets
    int32 BytesRead = 0;

    // Declare a pointer that we use to keep track of our position
    //	  in the .aif file as we copy the data
    uint8 *Aif_Index = Aif_CommonChunk_Start;

    // Store the Common Chunk ID in its struct on the stack
    char *Aif_CommonChunk_IDStart = (char *)Aif_Index;
    ReadID(Aif_CommonChunk_IDStart, (char *)CommonChunk->ID);
    // Check that we read a valid ID
    ValidateID(CommonChunk->ID, "COMM", __func__);
    // Compute updated BytesRead value
    BytesRead += ID_WIDTH;

    // Store the size of the Common Chunk
    Aif_Index = AdvancePointer(Aif_CommonChunk_Start, BytesRead);
    int32 *Aif_CommonChunk_ChunkSizeAddress = (int32 *)Aif_Index;
    CommonChunk->ChunkSize = FlipEndianness(*Aif_CommonChunk_ChunkSizeAddress);
    // per the spec, ChunkSize is always 18
    ValidateInteger(CommonChunk->ChunkSize, 18, __func__);
    BytesRead += sizeof(CommonChunk->ChunkSize);

    // Store the number of audio channels
    Aif_Index = AdvancePointer(Aif_CommonChunk_Start, BytesRead);
    int16 *Aif_CommonChunk_NumChannels = (int16 *)Aif_Index;
    CommonChunk->NumChannels = FlipEndianness(*Aif_CommonChunk_NumChannels);
    BytesRead += sizeof(CommonChunk->NumChannels);

    // Store the number of sample frames
    Aif_Index = AdvancePointer(Aif_CommonChunk_Start, BytesRead);
    uint32 *Aif_CommonChunk_NumSampleFrames = (uint32 *)Aif_Index;
    CommonChunk->NumSampleFrames = FlipEndianness(*Aif_CommonChunk_NumSampleFrames);
    BytesRead += sizeof(CommonChunk->NumSampleFrames);

    // Store the sample size (the number of bytes dedicated to each sample)
    Aif_Index = AdvancePointer(Aif_CommonChunk_Start, BytesRead);
    int16 *Aif_CommonChunk_SampleSize = (int16 *)Aif_Index;
    CommonChunk->SampleSize = FlipEndianness(*Aif_CommonChunk_SampleSize);
    BytesRead += sizeof(CommonChunk->SampleSize);

    // .aif files use a 10-byte, extended-width floating point number
    //	  to represent sample rate. See comments on the GetSampleRate
    //	  function for details.
    Aif_Index = AdvancePointer(Aif_CommonChunk_Start, BytesRead);
    uint8 *Aif_CommonChunk_SampleRate = Aif_Index;
    CommonChunk->SampleRate = GetSampleRate(Aif_CommonChunk_SampleRate);
    BytesRead += EXTENDED_WIDTH;

    return(BytesRead);
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
int
ParseMarkerChunk(uint8 *Aif_MarkerChunk, marker_chunk *Converted_MarkerChunk, 
			uint8 **Aif_MarkersData_Start)
{

    // Keep track of number of bytes we've read so that
    //	  we can compute necessary offsets
    int32 BytesRead = 0;
    uint8 *Aif_Index = Aif_MarkerChunk;

    // Store the Marker Chunk ID in its struct in memory
    char *Aif_MarkerChunk_HeaderIDStart = (char *)Aif_Index;
    ReadID(Aif_MarkerChunk_HeaderIDStart, Converted_MarkerChunk->ID);
    // Check that we read a valid ID
    ValidateID(Converted_MarkerChunk->ID, "MARK", __func__);
    // Compute updated BytesRead value
    BytesRead += ID_WIDTH;

    // Store the size of the Marker Chunk
    Aif_Index = AdvancePointer(Aif_MarkerChunk, BytesRead);
    int32 *Aif_MarkerChunk_Size = (int32 *)Aif_Index;
    Converted_MarkerChunk->Size = FlipEndianness(*Aif_MarkerChunk_Size);
    BytesRead += sizeof(Converted_MarkerChunk->Size);

    // Store the number of Markers. We use this value later to know
    //	  how much space to put on the heap to store them
    Aif_Index = AdvancePointer(Aif_MarkerChunk, BytesRead);
    uint16 *Aif_MarkerChunk_HeaderTotalMarkers = (uint16 *)Aif_Index;
    Converted_MarkerChunk->TotalMarkers = FlipEndianness(*Aif_MarkerChunk_HeaderTotalMarkers);
    BytesRead += sizeof(Converted_MarkerChunk->TotalMarkers);
    Aif_Index = AdvancePointer(Aif_MarkerChunk, BytesRead);

    // Aif_Index now points at the first Marker, so we grab its address so 
    //	  the separate function that actually parses the markers knows where
    //	  to start
    *Aif_MarkersData_Start = Aif_Index;

    return(BytesRead);
}

// In this function we figure out how many bytes to allocate in memory for
//    converted versions of the Markers.
//    
//    Markers in .aif files use Pascal-style strings for their Name field.
//    i.e., Instead of terminating strings with the null char, the first byte of
//    the Marker's name is an int that represents the number of one-byte chars 
//    in the string. The function below refers to this int as "StringLen".
//
//    In all other respects, Marker chunk fields have uniform
//    sizes. In the function below, this uniform number of bytes per marker
//    is represented as MARKER_BOILERPLATE.
//
//    Therefore, the number of required bytes will be equal to:
//	  (MARKER_BOILERPLATE * Number of Markers)
//			    +
//	  Sum of the StringLen fields for Markers
int
GetNumBytesToStoreConverted_Markers(uint8 *Aif_MarkersData_Start, int32 TotalMarkers)
{
    int BytesNeededToStoreMarkers = 0;

    uint8 *Aif_Index = Aif_MarkersData_Start;
    for(int i = 0; i < TotalMarkers; i++)
    {
	aif_marker *ThisMarker = (aif_marker *)Aif_Index;
	BytesNeededToStoreMarkers += MARKER_BOILERPLATE;
	BytesNeededToStoreMarkers += ThisMarker->MarkerNameLen;

	// Skip over the MarkerName string to the start of the next Marker
	Aif_Index = (uint8 *)ThisMarker->MarkerName;
	Aif_Index += ThisMarker->MarkerNameLen;
    }
    
    return(BytesNeededToStoreMarkers);
}

int32
ParseMarkers(uint8 *Aif_MarkersData_Start, 
        marker_chunk *Converted_MarkerChunk, int BytesAllocatedForMarkers)
{
    int32 BytesRead = 0;
    int32 BytesWritten = 0;
    uint8 *Aif_Index = (uint8 *)Aif_MarkersData_Start;
    uint8 *Converted_Index = Converted_MarkerChunk->Markers;

    for(int MarkerNumber = 0; 
	    MarkerNumber < Converted_MarkerChunk->TotalMarkers;
	    MarkerNumber++)
    {
	Aif_Index = AdvancePointer(Aif_MarkersData_Start, BytesRead);
	Converted_Index = AdvancePointer(Converted_MarkerChunk->Markers, BytesRead);
	int16 *Aif_MarkerID = (int16 *)Aif_Index;
	int16 *Converted_MarkerID = (int16 *)Converted_Index;
	*Converted_MarkerID = FlipEndianness(*Aif_MarkerID);
	BytesWritten += sizeof(*Converted_MarkerID);
	Assert(BytesWritten <= BytesAllocatedForMarkers);

	// Per the .aif spec, the ID must be a positive integer
	ValidateIntegerRange(*Converted_MarkerID, 0, INT_MAX, __func__);
	BytesRead += sizeof(*Converted_MarkerID);

	// Read the Marker position
	Aif_Index = AdvancePointer(Aif_MarkersData_Start, BytesRead);
	Converted_Index = AdvancePointer(Converted_MarkerChunk->Markers, BytesRead);
	uint32 *Aif_MarkerPosition = (uint32 *)Aif_Index;
	uint32 *Converted_MarkerPosition = (uint32 *)Converted_Index;
	*Converted_MarkerPosition = FlipEndianness(*Aif_MarkerPosition);
	BytesWritten += sizeof(*Converted_MarkerPosition);
	Assert(BytesWritten <= BytesAllocatedForMarkers);
	BytesRead += sizeof(*Converted_MarkerPosition);

	// Read the length of the string containing the Marker's name 
	Aif_Index = AdvancePointer(Aif_MarkersData_Start, BytesRead);
	Converted_Index = AdvancePointer(Converted_MarkerChunk->Markers, BytesRead);
	// This is just an 8-bit uint, so we don't have to flip endianness
	uint8 *Aif_MarkerNameStringLen = (uint8 *)Aif_Index;
	uint8 *Converted_MarkerNameStringLen = (uint8 *)Converted_Index;
	*Converted_MarkerNameStringLen = *Aif_MarkerNameStringLen;
	BytesWritten += sizeof(*Converted_MarkerNameStringLen);
	Assert(BytesWritten <= BytesAllocatedForMarkers);
	BytesRead += sizeof(*Converted_MarkerNameStringLen);

	// Read the string containing the Marker's name
	Aif_Index = AdvancePointer(Aif_MarkersData_Start, BytesRead);
	Converted_Index = AdvancePointer(Converted_MarkerChunk->Markers, BytesRead);
	// Strings don't need their endianness flipped, so we just do a straight copy
	BytesWritten += SteenCopy(Aif_Index, 
				    Converted_Index, 
				    (int)*Converted_MarkerNameStringLen);
	Assert(BytesWritten <= BytesAllocatedForMarkers);
	// Increment BytesRead by the number of bytes in the string
	BytesRead += *Converted_MarkerNameStringLen;
    }

    return(BytesRead);
}

int
ParseInstrumentChunk(uint8 *Aif_InstrumentChunk_Start, instrument_chunk *Converted_InstrumentChunk)
{
    int BytesRead = 0;
    aif_instrument_chunk *Aif_InstrumentChunk = (aif_instrument_chunk *)Aif_InstrumentChunk_Start;

    ReadID(Aif_InstrumentChunk->ID, Converted_InstrumentChunk->ID);
    Converted_InstrumentChunk->ChunkSize = FlipEndianness(Aif_InstrumentChunk->ChunkSize);
    Converted_InstrumentChunk->BaseNote = Aif_InstrumentChunk->BaseNote;
    Converted_InstrumentChunk->Detune = Aif_InstrumentChunk->Detune;
    Converted_InstrumentChunk->LowNote = Aif_InstrumentChunk->LowNote;
    Converted_InstrumentChunk->HighNote = Aif_InstrumentChunk->HighNote;
    Converted_InstrumentChunk->LowVelocity = Aif_InstrumentChunk->LowVelocity;
    Converted_InstrumentChunk->HighVelocity = Aif_InstrumentChunk->HighVelocity;
    Converted_InstrumentChunk->Gain = FlipEndianness(Aif_InstrumentChunk->Gain);

    Converted_InstrumentChunk->SustainLoop.PlayMode = 
		FlipEndianness(Aif_InstrumentChunk->SustainLoop.PlayMode);
    Converted_InstrumentChunk->SustainLoop.BeginLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->SustainLoop.BeginLoopMarker);
    Converted_InstrumentChunk->SustainLoop.EndLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->SustainLoop.EndLoopMarker);
    Converted_InstrumentChunk->ReleaseLoop.PlayMode = 
		FlipEndianness(Aif_InstrumentChunk->ReleaseLoop.PlayMode);
    Converted_InstrumentChunk->ReleaseLoop.BeginLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->ReleaseLoop.BeginLoopMarker);
    Converted_InstrumentChunk->ReleaseLoop.EndLoopMarker = 
		FlipEndianness(Aif_InstrumentChunk->ReleaseLoop.EndLoopMarker);

    // Chunk size is always 20 per .aif spec
    ValidateInteger(Converted_InstrumentChunk->ChunkSize, 20, __func__);

    // Detune value is between -50 and 50 per .aif spec
    ValidateIntegerRange(Converted_InstrumentChunk->Detune, -50, 50, __func__);

    // LowNote, HighNote, LowVelocity, HighVelocity are MIDI note values and 
    //	  must be between 1 and 127 per .aif spec
    ValidateIntegerRange(Converted_InstrumentChunk->LowNote, 1, 127, __func__);
    ValidateIntegerRange(Converted_InstrumentChunk->HighNote, 1, 127, __func__);
    ValidateIntegerRange(Converted_InstrumentChunk->LowVelocity, 1, 127, __func__);
    ValidateIntegerRange(Converted_InstrumentChunk->HighVelocity, 1, 127, __func__);


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
    ReadID(FillerChunk_HeaderIDStart, FillerChunk->ID);
    BytesRead += ID_WIDTH;

    FillerChunk_HeaderIndex = AdvancePointer(Aif_FillerChunk_Start, BytesRead);
    uint32 *TotalFillerBytes = (uint32 *)FillerChunk_HeaderIndex;
    FillerChunk->TotalFillerBytes = FlipEndianness(*TotalFillerBytes);
    BytesRead += sizeof(FillerChunk->TotalFillerBytes);

    return(BytesRead);
}

int 
ParseSoundDataChunk(uint8 *Aif_SoundDataChunk_Start, sound_data_chunk *SoundDataChunk)
{

    int BytesRead = 0;

    uint8 *Aif_Index = Aif_SoundDataChunk_Start;

    char *Aif_SoundDataChunk_Header_IDStart = (char *)Aif_Index;
    ReadID(Aif_SoundDataChunk_Header_IDStart, SoundDataChunk->ID);
    BytesRead += ID_WIDTH;

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Start, BytesRead);
    int32 *Aif_SoundDataChunk_Header_Size = (int32 *)Aif_Index;
    SoundDataChunk->ChunkSize = FlipEndianness(*Aif_SoundDataChunk_Header_Size);
    BytesRead += sizeof(SoundDataChunk->ChunkSize);

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Start, BytesRead);
    uint32 *Aif_SoundDataChunk_Header_Offset = (uint32 *)Aif_Index;
    SoundDataChunk->Offset = FlipEndianness(*Aif_SoundDataChunk_Header_Offset);
    BytesRead += sizeof(SoundDataChunk->Offset);

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Start, BytesRead);
    uint32 *SoundDataChunk_Header_BlockSize = (uint32 *)Aif_Index;
    SoundDataChunk->BlockSize = FlipEndianness(*SoundDataChunk_Header_BlockSize);
    BytesRead += sizeof(SoundDataChunk->BlockSize);

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Start, BytesRead);
    SoundDataChunk->SamplesStart = Aif_Index;

    return(BytesRead);
}

int
FlipSampleEndianness16Bits(common_chunk *CommonChunk, uint8 *LittleEndianSamplesStart, 
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
FlipSampleEndianness24Bits(common_chunk *CommonChunk, uint8 *LittleEndianSamplesStart, 
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
