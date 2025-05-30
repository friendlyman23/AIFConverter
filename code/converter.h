#if !defined (CONVERTER_H)

#include <windows.h>
#include <stddef.h>
#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#define static global


#define MAX_LOOP_CHUNKS 25

// Hash values for chunk types. We hash the 4-byte
//    strings used in .aif files so that we can quickly
//    match them with a switch statement as we parse the file,
//    rather than doing endless if(strncmp()) tests.
//    Note: Values derived from Gperf hash function

typedef enum 
{
    FORM_CHUNK =		     20,
    COMMON_CHUNK =		     15,
    SOUND_DATA_CHUNK =		     30,
    MARKER_CHUNK =		     14,
    INSTRUMENT_CHUNK =		      0,
    COMMENT_CHUNK =		     10,
    NAME_CHUNK =		     24,
    AUTHOR_CHUNK =		      8,
    COPYRIGHT_CHUNK =		     13,
    ANNOTATION_CHUNK =		      4,
    AUDIO_RECORDING_CHUNK =	     19,
    MIDI_CHUNK =		      5,
    APPLICATION_SPECIFIC_CHUNK =      9,
    FILLER_CHUNK =		     25
} chunk;

// Other useful chunk-related defines for readability. We talk about
//    chunks a lot in the code.
#define TOTAL_CHUNK_TYPES	     13
#define HASHED_CHUNK_ID_ARRAY_SIZE     31

//    .aif files have three essential chunks:
//	  1.) Form chunk
//	  2.) Common chunk
//	  3.) Sound Data chunk
#define TOTAL_IMPORTANT_CHUNK_TYPES  3


// Function-like macros
#define Assert(Value) if(!(Value)) {*(int *)0 = 0;}
#define ArrayCount(Array) ( (sizeof(Array)) / (sizeof(Array[0])) )
#define Stringize(Variable) #Variable
#define Kilobytes(NumKilobytes) ((NumKilobytes) * 1024)
#define Megabytes(NumMegabytes) ((Kilobytes((NumMegabytes))) * 1024)

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

// Width of special extended precision floating point number that
//    .aif files use to store sample rate
#define EXTENDED_WIDTH 10

// Convenient macro that allows us to quickly skip over some boilerplate
//    fields in chunk headers
#define CHUNK_HEADER_BOILERPLATE (ID_WIDTH + sizeof(int32))

// miscellaneous defines
#define BITS_IN_BYTE 8
#define MAX_STRING_LEN 255 

// Here come da chunks
    // Form chunk
    // Common chunk		    Highest precedence
    // Sound Data chunk
    // Marker chunk
    // Instrument chunk
    // Comment chunk
    // Name chunk
    // Author chunk
    // Copyright chunk
    // Annotation chunk
    // Audio Recording chunk
    // MIDI Data chunk
    // Application Specific chunk   Lowest precedence

struct arena
{
    uint64 ArenaSize;
    uint8 *DoNotCrossThisLine;
    uint8 *NextFreeByte;
    uint8 ArenaStart[];
};

#define ARENA_BOILERPLATE offsetof(arena, ArenaStart)

struct chunk_finder
{
    chunk HashedID;
    uint8 *Aif_Chunk;
    uint8 *Conv_Chunk;
};

struct conv_form_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    char FormType[ID_WIDTH + 1];
    uint8 *Aif_SubChunksStart;
};

#pragma pack(push,1)
struct aif_form_chunk
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    char FormType[ID_WIDTH];
    uint8 SubChunksStart;
};
#pragma pack(pop)

struct conv_common_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    int16 NumChannels;
    uint32 NumSampleFrames;
    int16 SampleSize;
    uint32 SampleRate;
};
#pragma pack(push,1)
struct aif_common_chunk
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    int16 NumChannels;
    uint32 NumSampleFrames;
    int16 SampleSize;
    uint32 SampleRate;
};
#pragma pack(pop)

#pragma pack(push,1)
struct aif_marker
{
    int16 ID;
    uint32 Position;
    uint8 MarkerNameLen;
    uint8 MarkerNameStart;
};
#pragma pack(pop)

struct conv_marker
{
    int16 ID; // For some reason .aif uses "ID" to refer to the unique
			//    16-bit number assigned to each Marker, even though
			//    in all other .aif contexts "ID" means "4-byte string"
    uint32 Position;
    uint8 MarkerNameLen;
    uint8 *MarkerNameStart;
};

#pragma pack(push,1)
struct aif_marker_chunk
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    uint16 TotalMarkers;
    uint8 MarkersStart;
};
#pragma pack(pop)

struct conv_marker_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    uint16 TotalMarkers;
    int NumPointlessMarkers;
    uint8 *Aif_MarkersStart;
    conv_marker *Conv_MarkersStart;
};


