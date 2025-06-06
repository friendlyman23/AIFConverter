// INCLUDED IN CONVERTER.H

#include <windows.h>
#include <stdio.h>
#include <string.h>
/*#include "converter.h"*/
/*#include "converter.cpp"*/
#include <stdint.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef enum {RIFF = 295, fmt = 359, smpl = 444, inst = 446, data = 410} Chunk;

#define ID_WIDTH 4
#define MAX_STRING_LEN 255 
#define Stringize(String) #String

#pragma pack(push, 1)
struct wav_riff_chunk
{
    char ID[4];          // 'RIFF'
    u32 ChunkSize;    // Size of the entire file minus 8 bytes
    char Format[4];      // 'WAVE'
};

struct wav_fmt_chunk
{
    char ID[4];            // 'fmt '
    u32 ChunkSize;      // Size of the rest of this chunk (typically 16 for PCM)
    u16 AudioFormat;    // PCM = 1, others for compression
    u16 NumChannels;    // Mono = 1, Stereo = 2, etc.
    u32 SampleRate;     // 44100, 48000, etc.
    u32 ByteRate;       // SampleRate * NumChannels * BitsPerSample / 8
    u16 BlockAlign;     // NumChannels * BitsPerSample / 8
    u16 BitsPerSample;  // 8, 16, 24, etc.
};

struct wav_data_chunk
{
    char ID[4];          // 'data'
    u32 ChunkSize;    // Number of bytes of sample data
    u8 Samples[];
};

struct wav_inst_chunk
{
    char ID[4];               // 'inst'
    u32 ChunkSize;         // Should be 7 bytes
    u8 UnshiftedNote;      // MIDI root note
    i8 FineTune;            // -50 to +50 cents
    i8 Gain;                // -64 to +64 dB
    u8 LowNote;            // Lowest note played
    u8 HighNote;           // Highest note played
    u8 LowVelocity;        // Lowest velocity played
    u8 HighVelocity;       // Highest velocity played
    u8 PadByte;
};

struct wav_sampler_chunk 
{
    char ID[ID_WIDTH];
    u32 ChunkSize;
    u32 Manufacturer;
    u32 Product;
    u32 SamplePeriod;
    u32 MidiUnityNote;
    u32 MidiPitchFraction;
    u32 SmpteFormat;
    u32 SmpteOffset;
    u32 NumSampleLoops;
    u32 SamplerData;
};

struct wav_sample_loop 
{
    u32 CuePointID;
    u32 Type;          
    u32 Start;         
    u32 End;           
    u32 Fraction;
    u32 PlayCount;     
};

struct arena
{
    u64 ArenaSize;
    u8 *DoNotCrossThisLine;
    u8 *NextFreeByte;
    u8 ArenaStart[];
};
#pragma pack(pop)

void *
Win32_AllocateMemory(u64 Size,  char *CallingFunction)
{
    HANDLE HeapHandle = GetProcessHeap();
    LPVOID Result;
    char *FailureReason;

    if(HeapHandle)
    {
	Result = HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, Size);
	if(Result)
	{
	    return(Result);
	}
	else
	{
	    char *Why = "because call to HeapAlloc failed.\n";
	    FailureReason = Why;
	}
    }
    else
    {
	char *Why = "because unable to get process heap handle.\n";
	FailureReason = Why;
    }
    char DebugPrintStringBuffer[MAX_STRING_LEN];
    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
	    "\nERROR:\n\t"
	    "In function"
	    "\n\t\t%s"
	    "\n\tmemory allocation failed %s",
	    CallingFunction, FailureReason);
    OutputDebugStringA((char *)DebugPrintStringBuffer);
    exit(1);
}

arena *
ArenaAlloc(u64 Size)
{
    arena *Arena = (arena *)Win32_AllocateMemory(Size, __func__);
    Arena->ArenaSize = Size;
    Arena->DoNotCrossThisLine = ( ((u8 *)Arena + Arena->ArenaSize) - 1 );
    Arena->NextFreeByte = Arena->ArenaStart;
    return(Arena);
}

