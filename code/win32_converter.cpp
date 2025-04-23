#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;



static char *MidiNoteLUT[128] = {
    /*  0 – 11 */   "C-2",  "C#-2", "D-2",  "D#-2", "E-2",  "F-2",  "F#-2", "G-2",  "G#-2", "A-2",  "A#-2", "B-2",
    /* 12 – 23 */   "C-1",  "C#-1", "D-1",  "D#-1", "E-1",  "F-1",  "F#-1", "G-1",  "G#-1", "A-1",  "A#-1", "B-1",
    /* 24 – 35 */   "C0",   "C#0",  "D0",   "D#0",  "E0",   "F0",   "F#0",  "G0",   "G#0",  "A0",   "A#0",  "B0",
    /* 36 – 47 */   "C1",   "C#1",  "D1",   "D#1",  "E1",   "F1",   "F#1",  "G1",   "G#1",  "A1",   "A#1",  "B1",
    /* 48 – 59 */   "C2",   "C#2",  "D2",   "D#2",  "E2",   "F2",   "F#2",  "G2",   "G#2",  "A2",   "A#2",  "B2",
    /* 60 – 71 */   "C3",   "C#3",  "D3",   "D#3",  "E3",   "F3",   "F#3",  "G3",   "G#3",  "A3",   "A#3",  "B3",
    /* 72 – 83 */   "C4",   "C#4",  "D4",   "D#4",  "E4",   "F4",   "F#4",  "G4",   "G#4",  "A4",   "A#4",  "B4",
    /* 84 – 95 */   "C5",   "C#5",  "D5",   "D#5",  "E5",   "F5",   "F#5",  "G5",   "G#5",  "A5",   "A#5",  "B5",
    /* 96 –107 */   "C6",   "C#6",  "D6",   "D#6",  "E6",   "F6",   "F#6",  "G6",   "G#6",  "A6",   "A#6",  "B6",
    /*108 –119 */   "C7",   "C#7",  "D7",   "D#7",  "E7",   "F7",   "F#7",  "G7",   "G#7",  "A7",   "A#7",  "B7",
    /*120 –127 */   "C8",   "C#8",  "D8",   "D#8",  "E8",   "F8",   "F#8",  "G8"
};

// stop: update inst struct chars to point to LUT

#define Assert(Value) if(!(Value)) {*(int *)0 = 0;}
#define ArrayCount(Array) sizeof(Array) / (sizeof(Array[0]))

/*
 * The aif spec: https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/AIFF/AIFF.html 
 * uses a C-like language to define an aif file's structure. This includes
 * an "ID" data type, defined as follows:
 *
 *    "32 bits, the concatenation of four printable ASCII character (sic)
 *    in the range ' ' (SP, 0x20) through '~' (0x7E).
 *    Spaces (0x20) cannot precede printing characters; trailing spaces
 *    are allowed. Control characters are forbidden."
 *
 * The spec declares multiple file header fields using the ID type.
 * Per above, it is always 4 characters. We thus define it:
*/
#define ID_WIDTH 4

/*
 * aif stores sample rate in an 80-bit (10-byte) floating point data type
 * called "extended." MSVC doesn't support this type. We attempt to
 * truncate it and store it in a double. We abort the conversion if
 * the truncation would result in data loss.
*/
#define EXTENDED_WIDTH 10
#define BITS_IN_BYTE 8
#define MAX_STRING_LEN 255 

/* TODO:
 *    1. For chunk types that should only appear once in a file, assert one doesn't already exist
 *	    when we start reading
 *
 *    2. Use Windows data types in the platform layer
 *
 *    3. Make File Index advancement mechanic more explicit by
 *	  having the read chunk functions return a BytesRead
 *	  value and then increment the FileIndex pointer by
 *	  that value
 *
 *    4. unify terminology in struct fields e.g. "DataSize" vs. "ChunkSize"
 *
 *    5. test InstrumentChunk loop parsers with nonzero data
 *    
 *    6. have ReadID return bytes read and use this to
 *	    advance FileIndex
 *	  
*/

#include "converter.h"

void
PrintDebugString(int32 i)
{
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "%d\n", i);
    OutputDebugStringA(buffer);
}

