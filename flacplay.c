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
#include <FLAC/callback.h>
#include <ao/ao.h>
#include "packetqueue.h"
#include "flacplay.h"

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
			PacketQueueInit(list);
			pthread_cond_init(&cond,NULL);
			pthread_mutex_init(&mutex,NULL);
			
			//Start playback
			fprintf(stderr,"Playing file %d of %d: %s\n",fileindex,(argc-1),filename);
			fprintf(stderr,"--------------------------------------------------------\n");
			get_tags((void *)filename);
			
			pthread_create(&decode_thread,NULL,&decode_FLAC,NULL);//Start decoder thread
			pthread_create(&play_thread,NULL,&play_FLAC,(void *)fp);//Start playback thread
			
			//Wait for audio to finish
			pthread_join(decode_thread,NULL);
			pthread_join(play_thread,NULL);
			
			fprintf(stderr,"--------------------------------------------------------\n");

			//Clean up
			free(format);
			format=NULL;
			fclose(fp);
			pthread_mutex_destroy(&mutex);
			pthread_cond_destroy(&cond);
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

void *play_FLAC(void *arg)
{
	FILE *fp=(FILE *)arg;
	FLAC__bool ok=false;
	while(ok==false)
	{
		pthread_mutex_lock(&mutex);
		
		while(list.num_packets==0)
		{
			pthread_cond_wait(&cond,&mutex);
		}
		PacketNode *node=PacketQueueGet(&list);
		ao_play(dev,node->packet,node->packet_size);
		pthread_cond_signal(&cond);
		pthread_mutex_unlock(&mutex);
		fprintf(stderr,"Num of packets in queue : %3d\r",list.num_packets);
		
		//Clean up to prevent huge memory leaks
		free(node->packet);
		node->packet=NULL;
		free(node);
		node=NULL;
		
		ok=eof_callback(FLAC_decoder,fp);
	}
	
	//Play remaining packets in queue
	while(list.num_packets!=0)
	{
		PacketNode *node=PacketQueueGet(&list);
		ao_play(dev,node->packet,node->packet_size);
		free(node->packet);
		node->packet=NULL;
		free(node);
		node=NULL;
		fprintf(stderr,"Num of packets in queue : %3d\r",list.num_packets);
	}
	return NULL;
}

void *get_tags(void *arg)
{
	char *filename=(char *)arg;
	FLAC__Metadata_Chain *chain=FLAC__metadata_chain_new();
	FLAC__Metadata_Iterator *iterator=FLAC__metadata_iterator_new();
	FLAC__StreamMetadata *metadata;

	if(FLAC__metadata_chain_status(chain)!=FLAC__METADATA_CHAIN_STATUS_OK)
	{
		fprintf(stderr,"Error in chain.\n");
		return 0;
	}
	else
	{
		FLAC__metadata_chain_read(chain,filename);
		FLAC__metadata_iterator_init(iterator,chain);
		int i=0;
		do
		{
			FLAC__metadata_iterator_next(iterator);
		}
		while(FLAC__metadata_iterator_get_block_type(iterator)!=FLAC__METADATA_TYPE_VORBIS_COMMENT);
		
		metadata=FLAC__metadata_iterator_get_block(iterator);
		if(metadata->type==FLAC__METADATA_TYPE_VORBIS_COMMENT)
		{
			for(i=0;i<(int)metadata->data.vorbis_comment.num_comments;i++)
			{
				fprintf(stderr,"%s\n",metadata->data.vorbis_comment.comments[i].entry);
			}
		}
	}

	FLAC__metadata_iterator_delete(iterator);
	FLAC__metadata_chain_delete(chain);
	return NULL;
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
	
	if(fp==stdin)
	{
		return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
	}
	
	struct stat *filestats=(struct stat *)malloc(sizeof(struct stat));
	if(fstat(fileno(fp),filestats)!=0)
	{
		return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
	}
	
	*stream_length=(FLAC__uint64)filestats->st_size;
	
	free(filestats);
	filestats=NULL;
	
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

//EOF CALLBACK
static FLAC__bool eof_callback(const FLAC__StreamDecoder *decoder,void *client_data)//EOF callback
{
	(void)decoder;
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
	size_t channels=frame->header.channels;
	size_t blocksize=frame->header.blocksize;
	
	FLAC__int16 *audio_buffer=(FLAC__int16 *)malloc((channels * blocksize) * sizeof(FLAC__int16));//Audio buffer which is to be played
	PacketNode *node=(PacketNode *)malloc(sizeof(PacketNode));
	int i,j;
	
	//Fill audio buffer
	for(i = 0; i < frame->header.blocksize; i++)
	{
		for(j = 0; j < channels; j++)
		{
			*(audio_buffer + (channels*i) + j)=(FLAC__int16) *( *(buffer + j) + i);
		}
	}
	
	node->packet=(char *)audio_buffer;
	node->packet_size=2*channels*blocksize;
	
	pthread_mutex_lock(&mutex);
	while(list.num_packets==100)
	{
		pthread_cond_wait(&cond,&mutex);
	}
	
	PacketQueuePut(&list,node);//Put in packet queue
	pthread_cond_signal(&cond);
	pthread_mutex_unlock(&mutex);
	
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
