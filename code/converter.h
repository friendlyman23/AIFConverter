#if !defined (CONVERTER_H)

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

struct pstring
{
    //todo: assert that the count is even
    //	per the spec, all pstring counts must be even
    //	(not including pad bytes)
    //
    //note: when writing to wav, do not store the entire buffer
    //	only store the number of characters in Count
    uint8 StringLength;
    char Letters[MAX_STRING_LEN];
};

struct marker
{
    int16 ID;
    // todo: assert that ID is a positive, nonzero int
    uint32 Position;
    pstring PString;
};

struct marker_chunk_header
{
    uint8 *Address;
    char ID[ID_WIDTH + 1];
    int32 Size;
    uint16 NumMarkers;
    marker *Markers;
};

struct loop
{
    int16 PlayMode;
    int16 BeginLoopMarker;
    int16 EndLoopMarker;
};

struct instrument_chunk
{
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

struct filler_chunk
{
    // this appears in aif files but is not documented in the aif spec.
    // For some reason.
    // In the file, filler bytes is plainly a 4-byte int. We assume
    // it's a uint because: what possible need could there be for specifying a negative
    // number of filler bytes? Although, elsehwere in the spec, plenty of examples
    // of signed ints used for values that would never be negative.
    char ID[ID_WIDTH + 1];
    uint32 NumFillerBytes;

};

struct sound_data_chunk_header
{
    char ID[ID_WIDTH + 1];
    int32 ChunkSize;
    uint32 Offset;
    uint32 BlockSize;
    uint8 *DataStart;
};

#define CONVERTER_H
#endif