// Macro that represents the size of the first three items of an .aif Marker
//    chunk. We use this later in computing the amount of memory
//    required to store the marker data we read from the .aif file.
//
//			     Aif_MarkerID       Position      MarkerNameLen
#define MARKER_BOILERPLATE ( sizeof(int16) + sizeof(uint32) + sizeof(uint8) )

struct conv_loop
{
    int16 PlayMode;
    int16 BeginLoopMarker;
    int16 EndLoopMarker;
};

#pragma pack(push,1)
struct aif_loop
{
    int16 PlayMode;
    int16 BeginLoopMarker;
    int16 EndLoopMarker;
};
#pragma pack(pop)

struct conv_instrument_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    int8 BaseNote;
    int8 Detune;
    int8 LowNote;
    int8 HighNote;
    int8 LowVelocity;
    int8 HighVelocity;
    int16 Gain;
    conv_loop SustainLoop;
    conv_loop ReleaseLoop;
};

#pragma pack(push,1)
struct aif_instrument_chunk
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    int8 BaseNote;
    int8 Detune;
    int8 LowNote;
    int8 HighNote;
    int8 LowVelocity;
    int8 HighVelocity;
    int16 Gain;
    aif_loop SustainLoop;
    aif_loop ReleaseLoop;
};
#pragma pack(pop)

struct filler_chunk
{
    // this appears in aif files but is not documented in the aif spec.
    //	  For some reason.
    //	  In the file, filler bytes is plainly a 4-byte int. We assume
    //	  it's a uint because: what possible need could there be for specifying a negative
    //	  number of filler bytes? Although, elsehwere in the spec, plenty of examples
    //	  of signed ints used for values that would never be negative.
    char ID[ID_WIDTH + 1];
    uint32 TotalFillerBytes;

};

struct conv_sound_data_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    uint32 Offset;
    uint32 BlockSize;
    uint8 *Aif_SamplesStart;
};

#pragma pack(push,1)
struct aif_sound_data_chunk
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    uint32 Offset;
    uint32 BlockSize;
    uint8 SamplesStart;
};
#pragma pack(pop)


struct generic_chunk_header
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    uint8 Data[];
};

#pragma pack(push, 1)
struct wav_riff_chunk
{
    char ID[4];          // 'RIFF'
    uint32 ChunkSize;    // Size of the entire file minus 8 bytes
    char Format[4];      // 'WAVE'
};

struct wav_fmt_chunk
{
    char ID[4];            // 'fmt '
    uint32 ChunkSize;      // Size of the rest of this chunk (typically 16 for PCM)
    uint16 AudioFormat;    // PCM = 1, others for compression
    uint16 NumChannels;    // Mono = 1, Stereo = 2, etc.
    uint32 SampleRate;     // 44100, 48000, etc.
    uint32 ByteRate;       // SampleRate * NumChannels * BitsPerSample / 8
    uint16 BlockAlign;     // NumChannels * BitsPerSample / 8
    uint16 BitsPerSample;  // 8, 16, 24, etc.
};

struct wav_data_chunk
{
    char ID[4];          // 'data'
    uint32 ChunkSize;    // Number of bytes of sample data
    uint8 Samples[];
};

struct wav_inst_chunk
{
    char ID[4];               // 'inst'
    uint32 ChunkSize;         // Should be 7 bytes
    uint8 UnshiftedNote;      // MIDI root note
    int8 FineTune;            // -50 to +50 cents
    int8 Gain;                // -64 to +64 dB
    uint8 LowNote;            // Lowest note played
    uint8 HighNote;           // Highest note played
    uint8 LowVelocity;        // Lowest velocity played
    uint8 HighVelocity;       // Highest velocity played
    uint8 PadByte;
};

struct wav_sampler_chunk 
{
    char ID[ID_WIDTH];
    uint32 ChunkSize;
    uint32 Manufacturer;
    uint32 Product;
    uint32 SamplePeriod;
    uint32 MidiUnityNote;
    uint32 MidiPitchFraction;
    uint32 SmpteFormat;
    uint32 SmpteOffset;
    uint32 NumSampleLoops;
    uint32 SamplerData;
};

struct wav_sample_loop 
{
    uint32 CuePointID;
    uint32 Type;          
    uint32 Start;         
    uint32 End;           
    uint32 Fraction;
    uint32 PlayCount;     
};
#pragma pack(pop)

#define AIF_NO_LOOP 0
#define AIF_FWD_LOOP 1
#define AIF_FWD_BACK_LOOP 2
#define WAV_FWD_LOOP 0
#define WAV_FWD_BACK_LOOP 1

extern int    Global_CountOfUnimportantChunks;
extern uint32 Global_BytesNeededForWav;
extern int    Global_NumSampleLoops;
extern chunk_finder *Global_UnChunkDirectory;

#define CONVERTER_H
#endif
