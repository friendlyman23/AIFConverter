#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

static HANDLE HeapHandle = GetProcessHeap();


/* TODO:
 *    1. For chunk types that should only appear once in a file, assert one doesn't already exist
 *	    when we start reading
 *
 *    2. Use Windows data types in the platform layer
 *
 *    3. unify terminology in struct fields e.g. "DataSize" vs. "ChunkSize"
 *
 *    4. test InstrumentChunk loop parsers with nonzero data
 *    
 *    5. update ints for wav headers to be signed unless
 *	    they're 8 bits wide
 *
 *    6. Check to see that the functions we try to inline actually get inlined when
 *	  we turn on optimization
*/

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

inline uint8 *
AdvancePointer(uint8 *AifChunkStart, int32 BytesRead)
{
    uint8 *NewAddress = (uint8 *)(AifChunkStart + BytesRead);
    return(NewAddress);
}

void *
Win32GetFilePointer(LPCWSTR Filename)
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


inline uint8 *
ReadAddress(uint8 *AddressOfHeaderInFile)
{
    return(AddressOfHeaderInFile);
}


void
SteenCopy(uint8 *MemToCopy, uint8 *MemDestination, uint32 BytesToCopy)
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

    uint8 *ExtendedIndex = FirstByteOfExtendedPrecisionFloat;

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
    uint16 ExponentBytesToFlip = *(uint16 *)ExtendedIndex; 
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
    ExtendedIndex += 2;
    uint64 FractionBytesToFlip = *(uint64 *)ExtendedIndex;
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

// Functions named with the pattern "AppReadAif...Chunk" read the 
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
//    other "AppReadAif...Chunk" functions are not because they 
//    generally follow the patttern established here.

int32
AppReadAifFormChunk(uint8 *AifFormChunkStart, form_chunk *FormChunk)
{
    // We return the number of bytes this function read from the 
    //	  .aif file so main() knows how many bytes to skip to get to 
    //	  the next chunk in the .aif file
    int32 BytesRead = 0;

    // Declare a pointer to a single byte that we use to advance 
    //	  through the chunk and parse its data
    uint8 *AifFormChunkIndex = AifFormChunkStart;

    // Store the address of the first byte of the Form Chunk
    FormChunk->Address = ReadAddress(AifFormChunkIndex);

    // These AppReadAif...Chunk functions do not represent a large
    //	  share of the program's workload, so for readability,
    //	  we always declare a new pointer to each header item
    //	  as we encounter it, that explicitly names what it points to...
    char *AifFormChunkIDStart = (char *)FormChunk->Address;

    // ...and pass this pointer to a corresponding function
    //	  that parses it and stores its relevant data on the stack.
    ReadID(AifFormChunkIDStart, (char *)(&FormChunk->ID));

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
    AifFormChunkIndex = AdvancePointer(AifFormChunkStart, BytesRead);

    // and we keep doing this until we've finished parsing the chunk!
    int32 *AifFormChunkDataSizeAddress = (int32 *)AifFormChunkIndex;
    FormChunk->DataSize = FlipEndianness(*AifFormChunkDataSizeAddress);
    BytesRead += sizeof(FormChunk->DataSize);

    AifFormChunkIndex = AdvancePointer(AifFormChunkStart, BytesRead);
    char *AifFormChunkTypeStart = (char *)AifFormChunkIndex;
    ReadID(AifFormChunkTypeStart, (char *)(FormChunk->Type));

    // Per .aif spec, Form Chunk Type is always "AIFF"
    ValidateID(FormChunk->Type, "AIFF", __func__);
    BytesRead += ID_WIDTH;

    // The "data" of the Form Chunk is everything else
    //	  in the entire aif file.
    AifFormChunkIndex = AdvancePointer(AifFormChunkStart, BytesRead);
    uint8 *AifFormChunkDataStart = AifFormChunkIndex;
    FormChunk->DataStart = ReadAddress(AifFormChunkDataStart);

    return(BytesRead);
}

