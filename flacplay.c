//link with -lFLAC -lao -lpthread switches


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __gnu_linux__
	#include <x86_64-linux-gnu/sys/stat.h>
#elif _DARWIN_
	#include </sys/stat.h>
#endif

#include <pthread.h>
#include <FLAC/stream_decoder.h>
#include <FLAC/metadata.h>
#include <ao/ao.h>

FLAC__StreamDecoder *FLAC_decoder;//FLAC decoder handle
FLAC__StreamMetadata **tags;//Tags handle

ao_device *dev;//Audio device
ao_sample_format *format;//Output format
ao_info *driver_info;//Driver info
int driver;//Driver code
FLAC__uint64 bytes_count;

pthread_t audio_thread;

int check_if_flac(FILE *fp);
void initialize_ao();
void *decode_FLAC();
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

int main(int argc,char **argv)
{	
	int fileindex=1;
	while(fileindex < argc)
	{
		char *filename = argv[fileindex];
		FILE *fp=fopen(filename,"rb");
	
		if(check_if_flac(fp)!=0)//Check if FLAC
		{
			fprintf(stderr,"Input is not a valid FLAC file.\n");
			fclose(fp);
		}
		else
		{
			//Initialize
			ao_initialize();
			driver=ao_default_driver_id();//Get default driver id
			format=(ao_sample_format *)malloc(sizeof(ao_sample_format));
			driver_info=ao_driver_info(driver);
			FLAC_decoder = FLAC__stream_decoder_new();
			(void)FLAC__stream_decoder_set_md5_checking(FLAC_decoder,false);
			FLAC__stream_decoder_init_stream(FLAC_decoder, read_callback, seek_callback, tell_callback, length_callback, eof_callback, write_callback, metadata_callback, error_callback,(void *)fp);
			
			//Start playback
			fprintf(stderr,"Playing file %d of %d: %s\n",fileindex,(argc-1),filename);
			fprintf(stderr,"----------------------------------------------------\n");
			pthread_create(&audio_thread,NULL,&decode_FLAC,NULL);
			
			//Wait for audio to finish
			pthread_join(audio_thread,NULL);
			fprintf(stderr,"----------------------------------------------------\n");

			//Clean up
			free(format);
			fclose(fp);
			FLAC__stream_decoder_finish(FLAC_decoder);
			FLAC__stream_decoder_delete(FLAC_decoder);
			ao_close(dev);
			ao_shutdown();
		}
		fileindex++;//Get next file from queue
	}
	return 0;
}


int check_if_flac(FILE *fp)
{
	char header[4];
	size_t bytes=fread(header,sizeof(char),4,fp);
	fseek(fp,0,SEEK_SET);
	if(strncmp(header,"fLaC",4)==0)
		return 0;
	else
		return -1;
}

void initialize_ao()
{
	ao_initialize();
	driver=ao_default_driver_id();//Get default driver id
	format=(ao_sample_format *)malloc(sizeof(ao_sample_format));
	driver_info=ao_driver_info(driver);
}

