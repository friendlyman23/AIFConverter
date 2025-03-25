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

#define Assert(Value) if(!(Value)) {*(int *)0 = 0;}

#define ID_WIDTH 4

void
PrintDebugString(int32 i)
{
    char buffer[256];
    sprintf_s(buffer, sizeof(buffer), "%d\n", i);
    OutputDebugStringA(buffer);
}

void *
Win32GetAifPointer(LPCWSTR Filename)
{
    LPVOID StarAif = 0;
    HANDLE AifHandle = CreateFileW(Filename, GENERIC_READ | GENERIC_WRITE, 
				    0, 0, OPEN_EXISTING, 
				    FILE_ATTRIBUTE_NORMAL, 0);
    if(AifHandle != INVALID_HANDLE_VALUE)
    {
	LARGE_INTEGER FileSize;
	GetFileSizeEx(AifHandle, &FileSize);
	if(FileSize.QuadPart)
	{
	    Assert(FileSize.QuadPart <= 0xFFFFFFFF);

	    HANDLE HeapHandle = GetProcessHeap();
	    if(HeapHandle)
	    {
		StarAif = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, FileSize.QuadPart);
		if(StarAif)
		{
		    DWORD NumBytesRead;
		    BOOL FileReadSuccessfully = false;
		    FileReadSuccessfully = ReadFile(AifHandle, StarAif, FileSize.QuadPart, &NumBytesRead, 0);
		    if(!FileReadSuccessfully)
		    {
			OutputDebugStringA("failed to read file\n");

			//free if we failed to read the file
			HeapFree(HeapHandle, 0, StarAif);
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

    return(StarAif);
}

struct chunk_struct
{

    uint8 *Address;
    char ID[ID_WIDTH + 1];
    int32 DataSize;
    char Type[ID_WIDTH + 1];
    uint8 *Data;
};

void
ReadChunkAddress(uint8 *ChunkStart, chunk_struct *ChunkStruct)
{
    ChunkStruct->Address = ChunkStart;
}

void
ReadChunkID(uint8 *ChunkStart, chunk_struct *ChunkStruct)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
	uint8 *Letter = (uint8 *)(ChunkStart + i);
	ChunkStruct->ID[i] = *Letter;
    }

    ChunkStruct->ID[ID_WIDTH] = '\0';
}


int32
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

    /*uint8 BE_LeastSignificant = *BEDataSize & 255;*/
    /*uint8 BE_SecondByte = (*BEDataSize >> 8) & 255;*/
    /*uint8 BE_ThirdByte = (*BEDataSize >> 16) & 255;*/
    /*uint8 BE_MostSignificant = (*BEDataSize >> 24) & 255;*/
    /**/
    /*FormChunk.DataSize = (int32)(((uint32) BE_LeastSignificant << 24) | ((uint32) BE_SecondByte << 16) | ((uint32) BE_ThirdByte << 8) | ((uint32) BE_MostSignificant));*/

}


int WinMain(HINSTANCE Instance, 
	HINSTANCE PrevInstance, 
	PSTR CmdLine, 
	int CmdShow)
{
    LPCWSTR Filename = L"SC88PR~1.aif";
    uint8 *StarAif = (uint8 *)Win32GetAifPointer(Filename);

    chunk_struct FormChunk = {};

    ReadChunkAddress(StarAif, &FormChunk);

    ReadChunkID(StarAif, &FormChunk);

    int32 *BEDataSize = (int32 *)(StarAif+ID_WIDTH);

    FormChunk.DataSize = FlipEndianness(*BEDataSize);








    



    return(0);
}