int32
AppReadAifCommonChunk(uint8 *AifCommonChunkStart, common_chunk *CommonChunk)
{

    // Keep track of number of bytes we've read so that
    //	  we can compute necessary offsets
    int32 BytesRead = 0;
    uint8 *AifCommonChunkIndex = AifCommonChunkStart;

    // Store the address of the first byte of the Common Chunk
    CommonChunk->Address = ReadAddress(AifCommonChunkStart);

    // Store the Common Chunk ID in its struct on the stack
    char *AifCommonChunkIDStart = (char *)CommonChunk->Address;
    ReadID(AifCommonChunkIDStart, (char *)CommonChunk->ID);
    // Check that we read a valid ID
    ValidateID(CommonChunk->ID, "COMM", __func__);
    // Compute updated BytesRead value
    BytesRead += ID_WIDTH;

    // Store the size of the Common Chunk
    AifCommonChunkIndex = AdvancePointer(AifCommonChunkStart, BytesRead);
    int32 *AifCommonChunkDataSizeAddress = (int32 *)AifCommonChunkIndex;
    CommonChunk->DataSize = FlipEndianness(*AifCommonChunkDataSizeAddress);
    // per the spec, DataSize is always 18
    ValidateInteger(CommonChunk->DataSize, 18, __func__);
    BytesRead += sizeof(CommonChunk->DataSize);

    // Store the number of audio channels
    AifCommonChunkIndex = AdvancePointer(AifCommonChunkStart, BytesRead);
    int16 *AifCommonChunkNumChannels = (int16 *)AifCommonChunkIndex;
    CommonChunk->NumChannels = FlipEndianness(*AifCommonChunkNumChannels);
    BytesRead += sizeof(CommonChunk->NumChannels);

    // Store the number of sample frames
    AifCommonChunkIndex = AdvancePointer(AifCommonChunkStart, BytesRead);
    uint32 *AifCommonChunkNumSampleFrames = (uint32 *)AifCommonChunkIndex;
    CommonChunk->NumSampleFrames = FlipEndianness(*AifCommonChunkNumSampleFrames);
    BytesRead += sizeof(CommonChunk->NumSampleFrames);

    // Store the sample size (the number of bytes dedicated to each sample)
    AifCommonChunkIndex = AdvancePointer(AifCommonChunkStart, BytesRead);
    int16 *AifCommonChunkSampleSize = (int16 *)AifCommonChunkIndex;
    CommonChunk->SampleSize = FlipEndianness(*AifCommonChunkSampleSize);
    BytesRead += sizeof(CommonChunk->SampleSize);

    // .aif files use a 10-byte, extended-width floating point number
    //	  to represent sample rate. See comments on the GetSampleRate
    //	  function for details.
    AifCommonChunkIndex = AdvancePointer(AifCommonChunkStart, BytesRead);
    uint8 *AifCommonChunkSampleRate = AifCommonChunkIndex;
    CommonChunk->SampleRate = GetSampleRate(AifCommonChunkSampleRate);
    BytesRead += EXTENDED_WIDTH;

    return(BytesRead);
}

