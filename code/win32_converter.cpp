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
	    else
	    {
		OutputDebugStringA("failed to get process heap handle\n");
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

struct form_chunk
{
    uint64 HeaderStart;
    char ID[ID_WIDTH + 1];
    int32 DataSize;
    char Type[ID_WIDTH + 1];
    uint64 DataStart;
};

struct common_chunk
{
    uint64 HeaderStart;
    char ID[ID_WIDTH + 1];
    int32 DataSize;
    int16 NumChannels;
    uint32 NumSampleFrames;
    int16 SampleSize;
    uint32 SampleRate;
};

void
ReadChunkAddress(uint64 ChunkAddress, form_chunk *FormChunk)
{
    FormChunk->HeaderStart = ChunkAddress;
}

void
ReadChunkAddress(uint64 ChunkAddress, common_chunk *CommonChunk)
{
    CommonChunk->HeaderStart = ChunkAddress;
}

void
ReadChunkID(uint8 *ChunkIDStart, form_chunk *FormChunk)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
	uint8 *Letter = (uint8 *)(ChunkIDStart + i);
	FormChunk->ID[i] = *Letter;
    }

    FormChunk->ID[ID_WIDTH] = '\0';
}

void
ReadChunkID(uint8 *ChunkIDStart, common_chunk *CommonChunk)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
	uint8 *Letter = (uint8 *)(ChunkIDStart + i);
	CommonChunk->ID[i] = *Letter;
    }

    CommonChunk->ID[ID_WIDTH] = '\0';
}

void
ReadChunkType(uint8 *ChunkTypeStart, form_chunk *ChunkStruct)
{    
    for(int i = 0; i < ID_WIDTH; i++)
    {
	uint8 *Letter = (uint8 *)(ChunkTypeStart + i);
	ChunkStruct->Type[i] = *Letter;
    }

    ChunkStruct->Type[ID_WIDTH] = '\0';
}

void
ReadChunkDataStart(uint64 ChunkDataStart, form_chunk *ChunkStruct)
{
    ChunkStruct->DataStart = ChunkDataStart;
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

#if 0
inline double 
FlipEndianness(uint8 *FirstByteOfExtended)
{
    uint8 DoubleToWriteByteArray[sizeof(double)] = {};
    uint64 DoubleToFlip = *((uint64 *)FirstByteOfExtended);

    if(*(FirstByteOfExtended + (EXTENDED_WIDTH - 1)) > 0)
    {
	return(0);
    }
    else if(*(FirstByteOfExtended + (EXTENDED_WIDTH - 2)) > 0)
    {
	return(0);
    }
    else
    {
	for(int i = 0; i < sizeof(double); i++)
	{
	    int Offset = i * BITS_IN_BYTE;
	    int ArrayIndex = (ArrayCount(DoubleToWriteByteArray) - 1 - i);

	    DoubleToWriteByteArray[ArrayIndex] = 
		((DoubleToFlip >> Offset) & 0xFF);
	}
    }

    double DoubleToWrite = 0;
    for(int i = 0; ArrayCount(DoubleToWriteByteArray); i++)
    {
	int ShiftAmount = sizeof(double) - (i * BITS_IN_BYTE);
	uint8 DoubleByte = DoubleToWriteByteArray[i]; 
	DoubleToWrite = 
	    DoubleToWrite | (double)(((uint64)DoubleByte) << ShiftAmount);

    }


    return((double) 1);
}
#endif

inline uint32
GetSampleRate(uint8 *FirstBitOfExtendedPrecisionFloat)
{
    uint8 RawBitArray[10];

    for(int i = 0; i < ArrayCount(RawBitArray); i++)
    {
	RawBitArray[i] = *(FirstBitOfExtendedPrecisionFloat + i);
    }

    uint16 Exponent = 0;
    Exponent |= RawBitArray[0] << 8;
    Exponent |= RawBitArray[1];
    Exponent &= 0x7FFF;
    
    int ExtendedPrecisionBiasAmount = -16383;
    Exponent += ExtendedPrecisionBiasAmount;

    uint64 FractionBits = 0;
    for(int i = 0; i < sizeof(uint64); i++)
    {
	int RawBitIndex = i + 2;
	int ShiftAmount = 56 - (i * 8);
	FractionBits |= ((uint64)RawBitArray[RawBitIndex] << ShiftAmount);
    }
    FractionBits &= 0x7FFFFFFFFFFFFFFF;
    FractionBits <<= 1;

    double Fraction = (double)FractionBits / (double)UINT64_MAX;
    Fraction += 1;
    return(Fraction * pow(2, Exponent));
}

int WinMain(HINSTANCE Instance, 
	HINSTANCE PrevInstance, 
	PSTR CmdLine, 
	int CmdShow)
{
    LPCWSTR Filename = L"SC88PR~1.AIF";
    uint8 *FileAddress = (uint8 *)Win32GetFilePointer(Filename);

    form_chunk FormChunk = {};

    uint64 FormChunkHeaderStart = (uint64)FileAddress;
    ReadChunkAddress(FormChunkHeaderStart, &FormChunk);

    uint8 *FormChunkIDStart = (uint8 *)FormChunk.HeaderStart;
    ReadChunkID(FormChunkIDStart, &FormChunk);

    int32 *FormChunkDataSizeAddress = (int32 *)(FormChunk.HeaderStart + ID_WIDTH);
    FormChunk.DataSize = FlipEndianness(*FormChunkDataSizeAddress);

    uint8 *FormChunkTypeStart = (uint8 *)(FormChunk.HeaderStart + ID_WIDTH + 
				    sizeof(FormChunk.DataSize));
    ReadChunkType(FormChunkTypeStart, &FormChunk);

    uint64 FormChunkDataStart = (uint64) (FormChunk.HeaderStart + ID_WIDTH + 
				    sizeof(FormChunk.DataSize) +
				    sizeof(ID_WIDTH));
    ReadChunkDataStart(FormChunkDataStart, &FormChunk);

    common_chunk CommonChunk = {};

    uint8 *FileIndex = (uint8 *)FormChunk.DataStart;
    uint64 CommonChunkHeaderStart = (uint64)FileIndex;
    ReadChunkAddress(CommonChunkHeaderStart, &CommonChunk);

    uint8 *CommonChunkIDStart = (uint8 *)CommonChunk.HeaderStart;
    ReadChunkID(CommonChunkIDStart, &CommonChunk);

    FileIndex += ID_WIDTH;
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

    return(0);
}
