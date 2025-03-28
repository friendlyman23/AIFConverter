#include <windows.h>
#include <stdint.h>
#include <stdio.h>

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
#define NUM_BITS_IN_BYTE 8



void
PrintDebugString(int32 i)
{
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "%d\n", i);
    OutputDebugStringA(buffer);
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
    double SampleRate;
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

inline double 
FlipEndianness(uint8 *FirstByteOfExtended)
{
    double DoubleToWrite = 0;
    uint8 ExtendedBytes[EXTENDED_WIDTH] = {};

    uint8 *Index = FirstByteOfExtended;
    for(int i = 0; i < EXTENDED_WIDTH; i++)
    {
	if((i >= 8) && (*Index != 0))
	{
	    return(DoubleToWrite);
	}
	ExtendedBytes[i] = *Index++;
    }

    return(DoubleToWrite);
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

	/*   struct common_chunk*/
	/*   {*/
	/*uint64 HeaderStart;*/
	/*char ID[ID_WIDTH + 1];*/
	/*int32 DataSize;*/
	/*int16 NumChannels;*/
	/*uint32 NumSampleFrames;*/
	/*int16 SampleSize;*/
	/*float SampleRate;*/
	/*   };*/

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
    uint8 *CommonChunkSampleRate = FileIndex;
    CommonChunk.SampleRate = FlipEndianness(CommonChunkSampleRate);

/*7	   6	      5		 4*/
/*98765432 10987654 32109876 54321098*/
/*01000000 00001110 10101100 01000100 00000000 00000000 00000000 00000000 00000000 00000000 00000000*/
/*SEEEEEEE EEEEIFFF FFFFFFFF FFFFFFFF*/


    return(0);
}