void *decode_FLAC(void *arg)
{
	FLAC__bool ok = true;
	if(ok)
	{
		ok=FLAC__stream_decoder_process_until_end_of_metadata(FLAC_decoder);
		fprintf(stderr, "%s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(FLAC_decoder)]);
		ok=FLAC__stream_decoder_process_until_end_of_stream(FLAC_decoder);
		fprintf(stderr, "\n%s\n", FLAC__StreamDecoderStateString[FLAC__stream_decoder_get_state(FLAC_decoder)]);
	}
	return NULL;
}

void *get_tags(void *arg)
{
	char *filename=(char *)arg;
	FLAC__bool ok=true;
	FLAC__metadata_get_tags(filename,tags);
	return NULL;
}

void print_tags(FLAC__StreamMetadata **tags)
{
	fprintf(stdout,"%d\n",(int)tags[0]->data.vorbis_comment.num_comments);
}

//READ CALLBACK
static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder,FLAC__byte buffer[],size_t *bytes,void *client_data)
{
	FILE *fp=(FILE *)client_data;
	
	if(*bytes > 0)
	{
		*bytes=fread(buffer,sizeof(FLAC__byte),*bytes,fp);
		if(ferror(fp))
		{
			return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
		}
		else if(*bytes==0)
		{
			return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
		}
		else
		{
			FLAC__stream_decoder_get_decode_position(decoder,&bytes_count);
			fprintf(stderr,"BYTES_READ : %lu\r",bytes_count);
			return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
		}
	}
	return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

//SEEK CALLBACK
static FLAC__StreamDecoderSeekStatus seek_callback(const FLAC__StreamDecoder *decoder,FLAC__uint64 absolute_byte_offset,void *client_data)
{
	FILE *fp=(FILE *)client_data;
	if(fp==stdin)
	{
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
	}
	if(fseeko(fp,(off_t)absolute_byte_offset,SEEK_SET)<0)
	{
		return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
	}
	return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

//TELL CALLBACK
static FLAC__StreamDecoderTellStatus tell_callback(const FLAC__StreamDecoder *decoder,FLAC__uint64 *absolute_byte_offset,void *client_data)
{
	FILE *fp=(FILE *)client_data;
	off_t pos;
	
	if(fp==stdin)
	{
		return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
	}
	if((pos=ftello(fp))<0)
	{
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
	}
	
	*absolute_byte_offset=(FLAC__uint64)pos;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

//LENGTH CALLBACK
static FLAC__StreamDecoderLengthStatus length_callback(const FLAC__StreamDecoder *decoder,FLAC__uint64 *stream_length,void *client_data)
{
	FILE *fp=(FILE *)client_data;
	struct stat filestats;
	
	if(fp==stdin)
	{
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	}
	if(fstat(fileno(fp),&filestats)!=0)
	{
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	}
	
	*stream_length=(FLAC__uint64)filestats.st_size;
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

//EOF CALLBACK
static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder,void *client_data)//EOF callback
{
	FILE *fp=(FILE *)client_data;
	if(feof(fp))
	{
		return true;
	}
	return false;
}

//FLAC WRITE CALLBACK
FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
	FLAC__int16 audio_buffer[frame->header.channels*frame->header.blocksize];//Audio buffer which is to be played
	int i;
	
	if(frame->header.channels != 2)
	{
		fprintf(stderr,"Sorry, this program only support files with two channels.\n");
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	//Fill audio buffer
	for(i = 0; i < frame->header.blocksize; i++)
	{
		audio_buffer[2*i]=(FLAC__int16)buffer[0][i];
		audio_buffer[2*i+1]=(FLAC__int16)buffer[1][i];
	}
	
	//and play
	ao_play(dev,(char *)audio_buffer,(2*frame->header.channels*frame->header.blocksize));
	//2 is multiplied to compensate for conversion from FLAC__int16(16 bytes) to char(8 bytes).
	
	//Continue playing
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

//FLAC METADATA CALLBACK
void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	(void)client_data;
	
	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
	{
		//Set up audio device
		format->bits=metadata->data.stream_info.bits_per_sample;
		format->rate=metadata->data.stream_info.sample_rate;
		format->channels=metadata->data.stream_info.channels;
		format->byte_format=AO_FMT_NATIVE;
		format->matrix=0;
		dev=ao_open_live(driver,format,NULL);
		
		fprintf(stdout,"%s\n",driver_info->name);
		fprintf(stdout,"Sample Rate : %d Hz\n",metadata->data.stream_info.sample_rate);
		fprintf(stdout,"Channel count : %d\n",metadata->data.stream_info.channels);
		fprintf(stdout,"Bits per sample : %d\n",metadata->data.stream_info.bits_per_sample);
		fprintf(stdout,"Length : %d seconds\n",(int)((metadata->data.stream_info.total_samples)/(metadata->data.stream_info.sample_rate)));
	}
}

//FLAC ERROR CALLBACK
void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;
	fprintf(stderr, "Decoder Error: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}
