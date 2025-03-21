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

#define id char

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

struct form_chunk
{
    char ID[ID_WIDTH + 1];
    int32 DataSize;
    char Type[ID_WIDTH + 1];
    void *Data;
};


int WinMain(HINSTANCE Instance, 
	HINSTANCE PrevInstance, 
	PSTR CmdLine, 
	int CmdShow)
{
    LPCWSTR Filename = L"SC88PR~1.aif";
    uint8 *StarAif = (uint8 *)Win32GetAifPointer(Filename);

    form_chunk FormChunk;

    for(int i = 0; i < ID_WIDTH; i++)
    {
	unsigned char *Byte = (unsigned char  *)(StarAif + i);
	FormChunk.ID[i] = *Byte;
    }
    FormChunk.ID[ID_WIDTH] = '\0';

    int32 *BEDataSize = (int32 *)(StarAif+ID_WIDTH);

    uint8 BE_LeastSignificant = *BEDataSize & 255;
    uint8 BE_SecondByte = (*BEDataSize >> 8) & 255;
    uint8 BE_ThirdByte = (*BEDataSize >> 16) & 255;
    uint8 BE_MostSignificant = (*BEDataSize >> 24) & 255;

    FormChunk.DataSize = (int32)(((uint32) BE_LeastSignificant << 24) | ((uint32) BE_SecondByte << 16) | ((uint32) BE_ThirdByte << 8) | ((uint32) BE_MostSignificant));





    



    return(0);
}