// .aif files may need to specify the position of certain samples in the file
//    for use by software. For example, imagine a musician playing a digital
//    instrument based on a pre-recorded violin sound. The musician expects the
//    violin sound to sustain for as long as they hold down a key on their midi
//    keyboard.
//
//    .aif files encode such direction for software via a "Marker" chunk. 
//    In the example of the sustaining violin, an .aif file declares
//    the location of two Markers in the file that represent the beginning
//    and end points of the sustain phase. While the musician holds down the key,
//    the software "loops" over the samples between between the two Markers; 
//    when the key is released, the software plays the remainder of the samples.
//
//    Note that markers are not required. An .aif file containing a recording
//    of a snare drum, which is triggered once and does not loop, would probably
//    not include them.
//
//    For readability, we first read the header portion of the Marker chunk,
//    then read the actual Marker data in a separate function.
int32
AppReadAifMarkerChunkHeader(uint8 *AifMarkerChunkHeaderStart, marker_chunk_header *MarkerChunkHeader)
{
    
    // Keep track of number of bytes we've read so that
    //	  we can compute necessary offsets
    int32 BytesRead = 0;
    uint8 *AifMarkerChunkHeaderIndex = AifMarkerChunkHeaderStart;

    // Store the address of the first byte of the Marker Chunk
    MarkerChunkHeader->Address = AifMarkerChunkHeaderStart;

    // Store the Marker Chunk ID in its struct on the stack
    char *AifMarkerChunkHeaderIDStart = (char *)AifMarkerChunkHeaderIndex;
    ReadID(AifMarkerChunkHeaderIDStart, MarkerChunkHeader->ID);
    // Check that we read a valid ID
    ValidateID(MarkerChunkHeader->ID, "MARK", __func__);
    // Compute updated BytesRead value
    BytesRead += ID_WIDTH;

    // Store the size of the Marker Chunk
    AifMarkerChunkHeaderIndex = AdvancePointer(AifMarkerChunkHeaderStart, BytesRead);
    int32 *AifMarkerChunkHeaderSize = (int32 *)AifMarkerChunkHeaderIndex;
    MarkerChunkHeader->Size = FlipEndianness(*AifMarkerChunkHeaderSize);
    BytesRead += sizeof(MarkerChunkHeader->Size);

    // Store the number of Markers. We use this value later to know
    //	  how much space to put on the heap to store them
    AifMarkerChunkHeaderIndex = AdvancePointer(AifMarkerChunkHeaderStart, BytesRead);
    uint16 *AifMarkerChunkHeaderNumMarkers = (uint16 *)AifMarkerChunkHeaderIndex;
    MarkerChunkHeader->NumMarkers = FlipEndianness(*AifMarkerChunkHeaderNumMarkers);
    BytesRead += sizeof(MarkerChunkHeader->NumMarkers);

    return(BytesRead);
}

int32
AppReadAifMarkerChunkData(uint8 *AifMarkerChunkDataStart, marker_chunk_header *MarkerChunkHeader)
{
    int32 BytesRead = 0;
    
    uint8 *AifMarkerChunkDataIndex = AifMarkerChunkDataStart;

    //if there are markers in the aif file
    if(MarkerChunkHeader->NumMarkers > 0)
    {
	//allocate memory for the number of markers we read in the marker chunk header
	int32 HeapSpaceForMarkers = (sizeof(marker) * MarkerChunkHeader->NumMarkers);
	marker MarkersOnHeap[MarkerChunkHeader->NumMarkers]; 
	MarkersOnHeap = (marker *)HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, HeapSpaceForMarkers);
	
	//store the address of the markers in the marker chunk header
	MarkerChunkHeader->Markers = Markers;

	//read the marker data into structs on the heap. we have to use the heap
	//    since the number of markers will vary by .aif file and thus
	//    can't be known at compile time
	for(int ThisMarker = 0; ThisMarker < MarkerChunkHeader->NumMarkers; ThisMarker++)
	{
	    Markers[ThisMarker] = {};

	    int16 *MarkerID = (int16 *)AifMarkerChunkDataIndex;
	    Markers[ThisMarker].ID = FlipEndianness(*MarkerID);
	    BytesRead += sizeof(Markers[ThisMarker].ID);
	    AifMarkerChunkDataIndex = AdvancePointer(AifMarkerChunkDataStart, BytesRead);

	    uint32 *MarkerPosition = (uint32 *)AifMarkerChunkDataIndex;
	    Markers[ThisMarker].Position = FlipEndianness(*MarkerPosition);
	    BytesRead += sizeof(Markers[ThisMarker].Position);
	    AifMarkerChunkDataIndex = AdvancePointer(AifMarkerChunkDataStart, BytesRead);

	    // Read the length of the PString containing the Marker name.
	    //	  Because this this is just an 8-bit unsigned int,
	    //	  we don't need to flip the endianness.
	    uint8 *PStringLen = (uint8 *)AifMarkerChunkDataIndex;
	    Markers[ThisMarker].PString.StringLength = *PStringLen;
	    BytesRead += sizeof(Markers[ThisMarker].PString.StringLength);
	    AifMarkerChunkDataIndex = AdvancePointer(AifMarkerChunkDataStart, BytesRead);

	    uint8 *PStringLetters = (uint8 *)AifMarkerChunkDataIndex;
	    SteenCopy(PStringLetters, (uint8 *)(&Markers[ThisMarker].PString.Letters), 
			Markers[ThisMarker].PString.StringLength);
	    BytesRead += Markers[ThisMarker].PString.StringLength;

	    uint8 WhereToPutNullChar = Markers[ThisMarker].PString.StringLength;
	    Markers[ThisMarker].PString.Letters[WhereToPutNullChar] = '\0';

	}
    }
    return(BytesRead);

}


