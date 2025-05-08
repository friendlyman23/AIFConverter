#if !defined (CONVERTER_H)

#include <stddef.h>

#define Assert(Value) if(!(Value)) {*(int *)0 = 0;}
#define ArrayCount(Array) ( (sizeof(Array)) / (sizeof(Array[0])) )
#define Stringize(Variable) #Variable

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
 * aif stores sample rate in an 80-bit (10-byte) floating point
 * data type. MSVC doesn't support this type. We attempt to
 * truncate it and store it in a double. We abort the conversion if
 * the truncation would result in data loss.
*/
#define EXTENDED_WIDTH 10


// miscellaneous defines
#define BITS_IN_BYTE 8
#define MAX_STRING_LEN 255 


struct form_chunk
{
    uint8 *Address;
    char ID[ID_WIDTH + 1];
    int32 DataSize;
    char Type[ID_WIDTH + 1];
    uint8 *DataStart;
};

struct common_chunk
{
    uint8 *Address;
    char ID[ID_WIDTH + 1];
    int32 DataSize;
    int16 NumChannels;
    uint32 NumSampleFrames;
    int16 SampleSize;
    uint32 SampleRate;
};

// This struct never actually needs to be delcared
//    but leaving it in for now for reference
struct marker
{
    int16 ID;
    uint32 Position;
    uint8 MarkerNameLen;
    //struct hack
    char MarkerName[];
};

// macro that represents the size of the first two items of a marker
//    struct. we use this later in computing the amount of memory
//    required to store the marker data we read from the .aif file
#define MARKER_BOILERPLATE (offsetof(marker, MarkerNameLen))

struct marker_chunk_header
{
    char ID[ID_WIDTH + 1];
    uint8 *Address;
    int32 Size;
    uint16 TotalMarkers;
    uint8 *Markers;
};


struct loop
{
    int16 PlayMode;
    int16 BeginLoopMarker;
    int16 EndLoopMarker;
};

struct instrument_chunk
{
    uint8 *Address;
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    int8 BaseNote;
    char *BaseNoteDecode;
    int8 Detune;
    int8 LowNote;
    char *LowNoteDecode;
    int8 HighNote;
    char *HighNoteDecode;
    int8 LowVelocity;
    int8 HighVelocity;
    int16 Gain;
    loop SustainLoop;
    loop ReleaseLoop;
};

struct filler_chunk_header
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

struct sound_data_chunk_header
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    uint32 Offset;
    uint32 BlockSize;
    uint8 *DataStart;
};


		    /**********************WAV FILE SECTION**************************/

/*
 * Currently, this utility only supports conversion of uncompressed PCM .aif files 
 * to uncompressed PCM .wav files. For such files, some fields in .wav file headers 
 * are constant and can be declared at compile time.
 *
 	For uncompressed PCM .wav files, the size of the format chunk should always
 *	be 16 bytes, because the format chunk will only require the following
 *	fields:
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

/*
 *    For uncompressed PCM, the FormatTag field should always be 1:
 */
#define WAV_UNCOMPRESSED_PCM_FORMAT_TAG 1


#pragma pack(push,1)
struct wav_header {
    // Riff chunk
    char GroupID[ID_WIDTH];       // "RIFF" 4
    uint32 WavFileSize;				  
    char RiffType[ID_WIDTH];        // "WAVE" 4
				    
    // Format chunk
    char FormatChunkID[ID_WIDTH];   // "fmt " 4
    uint32 FormatChunkDataSize = WAV_FORMAT_CHUNK_DATA_SIZE;
    uint16 FormatTag = WAV_UNCOMPRESSED_PCM_FORMAT_TAG;
    uint16 NumChannels;
    uint32 SampleRate;
    uint32 AvgBytesPerSec;     // sampleRate × numChannels × bitsPerSample/8 (4)
    uint16 BlockAlign;   // numChannels × bitsPerSample/8 (2)
    uint16 BitsPerSample;  

    // Data chunk
    char DataChunkID[ID_WIDTH]; // "data"
    uint32 DataChunkDataSize;// numSamples × numChannels × bitsPerSample/8 (4)
};
#pragma pack(pop)

/* We can now define a size macro to help compute the wav_header's
 * "WavFileSize" field. WavFileSize should be equal to
 *	
 *	(Total size of file) - 
 *	[ (size of wav_header GroupID field) + (size of wav_header WavFileSize field) ]
 *
 * i.e., The size of the file, less the GroupID and WavFileSize fields.
 *
 */
#define HEADER_SIZE_FOR_FILE_SIZE_CALC (offsetof(wav_header, RiffType))

// .aif files encode note pitches via their corresponding midi pitch value, which
// is a number between 0 and 128. We fetch the corresponding pitch value via this table.
char *MidiNoteLUT[128] = 
{
    /*  0 - 11 */   "C-2",  "C#-2", "D-2",  "D#-2", "E-2",  "F-2",  "F#-2", "G-2",  "G#-2", "A-2",  "A#-2", "B-2",
    /* 12 - 23 */   "C-1",  "C#-1", "D-1",  "D#-1", "E-1",  "F-1",  "F#-1", "G-1",  "G#-1", "A-1",  "A#-1", "B-1",
    /* 24 - 35 */   "C0",   "C#0",  "D0",   "D#0",  "E0",   "F0",   "F#0",  "G0",   "G#0",  "A0",   "A#0",  "B0",
    /* 36 - 47 */   "C1",   "C#1",  "D1",   "D#1",  "E1",   "F1",   "F#1",  "G1",   "G#1",  "A1",   "A#1",  "B1",
    /* 48 - 59 */   "C2",   "C#2",  "D2",   "D#2",  "E2",   "F2",   "F#2",  "G2",   "G#2",  "A2",   "A#2",  "B2",
    /* 60 - 71 */   "C3",   "C#3",  "D3",   "D#3",  "E3",   "F3",   "F#3",  "G3",   "G#3",  "A3",   "A#3",  "B3",
    /* 72 - 83 */   "C4",   "C#4",  "D4",   "D#4",  "E4",   "F4",   "F#4",  "G4",   "G#4",  "A4",   "A#4",  "B4",
    /* 84 - 95 */   "C5",   "C#5",  "D5",   "D#5",  "E5",   "F5",   "F#5",  "G5",   "G#5",  "A5",   "A#5",  "B5",
    /* 96 - 107 */  "C6",   "C#6",  "D6",   "D#6",  "E6",   "F6",   "F#6",  "G6",   "G#6",  "A6",   "A#6",  "B6",
    /*108 - 119 */  "C7",   "C#7",  "D7",   "D#7",  "E7",   "F7",   "F#7",  "G7",   "G#7",  "A7",   "A#7",  "B7",
    /*120 - 127 */  "C8",   "C#8",  "D8",   "D#8",  "E8",   "F8",   "F#8",  "G8"
};



#define CONVERTER_H
#endif
