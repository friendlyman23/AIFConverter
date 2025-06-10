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

enum chunk {RIFF = 295, fmt = 359, smpl = 444, inst = 446, data = 410};

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
    s8 FineTune;            // -50 to +50 cents
    s8 Gain;                // -64 to +64 dB
    u8 LowNote;            // Lowest note played
    u8 HighNote;           // Highest note played
    u8 LowVelocity;        // Lowest velocity played
    u8 HighVelocity;       // Highest velocity played
    u8 PadByte;
};

struct wav_smpl_chunk 
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

struct wav_smpl_loop 
{
    u32 CuePointID;
    u32 Type;          
    u32 Start;         
    u32 End;           
    u32 Fraction;
    u32 PlayCount;     
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

struct aif_marker
{
    u16 ID;
    u32 Position;
    u8 MarkerNameLen;
    u8 MarkerNameStart;
};

struct aif_marker_chunk
{
    char ID[ID_WIDTH];
    s32 ChunkSize;
    u16 TotalMarkers;
    u8 MarkersStart[];
};

struct aif_loop
{
    s16 PlayMode;
    s16 BeginLoopMarker;
    s16 EndLoopMarker;
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
FlipEndiannessU32(u32 BytesToFlip)
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
static arena *WorkingMem = ArenaAlloc(1000000);
#if DEBUG
static char **UnhashedIDArray = PushArray(WorkingMem, char *, 100);
static int UnhashedIdArrayIdx = 0;
#endif 

// differences between this and the aif_sound_data_chunk in converter.h are:
//    struct tag (ssnd vs sound_data)
//    this one has a flexible array member instead of a samples start byte
#pragma pack(push, 1)
struct aif_ssnd_chunk 
{
    char ID[ID_WIDTH];
    s32 ChunkSize;
    u32 Offset;
    u32 BlockSize;
    u8 Samples[];
};

struct aif_inst_chunk
{
    char ID[ID_WIDTH];
    s32 ChunkSize;
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

enum chunk
HashID(char *ID)
{
    // We only parse these five chunks for now. If the hash doesn't match one
    //	  of the five we parse, exit
    enum chunk AllowedChunks[] = {RIFF, fmt, data, smpl, inst};
    int Hash = 0;
    char *Letter = ID;

    for(int i = 0; i < ID_WIDTH; i++)
    {
	Hash += ID[i];
    }
    
    for(int i = 0; i < (sizeof(AllowedChunks) / sizeof(AllowedChunks[0])); i++)
    {
	enum chunk ThisChunk = (enum chunk)Hash;
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
    IDBuffer[ID_WIDTH + 1] = '\0';
    sprintf_s(DebugPrintStringBuffer, sizeof(DebugPrintStringBuffer), 
		"Error: Failed to identify ID for chunk \"%s\"\n", (char *)IDBuffer);	
    OutputDebugStringA((char *)DebugPrintStringBuffer);
    exit(1);
}

struct chunk_node
{
    enum chunk HashedID;
    u8 *Wav_ChunkAddr; // Location of this chunk in the .wav file
    struct chunk_node *NextChunk; // next chunk in the list
};

static chunk_node *ChunkDirectory = (chunk_node *)NULLPTR;

u16
FlipEndiannessU16(u16 BytesToFlip)
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
    Aif_CommonChunk->SampleRateExponent = FlipEndiannessU16(Aif_CommonChunk->SampleRateExponent);
    
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

inline void *
GetWavChunkPointer(enum chunk Chunk)
{
    chunk_node *CurrentChunk = ChunkDirectory;

    while(CurrentChunk->HashedID != Chunk)
    {
	CurrentChunk = CurrentChunk->NextChunk;
    }

    return((void *)CurrentChunk->Wav_ChunkAddr);
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
/*    IDBuffer[ID_WIDTH + 1] = '\0';*/
/*    UnhashedIDArray[UnhashedIdArrayIdx++] = IDBuffer;*/
/*}*/
// END OF INCLUDES


int main(int argc, char *argv[])
{
    char *Wav_FileName = ValidateInputFileString(argc, argv); 
    wav_file_metadata *Wav_FileMetadata = PushStruct(WorkingMem, wav_file_metadata);
    Wav_FileMetadata->PathName = Wav_FileName;
    Win32_ReadFile(Wav_FileMetadata);
    u8 *Wav_Idx = Wav_FileMetadata->FileStart;
    wav_riff_chunk *Wav_RiffChunk = (wav_riff_chunk *)Wav_Idx;
    u8 *LastByteInFile = (Wav_FileMetadata->FileStart + (Wav_FileMetadata->Size - 1));

    // If this first chunk is not the RIFF chunk, the file is busted, so exit
    enum chunk RiffID = HashID((char *)Wav_Idx);
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
    ChunkDirectory = PushStruct(WorkingMem, chunk_node);
    ChunkDirectory->HashedID = RiffID;
    ChunkDirectory->Wav_ChunkAddr = Wav_FileMetadata->FileStart;
    ChunkDirectory->NextChunk = (chunk_node *)NULLPTR;

    while(Wav_Idx < LastByteInFile)
    {
	chunk_node *CurrentChunk = ChunkDirectory;
	while(CurrentChunk->NextChunk)
	{
	    CurrentChunk = CurrentChunk->NextChunk;
	}

	CurrentChunk->NextChunk = PushStruct(WorkingMem, chunk_node);
	CurrentChunk = CurrentChunk->NextChunk;
	CurrentChunk->NextChunk = (chunk_node *)NULLPTR;

	enum chunk HashedID = HashID((char *)Wav_Idx);
	CurrentChunk->HashedID = HashedID;
	CurrentChunk->Wav_ChunkAddr = Wav_Idx;

	// Advance Wav_Idx to the next byte in the file
	u32 ThisChunksSize = *(u32 *)(Wav_Idx + ID_WIDTH);
	Wav_Idx += (ID_WIDTH + sizeof(u32) + ThisChunksSize);
	Wav_Idx += (ThisChunksSize % 2); // chunks with odd sizes have a pad byte
    }

    // ValidateFile();

    // Get number of bytes for .aif file
    
    u32 BytesNeededForAif = 0;
    BytesNeededForAif += sizeof(aif_form_chunk);
    BytesNeededForAif += sizeof(aif_common_chunk);
    BytesNeededForAif += sizeof(aif_ssnd_chunk);

    for(chunk_node *ThisChunk = ChunkDirectory; 
	    ThisChunk != (chunk_node *)NULLPTR; 
	    ThisChunk = ThisChunk->NextChunk)
    {
	switch(ThisChunk->HashedID)
	{
	    case smpl:
	    {
		wav_smpl_chunk *Wav_SmplChunk = (wav_smpl_chunk *)ThisChunk->Wav_ChunkAddr;
		if(Wav_SmplChunk->NumSampleLoops)
		{
		    BytesNeededForAif += sizeof(aif_marker_chunk);
		    BytesNeededForAif += (sizeof(aif_marker) * Wav_SmplChunk->NumSampleLoops);
		} 
	    } break;
    
	    case inst:
	    {
		BytesNeededForAif += sizeof(aif_inst_chunk);
	    } break;

	    case data:
	    {
		wav_data_chunk *Wav_DataChunk = (wav_data_chunk *)ThisChunk->Wav_ChunkAddr;
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
			"Error: Encountered unexepected chunk when calculating"
			"bytes needed for .aif file\n");	
		OutputDebugStringA((char *)DebugPrintStringBuffer);
		exit(1);
	    } break;
	}
    }

    // Allocate bytes for the converted .aif file
    
    arena *Aif_Arena = ArenaAlloc(BytesNeededForAif + offsetof(arena, ArenaStart));

    // FORM Chunk
    aif_form_chunk *Aif_FormChunk = PushStruct(Aif_Arena, aif_form_chunk);
    CopyIDToAif("FORM", Aif_FormChunk->ID);
    u32 Aif_FormChunkSizeLittleEndian = (BytesNeededForAif - offsetof(aif_form_chunk, FormType));
    Aif_FormChunk->ChunkSize = FlipEndiannessU32(Aif_FormChunkSizeLittleEndian);
    CopyIDToAif("AIFF", Aif_FormChunk->FormType);

    // COMM Chunk
    aif_common_chunk *Aif_CommonChunk = PushStruct(Aif_Arena, aif_common_chunk);
    CopyIDToAif("COMM", Aif_CommonChunk->ID);
    wav_fmt_chunk *Wav_FormatChunk = (wav_fmt_chunk *)GetWavChunkPointer(fmt);
    Aif_CommonChunk->ChunkSize = FlipEndiannessU32((u32)18); // always 18
    Aif_CommonChunk->NumChannels = FlipEndiannessU16((u16)Wav_FormatChunk->NumChannels); 

    wav_fmt_chunk *Wav_DataChunk = (wav_fmt_chunk *)GetWavChunkPointer(data);
    u32 Wav_NumSampleFramesLittleEndian = (u32)(Wav_DataChunk->ChunkSize / Wav_FormatChunk->BlockAlign);
    Aif_CommonChunk->NumSampleFrames = FlipEndiannessU32(Wav_NumSampleFramesLittleEndian);
    Aif_CommonChunk->SampleSize = FlipEndiannessU16(Wav_FormatChunk->BitsPerSample);
    Write10ByteSampleRate(Aif_CommonChunk, Wav_FormatChunk->SampleRate);



















    










    return(0);
}