int32 
ProcessFillerChunk(uint8 *FillerChunkStart, filler_chunk *FillerChunk)
{
    int32 NumBytesRead = 0;

    uint8 *FillerChunkIndex = FillerChunkStart;
    char *FillerChunkIDStart = (char *)FillerChunkIndex;
    ReadID(FillerChunkIDStart, FillerChunk->ID);
    NumBytesRead += sizeof(FillerChunk->ID) - 1;

    FillerChunkIndex += sizeof(FillerChunk->ID) - 1;
    uint32 *NumFillerBytes = (uint32 *)FillerChunkIndex;
    FillerChunk->NumFillerBytes = FlipEndianness(*NumFillerBytes);
    NumBytesRead += sizeof(FillerChunk->NumFillerBytes);

    return(NumBytesRead);
}

void 
ProcessSoundDataChunkHeader(uint8 *SoundDataChunkHeaderStart, sound_data_chunk_header *SoundDataChunkHeader)
{
    uint8 *SoundDataChunkHeaderIndex = SoundDataChunkHeaderStart;

    char *SoundDataChunkHeaderIDStart = (char *)SoundDataChunkHeaderIndex;
    ReadID(SoundDataChunkHeaderIDStart, SoundDataChunkHeader->ID);
    SoundDataChunkHeaderIndex += sizeof(SoundDataChunkHeader->ID) - 1;

    int32 *SoundDataChunkHeaderSize = (int32 *)SoundDataChunkHeaderIndex;
    SoundDataChunkHeader->ChunkSize = FlipEndianness(*SoundDataChunkHeaderSize);
    SoundDataChunkHeaderIndex += sizeof(SoundDataChunkHeader->ChunkSize);

    uint32 *SoundDataChunkHeaderOffset = (uint32 *)SoundDataChunkHeaderIndex;
    SoundDataChunkHeader->Offset = FlipEndianness(*SoundDataChunkHeaderOffset);
    SoundDataChunkHeaderIndex += sizeof(SoundDataChunkHeader->Offset);

    uint32 *SoundDataChunkHeaderBlockSize = (uint32 *)SoundDataChunkHeaderIndex;
    SoundDataChunkHeader->BlockSize = FlipEndianness(*SoundDataChunkHeaderBlockSize);
    SoundDataChunkHeaderIndex += sizeof(SoundDataChunkHeader->BlockSize);

    SoundDataChunkHeader->DataStart = SoundDataChunkHeaderIndex;
}

