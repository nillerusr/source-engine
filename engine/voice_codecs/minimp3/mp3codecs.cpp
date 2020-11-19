#include "interface.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"
#include "vaudio/ivaudio.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

struct AudioStream_s
{
	IAudioStreamEvent *stream_event;
	int offset;
	unsigned int decode_size;
};

size_t mp3dec_read_callback(void *buf, size_t size, void *user_data)
{
	AudioStream_s *stream = static_cast<AudioStream_s*>( (void*)user_data);
	int ret_size = stream->stream_event->StreamRequestData( buf, size, stream->offset );
//	printf("mp3dec_read_callback size: %d, ret_size: %d\n", (int)size, ret_size);	
	stream->offset += ret_size;

	return ret_size;
}


int mp3dec_seek_callback(uint64_t position, void *user_data)
{
	struct AudioStream_s *stream = static_cast<AudioStream_s*>( (void*)user_data);
	stream->offset = position;
//	printf("mp3dec_seek_callback position: %d\n", (int)position);	

	return 0;
}

class CMiniMP3 : public IAudioStream
{
public:
	bool Init( IAudioStreamEvent *pHandler );

	// IAudioStream functions
	virtual int	Decode( void *pBuffer, unsigned int bufferSize );
	virtual int GetOutputBits();
	virtual int GetOutputRate();
	virtual int GetOutputChannels();
	virtual unsigned int GetPosition();
	virtual void SetPosition( unsigned int position );
private:

    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	mp3dec_ex_t mp3d;
	mp3dec_io_t mp3io;
	struct AudioStream_s audio_stream;
};

bool CMiniMP3::Init( IAudioStreamEvent *pHandler )
{
//	printf("CMiniMP3::Init\n");

	audio_stream.stream_event = pHandler;
	audio_stream.offset = 0;
	audio_stream.decode_size = 0;

	mp3io.read = &mp3dec_read_callback;
	mp3io.read_data = &audio_stream;
	mp3io.seek = &mp3dec_seek_callback;
	mp3io.seek_data = &audio_stream;
	if( mp3dec_ex_open_cb(&mp3d, &mp3io, MP3D_SEEK_TO_SAMPLE) )
	{
//		printf("mp3dec_ex_open_cb failed\n");		
		return false;
	}

	if ( mp3dec_ex_seek(&mp3d, 0) )
	{
//		printf("mp3dec_ex_seek failed\n");		
		return false;
	}

	return true;
}


// IAudioStream functions
int	CMiniMP3::Decode( void *pBuffer, unsigned int bufferSize )
{
	size_t readed = mp3dec_ex_read(&mp3d, (mp3d_sample_t*)pBuffer, bufferSize/2);
	return readed*2;
}


int CMiniMP3::GetOutputBits()
{
	return 16;
}


int CMiniMP3::GetOutputRate()
{
	return mp3d.info.hz;
}


int CMiniMP3::GetOutputChannels()
{
	return mp3d.info.channels;
}


unsigned int CMiniMP3::GetPosition()
{
	return audio_stream.offset;
}

void CMiniMP3::SetPosition( unsigned int position )
{
	audio_stream.offset = position;
	mp3dec_ex_seek(&mp3d, position);
}


class CVAudio : public IVAudio
{
public:
	IAudioStream *CreateMP3StreamDecoder( IAudioStreamEvent *pEventHandler )
	{
		CMiniMP3 *pMP3 = new CMiniMP3;
		if ( !pMP3->Init( pEventHandler ) )
		{
			delete pMP3;
			return NULL;
		}
		return pMP3;
	}

	void DestroyMP3StreamDecoder( IAudioStream *pDecoder )
	{
		 delete pDecoder;
	}
};

EXPOSE_INTERFACE( CVAudio, IVAudio, VAUDIO_INTERFACE_VERSION );