#if 0
void
PrintDebugString(double d)
{
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "%ld\n", d);
    OutputDebugStringA(buffer);
}
#endif

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


uint8 *
ReadAddress(uint8 *AddressOfHeaderInFile)
{
    return(AddressOfHeaderInFile);
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

inline int32
GetIntFromAsciiDigit(char AsciiDigit)
{
    return AsciiDigit - '0';
}

/*void*/
/*ReadChunkID(uint8 *ChunkIDStart, form_chunk *FormChunk)*/
/*{*/
/*    for(int i = 0; i < ID_WIDTH; i++)*/
/*    {*/
/*	uint8 *Letter = (uint8 *)(ChunkIDStart + i);*/
/*	FormChunk->ID[i] = *Letter;*/
/*    }*/
/**/
/*    FormChunk->ID[ID_WIDTH] = '\0';*/
/*}*/
/**/
/*void*/
/*ReadChunkID(uint8 *ChunkIDStart, common_chunk *CommonChunk)*/
/*{*/
/*    for(int i = 0; i < ID_WIDTH; i++)*/
/*    {*/
/*	uint8 *Letter = (uint8 *)(ChunkIDStart + i);*/
/*	CommonChunk->ID[i] = *Letter;*/
/*    }*/
/**/
/*    CommonChunk->ID[ID_WIDTH] = '\0';*/
/*}*/

/*void*/
/*ReadChunkType(uint8 *ChunkTypeStart, form_chunk *ChunkStruct)*/
/*{    */
/*    for(int i = 0; i < ID_WIDTH; i++)*/
/*    {*/
/*	uint8 *Letter = (uint8 *)(ChunkTypeStart + i);*/
/*	ChunkStruct->Type[i] = *Letter;*/
/*    }*/
/**/
/*    ChunkStruct->Type[ID_WIDTH] = '\0';*/
/*}*/

/*void*/
/*ReadChunkDataStart(uint64 ChunkDataStart, form_chunk *ChunkStruct)*/
/*{*/
/*    ChunkStruct->DataStart = ChunkDataStart;*/
/*}*/

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

inline uint32
GetSampleRate(uint8 *FirstByteOfExtendedPrecisionFloat)
{

    uint16 ExponentBytesToFlip = *(uint16 *)FirstByteOfExtendedPrecisionFloat; 
    uint16 Exponent = 0;
    Exponent = (
		(uint16)((ExponentBytesToFlip & 255) << 8) |
		(uint16)((ExponentBytesToFlip >> 8) & 255)
		);
    int32 ExtendedPrecisionFloatingPointExponentBias = -16383;
    Exponent += ExtendedPrecisionFloatingPointExponentBias;

    FirstByteOfExtendedPrecisionFloat += 2;
    uint64 FractionBytesToFlip = *(uint64 *)FirstByteOfExtendedPrecisionFloat;
    uint64 Fraction = 0;

    Fraction = (
			(uint64)((FractionBytesToFlip & 255) << 56) |
			(uint64)(((FractionBytesToFlip >> 8) & 255) << 48) |
			(uint64)(((FractionBytesToFlip >> 16) & 255) << 40) |
			(uint64)(((FractionBytesToFlip >> 24) & 255) << 32) |
			(uint64)(((FractionBytesToFlip >> 32) & 255) << 24) |
			(uint64)(((FractionBytesToFlip >> 40) & 255) << 16) |
			(uint64)(((FractionBytesToFlip >> 48) & 255) << 8) |
			(uint64)(((FractionBytesToFlip >> 56) & 255))
		);

    int32 TimesToShiftRight = 63 - Exponent;
    Fraction >>= TimesToShiftRight;

    return(Fraction);
    
}

inline uint64
FlipEndianness(uint64 IntToFlip)
{
    uint64 IntToWrite = 0;

    for(int i = 0; i < sizeof(uint64); i++)
    {
	uint64 ByteToFlip = 0;

	int ShiftRightAmt = i * 8;
	ByteToFlip = (IntToFlip >> ShiftRightAmt) & (uint8) 0xFF;
	
	int ShiftLeftAmt = 56 - (i * 8);
	IntToWrite |= (ByteToFlip << ShiftLeftAmt);
    }

    return(IntToWrite);
    
}

void
ReadMarkerChunkHeader(uint8 *MarkerChunkHeaderStart, marker_chunk_header *MarkerChunkHeader)
{
    //stop: check this function
    //	rewrite it so that we pass in a pointer to an uninitialized
    //	marker chunk structure and fill it out

    uint8 *MarkerChunkIndex = MarkerChunkHeaderStart;

    MarkerChunkHeader->Address = MarkerChunkIndex;
    char *MarkerChunkHeaderIDStart = (char *)MarkerChunkHeader->Address;
    ReadID(MarkerChunkHeaderIDStart, MarkerChunkHeader->ID);
    MarkerChunkIndex += sizeof(MarkerChunkHeader->ID) - 1;
    int32 *MarkerChunkHeaderSize = (int32 *)MarkerChunkIndex;
    MarkerChunkHeader->Size = FlipEndianness(*MarkerChunkHeaderSize);
    MarkerChunkIndex += sizeof(MarkerChunkHeader->Size);
    uint16 *MarkerChunkHeaderNumMarkers = (uint16 *)MarkerChunkIndex;
    MarkerChunkHeader->NumMarkers = FlipEndianness(*MarkerChunkHeaderNumMarkers);
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
    LPCWSTR Filename = L"SC88PR~1.AIF";
    uint8 *FileAddress;

    HANDLE HeapHandle = GetProcessHeap();
    if(HeapHandle)
    {
	FileAddress = (uint8 *)Win32GetFilePointer(Filename);
    }
    else
    {
	OutputDebugStringA("failed to get process heap handle\n");
	return(0);
    }

    uint8 *FileIndex = FileAddress;

    form_chunk FormChunk = {};

    uint8 *FormChunkStart = FileIndex;
    FormChunk.Address = ReadAddress(FormChunkStart);

    char *FormChunkIDStart = (char *)FormChunk.Address;
    ReadID(FormChunkIDStart, FormChunk.ID);

    FileIndex += sizeof(FormChunk.ID) - 1;
    int32 *FormChunkDataSizeAddress = (int32 *)FileIndex;
    FormChunk.DataSize = FlipEndianness(*FormChunkDataSizeAddress);

    FileIndex += sizeof(FormChunk.DataSize);
    char *FormChunkTypeStart = (char *)FileIndex;
    ReadID(FormChunkTypeStart, FormChunk.Type);

    FileIndex += sizeof(FormChunk.Type) - 1;
    uint8 *FormChunkDataStart = FileIndex;
    FormChunk.DataStart = ReadAddress(FormChunkDataStart);
/* 
    uint8 *FormChunkAddress = FileIndex;
    FormChunk.Address = ReadAddress(FormChunkAddress);

    uint8 *FormChunkIDStart = (uint8 *)FormChunk.Address;
    ReadID(FormChunkIDStart, FormChunk.ID);
*/
    common_chunk CommonChunk = {};

    FileIndex = FormChunk.DataStart;
    uint8 *CommonChunkStart = FileIndex;
    CommonChunk.Address = ReadAddress(CommonChunkStart);

    char *CommonChunkIDStart = (char *)CommonChunk.Address;
    ReadID(CommonChunkIDStart, CommonChunk.ID);

    FileIndex += sizeof(CommonChunk.ID) - 1;
    int32 *CommonChunkDataSizeAddress = (int32 *)FileIndex;
    CommonChunk.DataSize = FlipEndianness(*CommonChunkDataSizeAddress);
    //per the spec, DataSize is always 18
    Assert(CommonChunk.DataSize == 18);

    FileIndex += sizeof(CommonChunk.DataSize);
    int16 *CommonChunkNumChannels = (int16 *)FileIndex;
    CommonChunk.NumChannels = FlipEndianness(*CommonChunkNumChannels);

    FileIndex += sizeof(CommonChunk.NumChannels);
    uint32 *CommonChunkNumSampleFrames = (uint32 *)FileIndex;
    CommonChunk.NumSampleFrames = FlipEndianness(*CommonChunkNumSampleFrames);

    FileIndex += sizeof(CommonChunk.NumSampleFrames);
    int16 *CommonChunkSampleSize = (int16 *)FileIndex;
    CommonChunk.SampleSize = FlipEndianness(*CommonChunkSampleSize);

    FileIndex += sizeof(CommonChunk.SampleSize);
    CommonChunk.SampleRate = GetSampleRate(FileIndex);

    FileIndex += EXTENDED_WIDTH;
    marker_chunk_header MarkerChunkHeader = {};
    ReadMarkerChunkHeader(FileIndex, &MarkerChunkHeader);
    int32 BytesRead = ((sizeof(MarkerChunkHeader.ID ) - 1) + 
		    sizeof(MarkerChunkHeader.Size));
    FileIndex += BytesRead;
    uint8 *MarkersIndex = FileIndex + sizeof(MarkerChunkHeader.NumMarkers);
    marker *Markers = 0;

    if(MarkerChunkHeader.NumMarkers != 0)
    {
	int32 HeapSpaceForMarkers = (sizeof(marker) * MarkerChunkHeader.NumMarkers);
	Markers = (marker *)HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, HeapSpaceForMarkers);
	MarkerChunkHeader.Markers = Markers;

	for(int ThisMarker = 0; ThisMarker < MarkerChunkHeader.NumMarkers; ThisMarker++)
	{
	    Markers[ThisMarker] = {};
	    int16 *MarkerID = (int16 *)MarkersIndex;
	    Markers[ThisMarker].ID = FlipEndianness(*MarkerID);
	    MarkersIndex += sizeof(Markers[ThisMarker].ID);
	    uint32 *MarkerPosition = (uint32 *)MarkersIndex;
	    Markers[ThisMarker].Position = FlipEndianness(*MarkerPosition);
	    MarkersIndex += sizeof(Markers[ThisMarker].Position);
	    uint8 *PStringLen = MarkersIndex;
	    Markers[ThisMarker].PString.StringLength = *PStringLen;
	    MarkersIndex += sizeof(Markers[ThisMarker].PString.StringLength);
	    uint8 *PStringIndex = MarkersIndex;
	    for(int i = 0; i < Markers[ThisMarker].PString.StringLength; i++)
	    {
		char *Letter = (char *)(PStringIndex + i);
		Markers[ThisMarker].PString.Letters[i] = *Letter;
		
	    }
	    uint8 WhereToPutNullChar = Markers[ThisMarker].PString.StringLength;
	    Markers[ThisMarker].PString.Letters[WhereToPutNullChar] = '\0';
	    MarkersIndex += Markers[ThisMarker].PString.StringLength;

	}
    }

    FileIndex += MarkerChunkHeader.Size;

    instrument_chunk InstrumentChunk = {};

    char *InstrumentChunkIDStart = (char *)FileIndex;
    ReadID(InstrumentChunkIDStart, InstrumentChunk.ID);
    FileIndex += sizeof(InstrumentChunk.ID) - 1;


    /*int32 ChunkSize;*/
    int32 *InstrumentChunkSize = (int32 *)FileIndex;
    InstrumentChunk.ChunkSize = FlipEndianness(*InstrumentChunkSize);
    FileIndex += sizeof(InstrumentChunk.ChunkSize);

    /*char BaseNote;*/
    int8 *BaseNote = (int8 *)FileIndex;
    InstrumentChunk.BaseNote = *BaseNote;
    InstrumentChunk.BaseNoteDecode = MidiNoteLUT[InstrumentChunk.BaseNote];
    FileIndex += sizeof(InstrumentChunk.BaseNote);

    /*char Detune;*/
    char *Detune = (char *)FileIndex;
    InstrumentChunk.Detune = *Detune;
    FileIndex += sizeof(InstrumentChunk.Detune);

    /*char LowNote;*/
    char *LowNote = (char *)FileIndex;
    InstrumentChunk.LowNote = *LowNote;
    InstrumentChunk.LowNoteDecode = MidiNoteLUT[InstrumentChunk.LowNote];
    FileIndex += sizeof(InstrumentChunk.LowNote);

    /*char HighNote;*/
    char *HighNote = (char *)FileIndex;
    InstrumentChunk.HighNote = *HighNote;
    InstrumentChunk.HighNoteDecode = MidiNoteLUT[InstrumentChunk.HighNote];
    FileIndex += sizeof(InstrumentChunk.HighNote);

    /*char LowVelocity;*/
    char *LowVelocity = (char *)FileIndex;
    InstrumentChunk.LowVelocity = *LowVelocity;
    FileIndex += sizeof(InstrumentChunk.LowVelocity);

    /*char HighVelocity;*/
    char *HighVelocity = (char *)FileIndex;
    InstrumentChunk.HighVelocity = *HighVelocity;
    FileIndex += sizeof(InstrumentChunk.HighVelocity);

    /*int16 Gain;*/
    int16 *Gain = (int16 *)FileIndex;
    InstrumentChunk.Gain = FlipEndianness(*Gain);
    FileIndex += sizeof(InstrumentChunk.Gain);

    /*loop *SustainLoop;*/
    int16 *SustainLoopPlayMode = (int16 *)FileIndex;
    InstrumentChunk.SustainLoop.PlayMode = FlipEndianness(*SustainLoopPlayMode);
    FileIndex += sizeof(InstrumentChunk.SustainLoop.PlayMode);
    int16 *SustainLoopBeginLoopMarker = (int16 *)FileIndex;
    InstrumentChunk.SustainLoop.BeginLoopMarker = FlipEndianness(*SustainLoopBeginLoopMarker);
    FileIndex += sizeof(InstrumentChunk.SustainLoop.BeginLoopMarker);
    int16 *SustainLoopEndLoopMarker = (int16 *)FileIndex;
    InstrumentChunk.SustainLoop.EndLoopMarker = FlipEndianness(*SustainLoopEndLoopMarker);
    FileIndex += sizeof(InstrumentChunk.SustainLoop.EndLoopMarker);

    /*loop *ReleaseLoop;*/
    int16 *ReleaseLoopPlayMode = (int16 *)FileIndex;
    InstrumentChunk.ReleaseLoop.PlayMode = FlipEndianness(*ReleaseLoopPlayMode);
    FileIndex += sizeof(InstrumentChunk.ReleaseLoop.PlayMode);
    int16 *ReleaseLoopBeginLoopMarker = (int16 *)FileIndex;
    InstrumentChunk.ReleaseLoop.BeginLoopMarker = FlipEndianness(*ReleaseLoopBeginLoopMarker);
    FileIndex += sizeof(InstrumentChunk.ReleaseLoop.BeginLoopMarker);
    int16 *ReleaseLoopEndLoopMarker = (int16 *)FileIndex;
    InstrumentChunk.ReleaseLoop.EndLoopMarker = FlipEndianness(*ReleaseLoopEndLoopMarker);
    FileIndex += sizeof(InstrumentChunk.ReleaseLoop.EndLoopMarker);

    filler_chunk FillerChunk = {};
    int32 BytesReadInFillerChunk = ProcessFillerChunk(FileIndex, &FillerChunk);

    FileIndex += BytesReadInFillerChunk;
    FileIndex += FillerChunk.NumFillerBytes;

    //we may have just skipped thousands of bytes so assert
    //we aren't off in no man's land
    Assert(*FileIndex != 0);

    sound_data_chunk_header SoundDataChunkHeader = {};
    ProcessSoundDataChunkHeader(FileIndex, &SoundDataChunkHeader);
    FileIndex = SoundDataChunkHeader.DataStart;
    int32 BytesPerSample = CommonChunk.SampleSize / BITS_IN_BYTE;
    int32 BytesNeededForFlippedSamples = (CommonChunk.NumSampleFrames * 
					CommonChunk.NumChannels * 
					BytesPerSample);

    uint8 *FlippedSamplesStart = (uint8 *)HeapAlloc(HeapHandle, 
						HEAP_ZERO_MEMORY, 
						BytesNeededForFlippedSamples);
    uint8 *SamplesToFlipStart = SoundDataChunkHeader.DataStart;
    
    int BytesInSampleFrame = BytesPerSample * CommonChunk.NumChannels;
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
	    int SampleFrameOffset = Channel * BytesPerSample;

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
    
    HeapFree(HeapHandle, 0, Markers);
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