int WinMain(HINSTANCE Instance, 
	HINSTANCE PrevInstance, 
	PSTR CmdLine, 
	int CmdShow)
{
    LPCWSTR AifFilename = L"SC88PR~1.AIF";
    uint8 *AifFileAddress;

    if(HeapHandle)
    {
	AifFileAddress = (uint8 *)Win32GetFilePointer(AifFilename);
    }
    else
    {
	// if we somehow lost the heap handle...
	OutputDebugStringA("failed to get process heap handle\n");
	return(0);
    }

    uint8 *AifFileIndex = AifFileAddress;

    // Read form chunk
    form_chunk FormChunk = {};
    uint8 *AifFormChunkStart = AifFileIndex;
    int32 AifFormChunkBytesRead = AppReadAifFormChunk(AifFormChunkStart, &FormChunk);
    AifFileIndex += AifFormChunkBytesRead;

    // Read common chunk
    common_chunk CommonChunk = {};
    uint8 *AifCommonChunkStart = AifFileIndex;
    int32 AifCommonChunkBytesRead = AppReadAifCommonChunk(AifCommonChunkStart, &CommonChunk);
    AifFileIndex += AifCommonChunkBytesRead;

    // Read marker chunk header
    marker_chunk_header MarkerChunkHeader = {};
    uint8 *AifMarkerChunkHeaderStart = AifFileIndex;
    int32 AifMarkerChunkHeaderBytesRead = AppReadAifMarkerChunkHeader(AifMarkerChunkHeaderStart, &MarkerChunkHeader);
    AifFileIndex += AifMarkerChunkHeaderBytesRead;

    // Read marker chunk data
    uint8 *AifMarkerChunkDataStart = AifFileIndex;
    int32 AifMarkerChunkDataBytesRead = AppReadAifMarkerChunkData(AifMarkerChunkDataStart, &MarkerChunkHeader);
    AifFileIndex += AifMarkerChunkDataBytesRead;

    instrument_chunk InstrumentChunk = {};
    char *InstrumentChunkIDStart = (char *)AifFileIndex;
    ReadID(InstrumentChunkIDStart, InstrumentChunk.ID);
    AifFileIndex += sizeof(InstrumentChunk.ID) - 1;


    /*int32 ChunkSize;*/
    int32 *InstrumentChunkSize = (int32 *)AifFileIndex;
    InstrumentChunk.ChunkSize = FlipEndianness(*InstrumentChunkSize);
    AifFileIndex += sizeof(InstrumentChunk.ChunkSize);

    /*char BaseNote;*/
    int8 *BaseNote = (int8 *)AifFileIndex;
    InstrumentChunk.BaseNote = *BaseNote;
    InstrumentChunk.BaseNoteDecode = MidiNoteLUT[InstrumentChunk.BaseNote];
    AifFileIndex += sizeof(InstrumentChunk.BaseNote);

    /*char Detune;*/
    char *Detune = (char *)AifFileIndex;
    InstrumentChunk.Detune = *Detune;
    AifFileIndex += sizeof(InstrumentChunk.Detune);

    /*char LowNote;*/
    char *LowNote = (char *)AifFileIndex;
    InstrumentChunk.LowNote = *LowNote;
    InstrumentChunk.LowNoteDecode = MidiNoteLUT[InstrumentChunk.LowNote];
    AifFileIndex += sizeof(InstrumentChunk.LowNote);

    /*char HighNote;*/
    char *HighNote = (char *)AifFileIndex;
    InstrumentChunk.HighNote = *HighNote;
    InstrumentChunk.HighNoteDecode = MidiNoteLUT[InstrumentChunk.HighNote];
    AifFileIndex += sizeof(InstrumentChunk.HighNote);

    /*char LowVelocity;*/
    char *LowVelocity = (char *)AifFileIndex;
    InstrumentChunk.LowVelocity = *LowVelocity;
    AifFileIndex += sizeof(InstrumentChunk.LowVelocity);

    /*char HighVelocity;*/
    char *HighVelocity = (char *)AifFileIndex;
    InstrumentChunk.HighVelocity = *HighVelocity;
    AifFileIndex += sizeof(InstrumentChunk.HighVelocity);

    /*int16 Gain;*/
    int16 *Gain = (int16 *)AifFileIndex;
    InstrumentChunk.Gain = FlipEndianness(*Gain);
    AifFileIndex += sizeof(InstrumentChunk.Gain);

    /*loop *SustainLoop;*/
    int16 *SustainLoopPlayMode = (int16 *)AifFileIndex;
    InstrumentChunk.SustainLoop.PlayMode = FlipEndianness(*SustainLoopPlayMode);
    AifFileIndex += sizeof(InstrumentChunk.SustainLoop.PlayMode);
    int16 *SustainLoopBeginLoopMarker = (int16 *)AifFileIndex;
    InstrumentChunk.SustainLoop.BeginLoopMarker = FlipEndianness(*SustainLoopBeginLoopMarker);
    AifFileIndex += sizeof(InstrumentChunk.SustainLoop.BeginLoopMarker);
    int16 *SustainLoopEndLoopMarker = (int16 *)AifFileIndex;
    InstrumentChunk.SustainLoop.EndLoopMarker = FlipEndianness(*SustainLoopEndLoopMarker);
    AifFileIndex += sizeof(InstrumentChunk.SustainLoop.EndLoopMarker);

    /*loop *ReleaseLoop;*/
    int16 *ReleaseLoopPlayMode = (int16 *)AifFileIndex;
    InstrumentChunk.ReleaseLoop.PlayMode = FlipEndianness(*ReleaseLoopPlayMode);
    AifFileIndex += sizeof(InstrumentChunk.ReleaseLoop.PlayMode);
    int16 *ReleaseLoopBeginLoopMarker = (int16 *)AifFileIndex;
    InstrumentChunk.ReleaseLoop.BeginLoopMarker = FlipEndianness(*ReleaseLoopBeginLoopMarker);
    AifFileIndex += sizeof(InstrumentChunk.ReleaseLoop.BeginLoopMarker);
    int16 *ReleaseLoopEndLoopMarker = (int16 *)AifFileIndex;
    InstrumentChunk.ReleaseLoop.EndLoopMarker = FlipEndianness(*ReleaseLoopEndLoopMarker);
    AifFileIndex += sizeof(InstrumentChunk.ReleaseLoop.EndLoopMarker);

    filler_chunk FillerChunk = {};
    int32 BytesReadInFillerChunk = ProcessFillerChunk(AifFileIndex, &FillerChunk);

    AifFileIndex += BytesReadInFillerChunk;
    AifFileIndex += FillerChunk.NumFillerBytes;

    //we may have just skipped thousands of bytes so assert
    //we aren't off in no man's land
    Assert(*AifFileIndex != 0);

    sound_data_chunk_header SoundDataChunkHeader = {};
    ProcessSoundDataChunkHeader(AifFileIndex, &SoundDataChunkHeader);
    AifFileIndex = SoundDataChunkHeader.DataStart;
    int32 CommonChunkBytesPerSample = (CommonChunk.SampleSize / BITS_IN_BYTE);
    int32 FlippedSamplesByteCount = (CommonChunk.NumSampleFrames * 
					CommonChunk.NumChannels * 
					CommonChunkBytesPerSample);

    uint8 *FlippedSamplesStart = (uint8 *)HeapAlloc(HeapHandle, 
						HEAP_ZERO_MEMORY, 
						FlippedSamplesByteCount);
    uint8 *SamplesToFlipStart = SoundDataChunkHeader.DataStart;
    
    int BytesInSampleFrame = CommonChunkBytesPerSample * CommonChunk.NumChannels;
    int HowManyBytesToFlip = BytesInSampleFrame * CommonChunk.NumSampleFrames;
    int SampleBytesWritten = 0;
    int SampleBytesRead = 0;

    for(int SampleFrame = 0;
	    SampleFrame < HowManyBytesToFlip; 
	    SampleFrame += BytesInSampleFrame)
    {
	for(int Channel = 0;
		Channel < CommonChunk.NumChannels;
		Channel++)
	{
	    int SampleFrameOffset = Channel * CommonChunkBytesPerSample;

	    uint8 FirstByteOfSamplePoint = SamplesToFlipStart[SampleFrame + SampleFrameOffset];
	    SampleBytesRead++;
	    uint8 SecondByteOfSamplePoint = SamplesToFlipStart[SampleFrame + SampleFrameOffset + 1];
	    SampleBytesRead++;
	    uint8 ThirdByteOfSamplePoint = SamplesToFlipStart[SampleFrame + SampleFrameOffset + 2];	    
	    SampleBytesRead++;
	    uint8 Mask = (FirstByteOfSamplePoint) ^ (ThirdByteOfSamplePoint);
	    FlippedSamplesStart[SampleFrame + SampleFrameOffset] = (FirstByteOfSamplePoint) ^ Mask;
	    SampleBytesWritten++;
	    FlippedSamplesStart[SampleFrame + SampleFrameOffset + 1] = SecondByteOfSamplePoint;
	    SampleBytesWritten++;
	    FlippedSamplesStart[SampleFrame + SampleFrameOffset + 2] = (ThirdByteOfSamplePoint) ^ Mask;
	    SampleBytesWritten++;
	}

    }
    char RiffType[ID_WIDTH];        // "WAVE" 4
				    
    // Format chunk
    char FormatChunkID[ID_WIDTH];   // "fmt " 4
    uint32 FormatChunkDataSize;// 16 for PCM 4
    uint16 FormatTag;  // 1 = PCM 2
    uint16 NumChannels;  // 2
    uint32 SampleRate;   // 4
    uint32 AvgBytesPerSec;     // sampleRate × numChannels × bitsPerSample/8 (4)
    uint16 BlockAlign;   // numChannels × bitsPerSample/8 (2)
    uint16 BitsPerSample;  

    // Data chunk
    char DataChunkID[ID_WIDTH]; // "data"

     /******************************
     * wav file size calculation, bytes:
     *	  	  
     *	  Riff chunk:
     *	    RiffType, 4
     *
     *	  Format chunk:
     *	    FormatChunkID, 4    
     *      FormatChunkDataSize, 4
     *      FormatTag, 2
     *	    NumChannels, 2                                                       
     *	    SampleRate, 4
     *	    AvgBytesPerSec, 4
     *	    BlockAlign, 2
     *	    BitsPerSample, 2  
     *
     *	  Data chunk:
     *	    DataChunkID, 4
     *	    DataChunkDataSize, (FlippedSamplesByteCount)
     *	  
     ******************************/

    wav_header WavHeader = {};
    SteenCopy((uint8 *)"RIFF", (uint8 *)WavHeader.GroupID, ID_WIDTH);
    SteenCopy((uint8 *)"WAVE", (uint8 *)WavHeader.RiffType, ID_WIDTH);
    SteenCopy((uint8 *)"fmt ", (uint8 *)WavHeader.FormatChunkID, ID_WIDTH);
    SteenCopy((uint8 *)"data", (uint8 *)WavHeader.DataChunkID, ID_WIDTH);
    WavHeader.WavFileSize = HEADER_SIZE_FOR_FILE_SIZE_CALC + FlippedSamplesByteCount;
    WavHeader.NumChannels = CommonChunk.NumChannels;
    WavHeader.SampleRate = CommonChunk.SampleRate;
    WavHeader.BitsPerSample = CommonChunk.SampleSize;

    uint32 WavHeaderBytesPerSample = (WavHeader.BitsPerSample / BITS_IN_BYTE); // for convenience!
    WavHeader.AvgBytesPerSec = WavHeader.SampleRate * 
				WavHeader.NumChannels * 
				WavHeaderBytesPerSample;
				
    WavHeader.BlockAlign = WavHeader.NumChannels * WavHeaderBytesPerSample;
    WavHeader.DataChunkDataSize = CommonChunk.NumSampleFrames * 
				    WavHeader.NumChannels * 
				    WavHeaderBytesPerSample;
    Assert(WavHeader.DataChunkDataSize == FlippedSamplesByteCount);

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
	    SamplesWriteResult = WriteFile(WavFileHandle, FlippedSamplesStart, 
		    WavHeader.DataChunkDataSize, &SampleBytesWritten, 0);
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
    HeapFree(HeapHandle, 0, (uint8 *)MarkerChunkHeader.Markers);
    HeapFree(HeapHandle, 0, FlippedSamplesStart);

#if 0
    uint8_t FortyEightKSampleRate[10] = {
	0x40, 0x0E, // Sign and exponent
	0xBB, 0x80, 0x00, 0x00, //Fraction bits
	0x00, 0x00, 0x00, 0x00
    };

    int32 Result = GetSampleRate(FortyEightKSampleRate);
#endif

    return(0);
}
