// INCLUDED IN CONVERTER.H

#include <windows.h>
#include <stdio.h>
#include <string.h>
/*#include "converter.h"*/
/*#include "converter.cpp"*/
#include <stdint.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


#define ID_WIDTH 4
#define MAX_STRING_LEN 255 
#define Stringize(String) #String
#define BITS_IN_BYTE 8

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
    u32 ChunkSize;      // Size of the rest of this chunk_hash (typically 16 for PCM)
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
    s8 FineTune;            // -50 to +50 cents
    s8 Gain;                // -64 to +64 dB
    u8 LowNote;            // Lowest note played
    u8 HighNote;           // Highest note played
    u8 LowVelocity;        // Lowest velocity played
    u8 HighVelocity;       // Highest velocity played
    u8 PadByte;
};

struct wav_smpl_loop 
{
    u32 CuePointID;
    u32 Type;          
    u32 Start;         
    u32 End;           
    u32 Fraction;
    u32 PlayCount;     
};

struct wav_smpl_chunk 
{
    char ID[ID_WIDTH];
    u32 ChunkSize;
    u32 Manufacturer;
    u32 Product;
    u32 SmplPeriod;
    u32 MidiUnityNote;
    u32 MidiPitchFraction;
    u32 SmpteFormat;
    u32 SmpteOffset;
    u32 NumSmplLoops;
    u32 SamplerData;
    wav_smpl_loop SmplLoopsStart[]; // if there are no sample loops this 
				    //	  just points to the next chunk
};

struct aif_form_chunk
{
    char ID[ID_WIDTH];
    u32 ChunkSize;
    char FormType[ID_WIDTH];
    u8 SubChunksStart[];
};

struct aif_common_chunk
{
    char ID[ID_WIDTH];
    u32 ChunkSize;
    u16 NumChannels;
    u32 NumSampleFrames;
    u16 SampleSize;
    u16 SampleRateExponent;
    u64 SampleRateFraction;
};

struct arena
{
    u64 ArenaSize;
    u8 *DoNotCrossThisLine;
    u8 *NextFreeByte;
    u64 SpaceLeft;
    u8 ArenaStart[];
};
#pragma pack(pop)

void *
Win32_AllocateMemory(u64 Size, char *CallingFunction)
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
    Arena->DoNotCrossThisLine = ((u8 *)Arena + Size - 1);
    Arena->NextFreeByte = Arena->ArenaStart;
    Arena->SpaceLeft = ((Arena->DoNotCrossThisLine - Arena->NextFreeByte) + 1);
    return(Arena);
}

void *
ArenaPush(arena *Arena, u64 Size)
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
	Arena->SpaceLeft = ((Arena->DoNotCrossThisLine - Arena->NextFreeByte) + 1);
	return(TheBytesMyLordHathRequested);
    }
}

inline void
CopyIDToAif(char *StartOfIDToRead, char *ID_Destination)
{
    for(int i = 0; i < ID_WIDTH; i++)
    {
        char *Letter = (char *)(StartOfIDToRead + i);
        ID_Destination[i] = *Letter;
    }
}

inline u32
FlipEndianness32(u32 BytesToFlip)
{
    u32 FlippedBytes = (u32)((((BytesToFlip >>  24) & 0xFF) <<  0) |
			      (((BytesToFlip >> 16) & 0xFF) <<  8) |
			      (((BytesToFlip >>  8) & 0xFF) << 16) |
			      (((BytesToFlip >>  0) & 0xFF) << 24) );
    return(FlippedBytes);
}

#define PushArray(Arena, Type, Count) (Type *)ArenaPush((Arena), (sizeof(Type) * (Count)))
#define PushStruct(Arena, Type) (Type *)PushArray((Arena), Type, 1)

// NOT INCLUDED IN CONVERTER.H

#define NULLPTR (int *)0
#define NULLCHAR '\0'
arena *WorkingMem = ArenaAlloc(1000000);
#if DEBUG
char **UnhashedIDArray = PushArray(WorkingMem, char *, 100);
int UnhashedIdArrayIdx = 0;
#endif 

