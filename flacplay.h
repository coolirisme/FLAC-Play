#ifndef _FLACPLAY_H_
#define _FLACPLAY_H_

FLAC__StreamDecoder *FLAC_decoder;//FLAC decoder handle
FLAC__StreamMetadata **tags;//Tags handle
FLAC__Metadata_Chain *chain;

ao_device *dev;//Audio device
ao_sample_format *format;//Output format
ao_info *driver_info;//Driver info
int driver;//Driver code

pthread_t decode_thread,play_thread;
pthread_cond_t cond;
pthread_mutex_t mutex;

PacketList list;

int check_if_flac(FILE *fp);
void initialize_ao();
void *decode_FLAC();
void *play_FLAC(void *arg);
void *get_tags(void *arg);
void print_tags(FLAC__StreamMetadata **tags);

/*CALLBACKS Required by FLAC*/
static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder,FLAC__byte buffer[],size_t *bytes,void *client_data);//Read callback
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);//Write callback
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);//Metadata callback
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);//Error callback
static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder,FLAC__uint64 absolute_byte_offset,void *client_data);//Seek callback
static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder,FLAC__uint64 *absolute_byte_offset,void *client_data);//Tell callback
static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder,FLAC__uint64 *stream_length,void *client_data);//Length callback
static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder,void *client_data);//EOF callback

#endif
