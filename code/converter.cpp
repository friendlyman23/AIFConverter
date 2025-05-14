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
Win32_GetFilePointer(LPCWSTR Filename)
{
    LPVOID FileAddress = 0;
    HANDLE FileHandle = CreateFileW(Filename, GENERIC_READ, FILE_SHARE_READ, 0, 
            OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        GetFileSizeEx(FileHandle, &FileSize);
        if(FileSize.QuadPart)
        {
            Assert(FileSize.QuadPart <= 0xFFFFFFFF);

            HANDLE HeapHandle = GetProcessHeap();
            if(HeapHandle)
            {
                FileAddress = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, FileSize.QuadPart);
                if(FileAddress)
                {
                    DWORD NumBytesRead;
                    BOOL FileReadSuccessfully = false;
                    FileReadSuccessfully = ReadFile(FileHandle, FileAddress, FileSize.QuadPart, &NumBytesRead, 0);
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

void
SteenCopy(uint8 *MemToCopy, uint8 *MemDestination, int BytesToCopy)
{
    for(int BytesCopied = 0; BytesCopied < BytesToCopy; BytesCopied++)
    {
        *MemDestination++ = *MemToCopy++;
    }
}

void
ReadID(char *IDStart, char *ID)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
        char *Letter = (char *)(IDStart + i);
        ID[i] = *Letter;
    }

    ID[ID_WIDTH] = '\0';
}

bool32 
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

void
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

bool32 
AreIntsTheSame(int IntToCheck, int IntToCheckAgainst)
{
    return(IntToCheck == IntToCheckAgainst);
}

void
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

void
ValidateIntegerRange(int IntToCheck, int LowerBound, int UpperBound, char *CallingFunction)
{
    if( !( (LowerBound < IntToCheck) && (IntToCheck < UpperBound) ) )
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
void
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
App_Parse_Aif_Chunk(uint8 *Aif_FormChunk_Start, form_chunk *FormChunk)
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

int
App_Parse_Aif_Chunk(uint8 *Aif_CommonChunk_Start, common_chunk *CommonChunk)
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
App_Parse_Aif_Chunk(uint8 *Aif_MarkerChunk_HeaderStart, marker_chunk_header *MarkerChunk_Header)
{

    // Keep track of number of bytes we've read so that
    //	  we can compute necessary offsets
    int32 BytesRead = 0;
    uint8 *Aif_Index = Aif_MarkerChunk_HeaderStart;

    // Store the Marker Chunk ID in its struct on the stack
    char *Aif_MarkerChunk_HeaderIDStart = (char *)Aif_Index;
    ReadID(Aif_MarkerChunk_HeaderIDStart, MarkerChunk_Header->ID);
    // Check that we read a valid ID
    ValidateID(MarkerChunk_Header->ID, "MARK", __func__);
    // Compute updated BytesRead value
    BytesRead += ID_WIDTH;

    // Store the size of the Marker Chunk
    Aif_Index = AdvancePointer(Aif_MarkerChunk_HeaderStart, BytesRead);
    int32 *Aif_MarkerChunk_HeaderSize = (int32 *)Aif_Index;
    MarkerChunk_Header->Size = FlipEndianness(*Aif_MarkerChunk_HeaderSize);
    BytesRead += sizeof(MarkerChunk_Header->Size);

    // Store the number of Markers. We use this value later to know
    //	  how much space to put on the heap to store them
    Aif_Index = AdvancePointer(Aif_MarkerChunk_HeaderStart, BytesRead);
    uint16 *Aif_MarkerChunk_HeaderTotalMarkers = (uint16 *)Aif_Index;
    MarkerChunk_Header->TotalMarkers = FlipEndianness(*Aif_MarkerChunk_HeaderTotalMarkers);
    BytesRead += sizeof(MarkerChunk_Header->TotalMarkers);

    return(BytesRead);
}

int
App_Parse_Aif_Chunk(uint8 *Aif_Index, int32 TotalMarkers)
{
    int BytesNeededToStoreMarkers = 0;
    for(int MarkerNumber = 0; 
            MarkerNumber < TotalMarkers;  
            MarkerNumber++)
    {
        uint8 *PointerToStringLen = (Aif_Index + MARKER_BOILERPLATE);
        BytesNeededToStoreMarkers += *PointerToStringLen;
        Aif_Index += (MARKER_BOILERPLATE + *PointerToStringLen);
        ++Aif_Index;
    }

    BytesNeededToStoreMarkers += ( ( offsetof(marker, MarkerName) ) * TotalMarkers );
    return(BytesNeededToStoreMarkers);
}

int32
App_Parse_Aif_Chunk(uint8 *Aif_MarkerChunk_DataStart, 
        marker_chunk_header *MarkerChunk_Header, 
        int BytesNeededToStoreMarkers)
{
    int32 BytesRead = 0;

    //if there are Markers in the aif file
    if(MarkerChunk_Header->TotalMarkers > 0)
    {
        // Allocate memory for the Markers
        uint8 *HeapMarkerChunk_DataStart = (uint8 *)Win32_AllocateMemory(BytesNeededToStoreMarkers, __func__);

        // Store the address of the Markers in the Marker chunk header
        MarkerChunk_Header->Markers = HeapMarkerChunk_DataStart;

        // Declare two pointers that we use as indices: 
        //
        //    One pointer to index into the Marker data in the 
        //    .aif file (this is the data that we're going to copy
        //    after flipping the endianness)
        //
        //    One pointer to index into the memory we allocated
        //    for the Marker data (where the copied data is
        //    going to go)
        //
        // Note that we can't treat the allocated memory like an array 
        // and index it like HeapMarkerChunk_DataStart[1], etc., because 
        // the MarkerName member of the Marker struct is not known at 
        // compile time. This is because it's a variable-length string. 
        // Since the length of the string varies, the size of the Marker 
        // structs vary, and therefore they can't be stored in an array.
        uint8 *Aif_Index = (uint8 *)Aif_MarkerChunk_DataStart;
        uint8 *HeapMarkerIndex = (uint8 *)HeapMarkerChunk_DataStart;

        for(int MarkerNumber = 0; 
                MarkerNumber < MarkerChunk_Header->TotalMarkers;
                MarkerNumber++)
        {
            // Read the Marker ID
            //	  Confusingly, the .aif spec calls the integer that uniquely identifies
            //    each Marker an "ID", despite using "ID" exclusively for 
            //    4-byte char strings elsewhere in the spec. Here it's a 
            //    16-bit int.
            //
            //	  Note: AdvancePointer will have no effect if this
            //	  is the first time through the loop
            Aif_Index = AdvancePointer(Aif_MarkerChunk_DataStart, BytesRead);
            HeapMarkerIndex = AdvancePointer(HeapMarkerChunk_DataStart, BytesRead);
            int16 *Aif_MarkerChunk_ID = (int16 *)Aif_Index;
            int16 *HeapMarkerChunk_ID = (int16 *)HeapMarkerIndex;
            *HeapMarkerChunk_ID = FlipEndianness(*Aif_MarkerChunk_ID);

            // Per the .aif spec, the ID must be a positive integer
            ValidateIntegerRange(*HeapMarkerChunk_ID, 0, INT_MAX, __func__);
            BytesRead += sizeof(*HeapMarkerChunk_ID);

            // Read the Marker position
            Aif_Index = AdvancePointer(Aif_MarkerChunk_DataStart, BytesRead);
            HeapMarkerIndex = AdvancePointer(HeapMarkerChunk_DataStart, BytesRead);
            uint32 *Aif_MarkerChunk_Position = (uint32 *)Aif_Index;
            uint32 *HeapMarkerChunk_Position = (uint32 *)HeapMarkerIndex;
            *HeapMarkerChunk_Position = FlipEndianness(*Aif_MarkerChunk_Position);
            BytesRead += sizeof(*HeapMarkerChunk_Position);

            // Read the length of the string containing the Marker's name 
            Aif_Index = AdvancePointer(Aif_MarkerChunk_DataStart, BytesRead);
            HeapMarkerIndex = AdvancePointer(HeapMarkerChunk_DataStart, BytesRead);
            // This is just an 8-bit uint, so we don't have to flip endianness
            uint8 *Aif_MarkerChunk_NameStringLen = (uint8 *)Aif_Index;
            uint8 *HeapMarkerChunk_NameStringLen = (uint8 *)HeapMarkerIndex;
            *HeapMarkerChunk_NameStringLen = *Aif_MarkerChunk_NameStringLen;
            BytesRead += sizeof(*HeapMarkerChunk_NameStringLen);

            // Read the string containing the Marker's name
            Aif_Index = AdvancePointer(Aif_MarkerChunk_DataStart, BytesRead);
            HeapMarkerIndex = AdvancePointer(HeapMarkerChunk_DataStart, BytesRead);
            // Strings don't need their endianness flipped, so we just do a straight copy
            SteenCopy(Aif_Index, HeapMarkerIndex, (int)*HeapMarkerChunk_NameStringLen);
            // Increment BytesRead by the number of bytes in the string
            BytesRead += *HeapMarkerChunk_NameStringLen;
        }

    }
    return(BytesRead);
}

int
App_Parse_Aif_Chunk(uint8 *Aif_InstrumentChunk_DataStart, instrument_chunk *InstrumentChunk)
{
    // Keep track of number of bytes we've read so that
    //	  we can compute necessary offsets
    int32 BytesRead = 0;

    uint8 *Aif_Index = (uint8 *)Aif_InstrumentChunk_DataStart;

    // Read the ID
    char *InstrumentChunk_IDStart = (char *)Aif_Index;
    ReadID(InstrumentChunk_IDStart, InstrumentChunk->ID);
    BytesRead += ID_WIDTH;

    // Read the ChunkSize value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int32 *InstrumentChunk_Size = (int32 *)Aif_Index;
    InstrumentChunk->ChunkSize = FlipEndianness(*InstrumentChunk_Size);
    BytesRead += sizeof(InstrumentChunk->ChunkSize);

    // Read the BaseNote value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int8 *BaseNote = (int8 *)Aif_Index;
    InstrumentChunk->BaseNote = *BaseNote;
    InstrumentChunk->BaseNoteDecode = (char *)MidiNoteLUT[InstrumentChunk->BaseNote];
    BytesRead += sizeof(InstrumentChunk->BaseNote);

    // Read the Detune value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    char *Detune = (char *)Aif_Index;
    InstrumentChunk->Detune = *Detune;
    BytesRead += sizeof(InstrumentChunk->Detune);

    // Read the LowNote value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    char *LowNote = (char *)Aif_Index;
    InstrumentChunk->LowNote = *LowNote;
    InstrumentChunk->LowNoteDecode = (char *)MidiNoteLUT[InstrumentChunk->LowNote];
    BytesRead += sizeof(InstrumentChunk->LowNote);

    // Read the HighNote value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    char *HighNote = (char *)Aif_Index;
    InstrumentChunk->HighNote = *HighNote;
    InstrumentChunk->HighNoteDecode = (char *)MidiNoteLUT[InstrumentChunk->HighNote];
    BytesRead += sizeof(InstrumentChunk->HighNote);

    // Read the LowVelocity value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    char *LowVelocity = (char *)Aif_Index;
    InstrumentChunk->LowVelocity = *LowVelocity;
    BytesRead += sizeof(InstrumentChunk->LowVelocity);

    // Read the HighVelocity value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    char *HighVelocity = (char *)Aif_Index;
    InstrumentChunk->HighVelocity = *HighVelocity;
    BytesRead += sizeof(InstrumentChunk->HighVelocity);

    // Read the Gain value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int16 *Gain = (int16 *)Aif_Index;
    InstrumentChunk->Gain = FlipEndianness(*Gain);
    BytesRead += sizeof(InstrumentChunk->Gain);

    // Read the SustainLoop value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int16 *SustainLoopPlayMode = (int16 *)Aif_Index;
    InstrumentChunk->SustainLoop.PlayMode = FlipEndianness(*SustainLoopPlayMode);
    BytesRead += sizeof(InstrumentChunk->SustainLoop.PlayMode);

    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int16 *SustainLoopBeginLoopMarker = (int16 *)Aif_Index;
    InstrumentChunk->SustainLoop.BeginLoopMarker = FlipEndianness(*SustainLoopBeginLoopMarker);
    BytesRead += sizeof(InstrumentChunk->SustainLoop.BeginLoopMarker);

    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int16 *SustainLoopEndLoopMarker = (int16 *)Aif_Index;
    InstrumentChunk->SustainLoop.EndLoopMarker = FlipEndianness(*SustainLoopEndLoopMarker);
    BytesRead += sizeof(InstrumentChunk->SustainLoop.EndLoopMarker);

    // Read the ReleaseLoop value
    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int16 *ReleaseLoopPlayMode = (int16 *)Aif_Index;
    InstrumentChunk->ReleaseLoop.PlayMode = FlipEndianness(*ReleaseLoopPlayMode);
    BytesRead += sizeof(InstrumentChunk->ReleaseLoop.PlayMode);

    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int16 *ReleaseLoopBeginLoopMarker = (int16 *)Aif_Index;
    InstrumentChunk->ReleaseLoop.BeginLoopMarker = FlipEndianness(*ReleaseLoopBeginLoopMarker);
    BytesRead += sizeof(InstrumentChunk->ReleaseLoop.BeginLoopMarker);

    Aif_Index = AdvancePointer(Aif_InstrumentChunk_DataStart, BytesRead);
    int16 *ReleaseLoopEndLoopMarker = (int16 *)Aif_Index;
    InstrumentChunk->ReleaseLoop.EndLoopMarker = FlipEndianness(*ReleaseLoopEndLoopMarker);
    BytesRead += sizeof(InstrumentChunk->ReleaseLoop.EndLoopMarker);

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
App_Parse_Aif_Chunk(uint8 *Aif_FillerChunk_Header_Start, filler_chunk_header *FillerChunk_Header)
{
    int32 BytesRead = 0;

    // Define an pointer for indexing into the chunk
    uint8 *FillerChunk_HeaderIndex = Aif_FillerChunk_Header_Start;

    char *FillerChunk_HeaderIDStart = (char *)FillerChunk_HeaderIndex;
    ReadID(FillerChunk_HeaderIDStart, FillerChunk_Header->ID);
    BytesRead += ID_WIDTH;

    FillerChunk_HeaderIndex = AdvancePointer(Aif_FillerChunk_Header_Start, BytesRead);
    uint32 *TotalFillerBytes = (uint32 *)FillerChunk_HeaderIndex;
    FillerChunk_Header->TotalFillerBytes = FlipEndianness(*TotalFillerBytes);
    BytesRead += sizeof(FillerChunk_Header->TotalFillerBytes);

    return(BytesRead);
}

int 
App_Parse_Aif_Chunk(uint8 *Aif_SoundDataChunk_Header_Start, sound_data_chunk_header *SoundDataChunk_Header)
{

    int BytesRead = 0;

    uint8 *Aif_Index = Aif_SoundDataChunk_Header_Start;

    char *Aif_SoundDataChunk_Header_IDStart = (char *)Aif_Index;
    ReadID(Aif_SoundDataChunk_Header_IDStart, SoundDataChunk_Header->ID);
    BytesRead += ID_WIDTH;

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Header_Start, BytesRead);
    int32 *Aif_SoundDataChunk_Header_Size = (int32 *)Aif_Index;
    SoundDataChunk_Header->ChunkSize = FlipEndianness(*Aif_SoundDataChunk_Header_Size);
    BytesRead += sizeof(SoundDataChunk_Header->ChunkSize);

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Header_Start, BytesRead);
    uint32 *Aif_SoundDataChunk_Header_Offset = (uint32 *)Aif_Index;
    SoundDataChunk_Header->Offset = FlipEndianness(*Aif_SoundDataChunk_Header_Offset);
    BytesRead += sizeof(SoundDataChunk_Header->Offset);

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Header_Start, BytesRead);
    uint32 *SoundDataChunk_Header_BlockSize = (uint32 *)Aif_Index;
    SoundDataChunk_Header->BlockSize = FlipEndianness(*SoundDataChunk_Header_BlockSize);
    BytesRead += sizeof(SoundDataChunk_Header->BlockSize);

    Aif_Index = AdvancePointer(Aif_SoundDataChunk_Header_Start, BytesRead);
    SoundDataChunk_Header->SamplesStart = Aif_Index;

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

void
ValidateAifFile(form_chunk *FormChunk, common_chunk *CommonChunk, 
		sound_data_chunk_header *SoundDataChunkHeader,
		uint8 *Aif_FileStart, int TimesChunkAppears[CHUNK_ID_HASH_ARRAY_SIZE])
{
    // STOP: Update this function so that we just go ahead and fill out
    //	  the Sound Data chunk while we're here since trying to validate
    //	  the file without having all of the chunk information stored
    //	  in a way that is convenient to access is too cumbersome
    uint64 Aif_TotalBytesInFile = ID_WIDTH + FormChunk->ChunkSize;
    uint8 *Aif_LastByteInFile = Aif_FileStart + (Aif_TotalBytesInFile);
    uint8 *Aif_FileIndex = FormChunk->SubChunksStart;

    uint8 *SoundDataChunkStart = 0;
    while(Aif_FileIndex < Aif_LastByteInFile)
    {
	char *ThisChunksID = (char *)Aif_FileIndex;
	unsigned int HashedID = GPerfHasher(ThisChunksID, ID_WIDTH);
	
	// Maybe these loops should be a function since they are exactly the same
	if(HashedID == COMMON_CHUNK)
	{
	    if(TimesChunkAppears[COMMON_CHUNK] > 0)
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
	    else
	    {
		App_Parse_Aif_Chunk(Aif_FileIndex, CommonChunk);
		TimesChunkAppears[COMMON_CHUNK]++;
	    }
	}
	else if(HashedID == SOUND_DATA_CHUNK)
	{
	    if(TimesChunkAppears[SOUND_DATA_CHUNK] > 0)
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
	    else
	    {
		App_Parse_Aif_Chunk(Aif_FileIndex, SoundDataChunkHeader);
		TimesChunkAppears[SOUND_DATA_CHUNK]++;
	    }
	}
	else
	{
	    TimesChunkAppears[HashedID]++;
	}

	Aif_FileIndex += ID_WIDTH;
	uint32 *BytesToSkipToNextChunk = (uint32 *)Aif_FileIndex;
	Aif_FileIndex += *BytesToSkipToNextChunk;
    }

    // Confirm we found a Common Chunk
    if(TimesChunkAppears[COMMON_CHUNK] == 0)
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

    // If the Common Chunk says the file has sample frames, then there must be
    //	  a Sound Data chunk
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
    // The Sound Data and Common chunks need to agree on the number of bytes
    //	  of samples the .aif file contains. 
    //
    //	  First we compute the number of bytes of samples the Sound Data chunk 
    //	  should have, based on some metadata the Common Chunk reports...
    int32 CommonChunk_BytesPerSample = (CommonChunk->SampleSize / BITS_IN_BYTE);
    
    int32 ExpectedSoundDataChunkSize = ( CommonChunk->NumSampleFrames * 
					    CommonChunk->NumChannels * 
					    CommonChunk_BytesPerSample );


    // Sound Data chunk's ChunkSize field accounts for some boilerplate that 
    //	  we don't want to include here, so subtract it
    int SoundDataChunk_ChunkSize_WithoutBoilerplate = (
							SoundDataChunkHeader->ChunkSize - 
							sizeof(SoundDataChunkHeader->Offset) + 
							sizeof(SoundDataChunkHeader->BlockSize)
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
}