enum chunk_hash
{
    RIFF = 295, 
    fmt = 359, 
    smpl = 444, 
    inst = 446, 
    data = 410, 
    FORM = 308, 
    COMM = 300, 
    SSND = 312, 
    MARK = 299, 
    INST = 318
};

// differences between this and the aif_sound_data_chunk in converter.h are:
//    struct tag (ssnd vs sound_data)
//    this one has a flexible array member instead of a samples start byte
#pragma pack(push, 1)
struct aif_ssnd_chunk 
{
    char ID[ID_WIDTH];
    u32 ChunkSize;
    u32 Offset;
    u32 BlockSize;
    u8 Samples[];
};

struct aif_loop
{
    s16 PlayMode;
    s16 BeginLoopMarker;
    s16 EndLoopMarker;
};

struct aif_inst_chunk
{
    char ID[ID_WIDTH];
    u32 ChunkSize;
    s8 BaseNote;
    s8 Detune;
    s8 LowNote;
    s8 HighNote;
    s8 LowVelocity;
    s8 HighVelocity;
    s16 Gain;
    aif_loop SustainLoop;
    aif_loop ReleaseLoop;
};

struct aif_marker
{
    u16 ID;
    u32 Position;
    u8 MarkerNameLen;
    u8 PadByte;
};

// How this differs from the declaration in converter.h:
//    MarkersStart is a flexible array member instead of
//    an empty byte
struct aif_marker_chunk
{
    char ID[ID_WIDTH];
    u32 ChunkSize;
    u16 TotalMarkers;
    aif_marker MarkersStart[];
};

#pragma pack(pop)

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
    u8 *LastByteInFile;
};

char *
ValidateInputFileString(int argc, char **argv)
{
    if(argc != 2)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), "Error: Usage\n");	
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }

    char *Wav_Filename = argv[1];
    char *Dot = strrchr(Wav_Filename, '.');
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
    return(Wav_Filename);

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
		"Error: Failed to open .wav file\n");	
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
}

enum chunk_hash
HashID(char *ID)
{
    // We only parse these five chunks for now. If the hash doesn't match one
    //	  of the five we parse, exit
    enum chunk_hash AllowedChunks[] = {RIFF, fmt, data, smpl, inst,
						FORM, COMM, SSND, MARK, INST};
    int Hash = 0;

    for(int i = 0; i < ID_WIDTH; i++)
    {
	Hash += ID[i];
    }
    
    for(int i = 0; i < (sizeof(AllowedChunks) / sizeof(AllowedChunks[0])); i++)
    {
	enum chunk_hash ThisChunk = (enum chunk_hash)Hash;
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
    IDBuffer[ID_WIDTH + 1] = NULLCHAR;
    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"Error: Failed to identify ID for chunk_hash: \"%s\"\n", (char *)IDBuffer);	
    OutputDebugStringA((char *)DebugPrintStringBuffer);
    exit(1);
}

struct chunk_node
{
    enum chunk_hash HashedID;
    void *ChunkAddr; 
    struct chunk_node *NextChunk;
};

static chunk_node *Wav_ChunkDirectory = (chunk_node *)NULLPTR;
static chunk_node *Aif_ChunkDirectory = (chunk_node *)NULLPTR;

u16
FlipEndianness16(u16 BytesToFlip)
{
    return( (((BytesToFlip >> 0) & 0xFF) << 8)  | 
	    (((BytesToFlip >> 8) & 0xFF) << 0) );
}