void *
ArenaPush(arena *Arena, u64 Size)
{
    if( (Arena->NextFreeByte + Size) > Arena->DoNotCrossThisLine )
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
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

#define PushArray(Arena, Type, Count) (Type *)ArenaPush((Arena), (sizeof(Type) * (Count)))
#define PushStruct(Arena, Type) (Type *)PushArray((Arena), Type, 1)

// NOT INCLUDED IN CONVERTER.H

struct wav_file_metadata
{
    char *PathName;
    u8 *FileStart;
    union
    {
	LARGE_INTEGER LargeInteger;
	struct
	{
	    DWORD Size;
	    LONG HighPart;
	};

    };
    u8 *Index;
    HANDLE Handle;
};

char *
ValidateInputFileString(int argc, char **argv)
{
    if(argc != 2)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: Usage\n");	
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }

    char *Wav_FileName = argv[1];
    char *Dot = strrchr(Wav_FileName, '.');
    char *FileTypePtr = Dot + 1;
    char *ToMatchPtr = "wav";

    for(int i = 0; i < (strlen(ToMatchPtr)); i++)
    {
	if(tolower(*FileTypePtr) != *ToMatchPtr)
	{
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: File extension\n");	
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);
	}
	FileTypePtr++;
	ToMatchPtr++;
    }
    return(Wav_FileName);

}
void
Win32_ReadFile(wav_file_metadata *Wav_FileMetadata)
{
    
    Wav_FileMetadata->Handle = CreateFileA(Wav_FileMetadata->PathName, GENERIC_READ, 0, 0,
					    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(Wav_FileMetadata->Handle)
    {
	if(GetFileSizeEx(Wav_FileMetadata->Handle, &Wav_FileMetadata->LargeInteger))
	{
	    if(Wav_FileMetadata->HighPart)
	    {
		char DebugPrintStringBuffer[MAX_STRING_LEN];
		sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: .wav file is too large\n");	
		OutputDebugStringA((char *)DebugPrintStringBuffer);
		exit(1);
	    }
	    HANDLE HeapHandle = GetProcessHeap();
	    if(HeapHandle)
	    {
		Wav_FileMetadata->FileStart = (u8 *)HeapAlloc(HeapHandle, HEAP_ZERO_MEMORY, Wav_FileMetadata->Size);
		if(Wav_FileMetadata->FileStart)
		{
		    DWORD BytesRead = 0;
		    BOOL Result = ReadFile(Wav_FileMetadata->Handle, Wav_FileMetadata->FileStart, 
					    Wav_FileMetadata->Size, &BytesRead, 0);
		    if(!(Result && (Wav_FileMetadata->Size == BytesRead)))
		    {
			char DebugPrintStringBuffer[MAX_STRING_LEN];
			sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: Failed to read .wav into memory\n");	
			OutputDebugStringA((char *)DebugPrintStringBuffer);
			exit(1);
		    }
		}
		else
		{
		    char DebugPrintStringBuffer[MAX_STRING_LEN];
		    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: Failed to allocate memory for .wav file\n");	
		    OutputDebugStringA((char *)DebugPrintStringBuffer);
		    exit(1);
		}
	    }
	    else
	    {
		char DebugPrintStringBuffer[MAX_STRING_LEN];
		sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: Failed to get heap handle\n");	
		OutputDebugStringA((char *)DebugPrintStringBuffer);
		exit(1);
	    }
	}
	else
	{
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: Failed to get .wav file size\n");	
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);
	}
    }
    else
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: Failed to open file\n");	
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
}

Chunk
HashID(char *ID)
{
    // We only parse these five chunks for now. If the hash doesn't match one
    //	  of the five we parse, exit
    Chunk AllowedChunks[] = {RIFF, fmt, data, smpl, inst};
    int Hash = 0;
    char *Letter = ID;

    for(int i = 0; i < ID_WIDTH; i++)
    {
	Hash += ID[i];
    }
    
    for(int i = 0; i < (sizeof(AllowedChunks) / sizeof(AllowedChunks[0])); i++)
    {
	Chunk ThisChunk = (Chunk)Hash;
	if(ThisChunk == AllowedChunks[i])
	{
	    return(ThisChunk);
	}
    }

    char DebugPrintStringBuffer[MAX_STRING_LEN];
    char IDBuffer[ID_WIDTH + 1];
    for(int i = 0; i < ID_WIDTH; i++)
    {
	IDBuffer[i] = ID[i];
    }
    // stop:debug
    IDBuffer[ID_WIDTH + 1] = '\0';
    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"Error: Failed to identify ID for chunk \"%s\"\n", (char *)IDBuffer);	
    OutputDebugStringA((char *)DebugPrintStringBuffer);
    exit(1);
}

struct chunk_node
{
    Chunk HashedID;
    u8 *Wav_ChunkAddr; // Location of this chunk in the .wav file
    struct chunk_node *NextChunk; // next chunk in the list
};
// END OF INCLUDES

int main(int argc, char *argv[])
{

    char *Wav_FileName = ValidateInputFileString(argc, argv); 
    arena *WorkingMem = ArenaAlloc(1000000);
    wav_file_metadata *Wav_FileMetadata = PushStruct(WorkingMem, wav_file_metadata);
    Wav_FileMetadata->PathName = Wav_FileName;
    Win32_ReadFile(Wav_FileMetadata);
    u8 *Wav_Idx = Wav_FileMetadata->FileStart;
    wav_riff_chunk *Wav_RiffChunk = (wav_riff_chunk *)Wav_Idx;
    u8 *LastByteInFile = (Wav_FileMetadata->FileStart + (Wav_FileMetadata->Size - 1));

    // If this first chunk is not the RIFF chunk, the file is busted, so exit
    Chunk RiffID = HashID((char *)Wav_Idx);
    if(RiffID != RIFF)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
"Error: .wav file needs a RIFF chunk as first chunk.\n");	
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
    else
    {
	// point wav_idx to the start of the next chunk
	Wav_Idx += sizeof(wav_riff_chunk);
    }

    // Set up a linked list that represents the structure of the .wav file
    chunk_node *FirstChunk = PushStruct(WorkingMem, chunk_node);
    FirstChunk->HashedID = RiffID;
    FirstChunk->Wav_ChunkAddr = Wav_FileMetadata->FileStart;

    FirstChunk->NextChunk = PushStruct(WorkingMem, chunk_node);
    FirstChunk->NextChunk->NextChunk = 0;
    chunk_node *CurrentChunk = FirstChunk->NextChunk;

    while(Wav_Idx <= LastByteInFile)
    {
	Chunk HashedID = HashID((char *)Wav_Idx);
	CurrentChunk->HashedID = HashedID;
	CurrentChunk->Wav_ChunkAddr = Wav_Idx;
	CurrentChunk->NextChunk = PushStruct(WorkingMem, chunk_node);
	CurrentChunk = CurrentChunk->NextChunk;

	// Advance Wav_Idx to the next byte in the file
	u32 ThisChunksSize = *(u32 *)(Wav_Idx + ID_WIDTH);
	Wav_Idx += (ID_WIDTH + sizeof(u32) + ThisChunksSize);
    }




    





    










    /*u8 *Wav_Pointer = Wav_Buffer;*/

    







    return(0);
}
