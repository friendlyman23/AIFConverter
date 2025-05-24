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

struct form_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    char FormType[ID_WIDTH + 1];
};

#pragma pack(push,1)
struct aif_form_chunk
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    char FormType[ID_WIDTH];
    uint8 SubChunksStart[];
};
#pragma pack(pop)

struct common_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    int16 NumChannels;
    uint32 NumSampleFrames;
    int16 SampleSize;
    uint32 SampleRate;
};

struct marker_chunk
{
    char ID[ID_WIDTH + 1];
    int32 Size;
    uint16 TotalMarkers;
    uint8 *Markers;
};

#pragma pack(push,1)
struct aif_marker
{
    int16 Aif_MarkerID; // For some reason .aif uses "ID" to refer to the unique
			//    16-bit number assigned to each Marker, even though
			//    in all other .aif contexts "ID" means "4-byte string"
    uint32 Position;
    uint8 MarkerNameLen;
    char MarkerName[];
};
#pragma pack(pop)

// Macro that represents the size of the first three items of an .aif Marker
//    chunk. We use this later in computing the amount of memory
//    required to store the marker data we read from the .aif file.
//
//			     Aif_MarkerID       Position      MarkerNameLen
#define MARKER_BOILERPLATE ( sizeof(int16) + sizeof(uint32) + sizeof(uint8) )

struct loop
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

struct instrument_chunk
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
    loop SustainLoop;
    loop ReleaseLoop;
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
    // For some reason.
    // In the file, filler bytes is plainly a 4-byte int. We assume
    // it's a uint because: what possible need could there be for specifying a negative
    // number of filler bytes? Although, elsehwere in the spec, plenty of examples
    // of signed ints used for values that would never be negative.
    char ID[ID_WIDTH + 1];
    uint32 TotalFillerBytes;

};

struct sound_data_chunk
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    uint32 Offset;
    uint32 BlockSize;
    uint8 *SamplesStart;
};

struct generic_chunk_header
{
    char ID[ID_WIDTH];
    int32 ChunkSize;
    uint8 Data[];
};

		    /**********************WAV FILE SECTION**************************/

/*
 * Currently, this utility only supports conversion of uncompressed PCM .aif files 
 * to uncompressed PCM .wav files. For such files, some fields in .wav file headers 
 * are constant and can be declared at compile time:
 *
 *		Field			        Type	      Size (bytes)
 *	   ---------------                  ------------      ------------
 *	   FormatTag				uint16		    2
 *	   NumChannels				uint16		    2
 *	   SampleRate				int32		    4
 *	   AvgBytesPerSec			int32		    4
 *	   BlockAlign				int16		    2
 *	   BitsPerSample                        int16               2
 *
 *	                                                     Total: 16
 */    
#define WAV_FORMAT_CHUNK_DATA_SIZE 16

// For uncompressed PCM, the FormatTag field should always be 1:
#define WAV_UNCOMPRESSED_PCM_FORMAT_TAG 1


// Pack the bytes for the wav_header struct so we don't have to worry about struct
//    padding when writing .wav files to disk
#pragma pack(push,1)
struct wav_header {
    // Riff chunk
    char GroupID[ID_WIDTH];       // "RIFF" 4
    uint32 WavFileSize;				  
    char RiffType[ID_WIDTH];        // "WAVE" 4
				    
    // Format chunk
    char FormatChunkID[ID_WIDTH];   // "fmt " 4
    uint32 FormatChunk_ChunkSize = WAV_FORMAT_CHUNK_DATA_SIZE;
    uint16 FormatTag = WAV_UNCOMPRESSED_PCM_FORMAT_TAG;
    uint16 NumChannels;
    uint32 SampleRate;
    uint32 AvgBytesPerSec;     // sampleRate × numChannels × bitsPerSample/8 (4)
    uint16 BlockAlign;   // numChannels × bitsPerSample/8 (2)
    uint16 BitsPerSample;  

    // Data chunk
    char DataChunkID[ID_WIDTH]; // "data"
    uint32 DataChunk_ChunkSize;// numSamples × numChannels × bitsPerSample/8 (4)
};
#pragma pack(pop)

/* We can now define a size macro to help compute the wav_header's
 * "WavFileSize" field. WavFileSize should be equal to
 *	
 *	(Total size of file) - 
 *	[ (size of wav_header GroupID field) + (size of wav_header WavFileSize field) ]
 *
 * i.e., The size of the file, less the GroupID and WavFileSize fields (see above)
 *
 */
#define HEADER_SIZE_FOR_FILE_SIZE_CALC (offsetof(wav_header, RiffType))

#define CONVERTER_H
#endif