void
Write10ByteSampleRate(aif_common_chunk *Aif_CommonChunk, u32 Wav_SampleRate)
{
    // Convert the .wav file's sample rate into the 10-byte extended precision 
    //	  floating point number (hereafter "Extended") used by .aif files to 
    //	  encode sample rate.
    //
    //	  It's helpful to think of this like scientific notation, but in base 2. 
    //	  We need to express the .wav file's sample rate, an integer, as the 
    //	  product of: a real number between 1 and 2, and a power of 2. 
    //
    //	  e.g., for the common sample rate of 44,100:
    //
    //	      2^15 x 1.3458251953125 = 44,100

    // Get the exponent value 
    u16 Exponent = 0;
    u32 Temp = Wav_SampleRate;
    while(Temp > 1)
    {
	Temp /= 2;
	Exponent++;
    }

    // The Extended format biases the exponent field by 16,383
    u16 ExponentBias = 16383;
    Aif_CommonChunk->SampleRateExponent = Exponent + ExponentBias;
    
    // .aif files are Big Endian, so flip the exponent bytes
    Aif_CommonChunk->SampleRateExponent = FlipEndianness16(Aif_CommonChunk->SampleRateExponent);
    
    // Now compute the fractional part of the number 
    u64 Wav_U64SampleRate = (u64)Wav_SampleRate;

    // Flip the endianness of the fractional part
    u64 Wav_U64SampleRateFlipped = (  (((Wav_U64SampleRate >>  0) & 0xFF) << 56) |
				      (((Wav_U64SampleRate >>  8) & 0xFF) << 48) |
				      (((Wav_U64SampleRate >> 16) & 0xFF) << 40) |
				      (((Wav_U64SampleRate >> 24) & 0xFF) << 32) |
				      (((Wav_U64SampleRate >> 32) & 0xFF) << 24) |
				      (((Wav_U64SampleRate >> 40) & 0xFF) << 16) |
				      (((Wav_U64SampleRate >> 48) & 0xFF) <<  8) |
				      (((Wav_U64SampleRate >> 56) & 0xFF) <<  0) );

    //	Shift the fractional part until it is a real number between 1 and 2  
    u64 Wav_U64SampleRateFlippedAndShifted = (Wav_U64SampleRateFlipped >> (63 - Exponent));

    // Store the number in the .aif file buffer
    Aif_CommonChunk->SampleRateFraction = Wav_U64SampleRateFlippedAndShifted;
}

void *
Wav_GetChunkPointer(enum chunk_hash Wav_ChunkHash)
{
    chunk_node *CurrentChunk = Wav_ChunkDirectory;

    while((CurrentChunk->HashedID != Wav_ChunkHash) && (CurrentChunk != (chunk_node *)NULLPTR))
    {
	CurrentChunk = CurrentChunk->NextChunk;
    }
    if(CurrentChunk == (chunk_node *)NULLPTR)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"Error: Chunk lookup failed for HashedID %d\n", (int)Wav_ChunkHash);
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
    else
    {
	return(CurrentChunk->ChunkAddr);
    }
}

void
Aif_AddToDirectory(char *ID, void *ChunkAddr)
{
    chunk_node *Current = Aif_ChunkDirectory;
    while(Current->NextChunk)
    {
	Current = Current->NextChunk;
    }
    chunk_node *NewNode = PushStruct(WorkingMem, chunk_node);
    Current->NextChunk = NewNode; 
    NewNode->NextChunk = (chunk_node *)NULLPTR;
    NewNode->HashedID = HashID(ID);
    NewNode->ChunkAddr = ChunkAddr;
}

void *
Aif_GetChunkPointer(enum chunk_hash Aif_ChunkHash) 
{
    chunk_node *CurrentChunk = Aif_ChunkDirectory;
    while((CurrentChunk->HashedID != Aif_ChunkHash) && (CurrentChunk->NextChunk != (chunk_node *)NULLPTR))
    {
	CurrentChunk = CurrentChunk->NextChunk;
    }
    if(CurrentChunk == (chunk_node *)NULLPTR)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"Error: Chunk lookup failed for HashedID %d\n", (int)Aif_ChunkHash);
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
    return((void *)CurrentChunk->ChunkAddr);
}
/**/
/*void*/
/*Debug_PushNullTerminatedID(char *ID)*/
/*{*/
/*    char *IDBuffer = PushArray(WorkingMem, char, (ID_WIDTH + 1));*/
/*    for(int i = 0; i < ID_WIDTH; i++)*/
/*    {*/
/*	IDBuffer[i] = ID[i];*/
/*    }*/
/*    IDBuffer[ID_WIDTH + 1] = NULLCHAR;*/
/*    UnhashedIDArray[UnhashedIdArrayIdx++] = IDBuffer;*/
/*}*/

#define Assert(expression) if(!(expression)) {*(int *)0 = 0;}
// END OF INCLUDES

int main(int argc, char *argv[])
{
    // TODO: Check that input filepath isn't longer than 255 or some other reasonable limit
    char *Wav_Filename = ValidateInputFileString(argc, argv); 
    wav_file_metadata *Wav_FileMetadata = PushStruct(WorkingMem, wav_file_metadata);
    Wav_FileMetadata->PathName = Wav_Filename;
    Win32_ReadFile(Wav_FileMetadata);
    u8 *Wav_Idx = Wav_FileMetadata->FileStart;
    wav_riff_chunk *Wav_RiffChunk = (wav_riff_chunk *)Wav_Idx;
    Wav_FileMetadata->LastByteInFile = ((u8 *)Wav_FileMetadata->FileStart + Wav_FileMetadata->Size - 1);

    // If this first chunk_hash is not the RIFF chunk_hash, the file is busted, so exit
    enum chunk_hash RiffID = HashID((char *)Wav_Idx);
    if(RiffID != RIFF)
    {
	char DebugPrintStringBuffer[MAX_STRING_LEN];
	sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"Error: .wav file needs a RIFF chunk_hash as first chunk_hash.\n");	
	OutputDebugStringA((char *)DebugPrintStringBuffer);
	exit(1);
    }
    else
    {
	// point wav_idx to the start of the next chunk_hash
	Wav_Idx += sizeof(wav_riff_chunk);
    }

    // Set up a linked list that represents the structure of the .wav file
    Wav_ChunkDirectory = PushStruct(WorkingMem, chunk_node);
    Wav_ChunkDirectory->HashedID = RiffID;
    Wav_ChunkDirectory->ChunkAddr = (void *)Wav_FileMetadata->FileStart;
    Wav_ChunkDirectory->NextChunk = (chunk_node *)NULLPTR;

    while(Wav_Idx < Wav_FileMetadata->LastByteInFile)
    {
	chunk_node *CurrentChunk = Wav_ChunkDirectory;
	while(CurrentChunk->NextChunk)
	{
	    CurrentChunk = CurrentChunk->NextChunk;
	}

	CurrentChunk->NextChunk = PushStruct(WorkingMem, chunk_node);
	CurrentChunk = CurrentChunk->NextChunk;
	CurrentChunk->NextChunk = (chunk_node *)NULLPTR;

	enum chunk_hash HashedID = HashID((char *)Wav_Idx);
	CurrentChunk->HashedID = HashedID;
	CurrentChunk->ChunkAddr = (void *)Wav_Idx;

	// Advance Wav_Idx to the next byte in the file
	u32 ThisChunksSize = *(u32 *)(Wav_Idx + ID_WIDTH);
	Wav_Idx += (ID_WIDTH + sizeof(u32) + ThisChunksSize);
	Wav_Idx += (ThisChunksSize % 2); // chunks with odd sizes have a pad byte
    }

    // ValidateFile();

    // Get number of bytes for .aif file
    
    DWORD BytesNeededForAif = 0;
    BytesNeededForAif += sizeof(aif_form_chunk);
    BytesNeededForAif += sizeof(aif_common_chunk);
    BytesNeededForAif += sizeof(aif_ssnd_chunk);

    for(chunk_node *ThisChunk = Wav_ChunkDirectory; 
	    ThisChunk != (chunk_node *)NULLPTR; 
	    ThisChunk = ThisChunk->NextChunk)
    {
	switch(ThisChunk->HashedID)
	{
	    case smpl:
	    {
		wav_smpl_chunk *Wav_SmplChunk = (wav_smpl_chunk *)ThisChunk->ChunkAddr;
		if(Wav_SmplChunk->NumSmplLoops)
		{
		    BytesNeededForAif += sizeof(aif_marker_chunk);
		    BytesNeededForAif += ((sizeof(aif_marker) * Wav_SmplChunk->NumSmplLoops) * 2);
		} 
	    } break;
    
	    case inst:
	    {
		BytesNeededForAif += sizeof(aif_inst_chunk);
	    } break;

	    case data:
	    {
		wav_data_chunk *Wav_DataChunk = (wav_data_chunk *)ThisChunk->ChunkAddr;
		BytesNeededForAif += Wav_DataChunk->ChunkSize;
	    } break;

	    case RIFF:
	    case fmt:
	    {
		continue;
	    } break;

	    default:
	    {
		char DebugPrintStringBuffer[MAX_STRING_LEN];
		sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
			"Error: Encountered unexepected chunk_hash when calculating"
			"bytes needed for .aif file\n");	
		OutputDebugStringA((char *)DebugPrintStringBuffer);
		exit(1);
	    } break;
	}
    }

    // Allocate bytes for the converted .aif file
    arena *Aif_Buffer = ArenaAlloc(BytesNeededForAif + offsetof(arena, ArenaStart));

    // Declare a linked list to keep track of the structure of the .aif file
    Aif_ChunkDirectory = PushStruct(WorkingMem, chunk_node);
    Aif_ChunkDirectory->HashedID = HashID((char *)"FORM");
    Aif_ChunkDirectory->ChunkAddr = (void *)Aif_Buffer;
    Aif_ChunkDirectory->NextChunk = (chunk_node *)NULLPTR;

    // .aif FORM Chunk
    aif_form_chunk *Aif_FormChunk = PushStruct(Aif_Buffer, aif_form_chunk);
    Aif_AddToDirectory("FORM", Aif_FormChunk);
    CopyIDToAif("FORM", Aif_FormChunk->ID);
    u32 Aif_FormChunkSizeLittleEndian = (BytesNeededForAif - offsetof(aif_form_chunk, FormType));
    Aif_FormChunk->ChunkSize = FlipEndianness32(Aif_FormChunkSizeLittleEndian);
    CopyIDToAif("AIFF", Aif_FormChunk->FormType);

    // .aif COMM Chunk 
    aif_common_chunk *Aif_CommonChunk = PushStruct(Aif_Buffer, aif_common_chunk);
    Aif_AddToDirectory("COMM", Aif_FormChunk);
    CopyIDToAif("COMM", Aif_CommonChunk->ID);
    wav_fmt_chunk *Wav_FormatChunk = (wav_fmt_chunk *)Wav_GetChunkPointer(fmt);
    Aif_CommonChunk->ChunkSize = FlipEndianness32((u32)18); // always 18
    Aif_CommonChunk->NumChannels = FlipEndianness16((u16)Wav_FormatChunk->NumChannels); 
    wav_data_chunk *Wav_DataChunk = (wav_data_chunk *)Wav_GetChunkPointer(data);
    u32 Wav_NumSampleFramesLittleEndian = (u32)(Wav_DataChunk->ChunkSize / Wav_FormatChunk->BlockAlign);
    Aif_CommonChunk->NumSampleFrames = FlipEndianness32(Wav_NumSampleFramesLittleEndian);
    Aif_CommonChunk->SampleSize = FlipEndianness16(Wav_FormatChunk->BitsPerSample);
    Write10ByteSampleRate(Aif_CommonChunk, Wav_FormatChunk->SampleRate);

    // MARK 
    // TODO: Lots of ways to make this more robust:
    //	  - Look for Cue chunk_hash and see if the .wav file encodes names for its
    //	      SmplLoops. For now, we don't look for these and just write the .aif
    //	      MarkerNames as empty strings
    //
    //	  - Have logic that tries to determine what type of loop (sustain or release)
    //	      is represented by any .wav SmplLoop fields we find. For now, we assume the 
    //	      first SmplLoop structure refers to a sustain loop, any second loop refers
    //	      to a release loop, and if we find more than that, we abort
    //
    //	  - Test that we're reading the data we expect when we read the SmplLoop
    //	      fields: since the first field of each SmplLoop is supposed
    //	      to be a unique 32-bit int to distinguish it from other loops,
    //	      confirm we don't read any repeats; confirm that when we cast the
    //	      ID of the SmplLoop in the .wav file from a 32-bit int to its equivalent
    //	      field in the .aif file, a 16-bit int, we don't lose data, etc.
    //
    wav_smpl_chunk *Wav_SmplChunk = (wav_smpl_chunk *)Wav_GetChunkPointer(smpl);
    if(Wav_SmplChunk->NumSmplLoops)
    {
	if(Wav_SmplChunk->NumSmplLoops > 2)
	{
	    char DebugPrintStringBuffer[MAX_STRING_LEN];
	    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		    "Error: found more than 2 SmplLoops in .wav file.\n"
		    "This program only supports files containing 0, 1 or 2 SmplLoops.\n"
		    "This program will now exit.\n");	
	    OutputDebugStringA((char *)DebugPrintStringBuffer);
	    exit(1);
	}

	// Push the Marker chunk to the .aif buffer and fill in some values known
	//    at compile time
	aif_marker_chunk *Aif_MarkerChunk = PushStruct(Aif_Buffer, aif_marker_chunk);
	Aif_AddToDirectory("MARK", Aif_MarkerChunk);
	CopyIDToAif("MARK", Aif_MarkerChunk->ID);
	u32 Aif_MarkerChunkSize = (sizeof(aif_marker_chunk) + 
				    (u32)(sizeof(aif_marker) * Wav_SmplChunk->NumSmplLoops));
	Aif_MarkerChunk->ChunkSize = FlipEndianness32(Aif_MarkerChunkSize);
	Aif_MarkerChunk->TotalMarkers = FlipEndianness16((u16)Wav_SmplChunk->NumSmplLoops * 2);

	// Write data encoding looping parameters found in the .wav file to the
	//    .aif buffer
	u16 Aif_MarkerIDCounter = 1;
	for(u16 i = 0; i < Wav_SmplChunk->NumSmplLoops; i++)
	{
	    wav_smpl_loop Wav_ThisLoop = Wav_SmplChunk->SmplLoopsStart[i];
	    aif_marker *Aif_TwoMarkers = PushArray(Aif_Buffer, aif_marker, 2);

	    Aif_TwoMarkers[0].ID = FlipEndianness16(Aif_MarkerIDCounter);
	    Aif_MarkerIDCounter++;
	    Aif_TwoMarkers[0].Position = FlipEndianness32(Wav_ThisLoop.Start);
	    Aif_TwoMarkers[0].MarkerNameLen = 0;
	    Aif_TwoMarkers[0].PadByte = 0;

	    Aif_TwoMarkers[1].ID = FlipEndianness16(Aif_MarkerIDCounter);
	    Aif_MarkerIDCounter++;
	    Aif_TwoMarkers[1].Position = FlipEndianness32(Wav_ThisLoop.End);
	    Aif_TwoMarkers[1].MarkerNameLen = 0;
	    Aif_TwoMarkers[1].PadByte = 0;
	}
    }

    // INST Chunk
    //	  Todo: check for invalid wav loop types (there are only two valid types)
    //	  Todo: unify approach to getting wav chunks. here we cache, above we
    //	      access via pointer
    //
    aif_inst_chunk *Aif_InstChunk = PushStruct(Aif_Buffer, aif_inst_chunk);
    Aif_AddToDirectory("INST", Aif_InstChunk);
    CopyIDToAif("INST", Aif_InstChunk->ID);
    wav_inst_chunk Wav_InstChunk = *(wav_inst_chunk *)Wav_GetChunkPointer(inst);

    Aif_InstChunk->ChunkSize = (sizeof(aif_inst_chunk) - offsetof(aif_inst_chunk, BaseNote));
    Aif_InstChunk->BaseNote = Wav_InstChunk.UnshiftedNote;
    Aif_InstChunk->Detune = Wav_InstChunk.FineTune;
    Aif_InstChunk->LowNote = Wav_InstChunk.LowNote;
    Aif_InstChunk->HighNote = Wav_InstChunk.HighNote;
    Aif_InstChunk->LowVelocity = Wav_InstChunk.LowVelocity;
    Aif_InstChunk->HighVelocity = Wav_InstChunk.HighVelocity;

    Aif_InstChunk->Gain = FlipEndianness16((s16)Wav_InstChunk.Gain);
    enum aif_loop_type {AIF_NO_LOOP = 0, AIF_FWD_LOOP = 1, AIF_FWD_BACK_LOOP = 2};
    enum wav_loop_type {WAV_FWD_LOOP = 0, WAV_FWD_BACK_LOOP = 1};
    if(Wav_SmplChunk->NumSmplLoops)
    {
	// if we're here then there's at least a SustainLoop
	u16 Aif_MarkerIDCounter = 1;
	wav_smpl_loop Wav_ThisLoop = Wav_SmplChunk->SmplLoopsStart[0];
	if(Wav_ThisLoop.Type == WAV_FWD_LOOP)
	{
	    Aif_InstChunk->SustainLoop.PlayMode = FlipEndianness16((u16)AIF_FWD_LOOP);
	}
	else
	{
	    Aif_InstChunk->SustainLoop.PlayMode = FlipEndianness16((u16)AIF_FWD_BACK_LOOP);
	}
	Aif_InstChunk->SustainLoop.BeginLoopMarker = FlipEndianness16(Aif_MarkerIDCounter);
	Aif_MarkerIDCounter++;
	Aif_InstChunk->SustainLoop.EndLoopMarker = FlipEndianness16(Aif_MarkerIDCounter);
	Aif_MarkerIDCounter++;
	
	// if there's more than one SmplLoop in the .wav file
	if(Wav_SmplChunk->NumSmplLoops > 1)
	{
	    Wav_ThisLoop = Wav_SmplChunk->SmplLoopsStart[1];
	    if(Wav_ThisLoop.Type == WAV_FWD_LOOP)
	    {
		Aif_InstChunk->SustainLoop.PlayMode = AIF_FWD_LOOP;
	    }
	    else
	    {
		Aif_InstChunk->SustainLoop.PlayMode = AIF_FWD_BACK_LOOP;
	    }
	    Aif_InstChunk->ReleaseLoop.BeginLoopMarker = FlipEndianness16(Aif_MarkerIDCounter);
	    Aif_MarkerIDCounter++;
	    Aif_InstChunk->ReleaseLoop.EndLoopMarker = FlipEndianness16(Aif_MarkerIDCounter);
	}
    }

    // SSND chunk
    aif_ssnd_chunk *Aif_SSNDChunk = PushStruct(Aif_Buffer, aif_ssnd_chunk);
    
    int SamplesToCopy = Wav_DataChunk->ChunkSize;
    Assert(Aif_Buffer->SpaceLeft == Wav_DataChunk->ChunkSize);
    for(int Idx = 0; Idx < SamplesToCopy; Idx += 3)
    {
	Aif_SSNDChunk->Samples[Idx + 2] = Wav_DataChunk->Samples[Idx];
    }
    for(int Idx = 1; Idx < SamplesToCopy; Idx += 3)
    {
	Aif_SSNDChunk->Samples[Idx] = Wav_DataChunk->Samples[Idx];
    }
    for(int Idx = 2; Idx < SamplesToCopy; Idx += 3)
    {
	Aif_SSNDChunk->Samples[Idx] = Wav_DataChunk->Samples[Idx + 2];
    }

    // Write the .aif to disk
    // Get the length of the Aif filename
    char *Temp = Wav_Filename;
    int FilenameLen = 0;
    while(*Temp)
    {
	FilenameLen += 1;
	Temp++;
    }
    char *Aif_Filename = PushArray(WorkingMem, char, FilenameLen + 1);
    int Aif_FilenameIdx = 0;
    Temp = Wav_Filename;
    while(*Temp != '.')
    {
	Aif_Filename[Aif_FilenameIdx] = *Temp;
	Aif_FilenameIdx++;
	Temp++;
    }
    Aif_Filename[Aif_FilenameIdx++] = '.';
    Aif_Filename[Aif_FilenameIdx++] = 'a';
    Aif_Filename[Aif_FilenameIdx++] = 'i';
    Aif_Filename[Aif_FilenameIdx++] = 'f';
    Aif_Filename[Aif_FilenameIdx] = NULLCHAR;
	/*   HANDLE Aif_FileHandle = CreateFileA((LPCSTR)Aif_Filename, GENERIC_WRITE, 0, 0, */
	/*				CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);*/
	/*   if(Aif_FileHandle != INVALID_HANDLE_VALUE)*/
	/*   {*/
	/*DWORD BytesWritten = 0;*/
	/*// stop: the written file is corrupted*/
	/*BOOL Aif_WriteResult = WriteFile(Aif_FileHandle, Aif_Buffer->ArenaStart, */
	/*				    BytesNeededForAif, &BytesWritten, 0);*/
	/*Assert(Aif_WriteResult && (BytesNeededForAif == BytesWritten));*/
	/*   }*/
	/**/
	/*   //	  */
	/**/


    return(0);
}
