//========= Copyright 1996-2009, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
// This module implements the voice record and compression functions 

#include <Carbon/Carbon.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>

#include "tier0/platform.h"
#include "tier0/threadtools.h"
//#include "tier0/vcrmode.h"
#include "ivoicerecord.h"


#define kNumSecAudioBuffer	1.0f

// ------------------------------------------------------------------------------
// VoiceRecord_AudioQueue
// ------------------------------------------------------------------------------

class VoiceRecord_AudioQueue : public IVoiceRecord
{
public:
	
	VoiceRecord_AudioQueue();
	virtual	~VoiceRecord_AudioQueue();

	// IVoiceRecord.
	virtual void		Release();
	
	virtual bool		RecordStart();
	virtual void		RecordStop();
	
	// Initialize. The format of the data we expect from the provider is
	// 8-bit signed mono at the specified sample rate.
	virtual bool		Init( int nSampleRate );

	virtual void		Idle();
	
	// Get the most recent N samples.
	virtual int			GetRecordedData(short *pOut, int nSamplesWanted );
	
	AudioUnit GetAudioUnit() { return m_AudioUnit; }
	AudioConverterRef GetConverter() { return m_Converter; }
	void RenderBuffer( const short *pszBuf, int nSamples );
	bool BRecording() { return m_bRecordingAudio; }
	void ClearThreadHandle() { m_hThread = NULL; m_bFirstInit = false; }
	
	AudioBufferList m_MicInputBuffer;
	AudioBufferList m_ConverterBuffer;
	void *m_pMicInputBuffer;
	
	int m_nMicInputSamplesAvaialble;
	float m_flSampleRateConversion;
	int m_nBufferFrameSize;
	int m_ConverterBufferSize;
	int m_MicInputBufferSize;
	int m_InputBytesPerPacket;

private:
	bool				InitalizeInterfaces();	// Initialize the openal capture buffers and other interfaces
	void				ReleaseInterfaces();	// Release openal buffers and other interfaces
	void				ClearInterfaces();				// Clear members.
	
	
private:
	AudioUnit m_AudioUnit;
	char *m_SampleBuffer;
	int m_SampleBufferSize;
	int m_nSampleRate;
	bool m_bRecordingAudio;
	bool m_bFirstInit;
	ThreadHandle_t m_hThread;
	AudioConverterRef m_Converter;
	
	CInterlockedUInt m_SampleBufferReadPos;
	CInterlockedUInt m_SampleBufferWritePos;
	
	//UInt32 nPackets = 0;
	//bool bHaveListData = false;

	
};


VoiceRecord_AudioQueue::VoiceRecord_AudioQueue() :
m_nSampleRate( 0 ), m_AudioUnit( NULL ), m_SampleBufferSize(0), m_SampleBuffer(NULL),
m_SampleBufferReadPos(0), m_SampleBufferWritePos(0), m_bRecordingAudio(false), m_hThread( NULL ), m_bFirstInit( true )
{
	ClearInterfaces();
}


VoiceRecord_AudioQueue::~VoiceRecord_AudioQueue()
{
	ReleaseInterfaces();
	if ( m_hThread )
		ReleaseThreadHandle( m_hThread );
	m_hThread = NULL;
}


void VoiceRecord_AudioQueue::Release()
{
	ReleaseInterfaces();
}

uintp StartAudio( void *pRecorder )
{
	VoiceRecord_AudioQueue *vr = (VoiceRecord_AudioQueue *)pRecorder;
	if ( vr )
	{
		//printf( "AudioOutputUnitStart\n" );
		AudioOutputUnitStart( vr->GetAudioUnit() );
		vr->ClearThreadHandle();
	}
	//printf( "StartAudio thread done\n" );

	return 0;
}

bool VoiceRecord_AudioQueue::RecordStart()
{
	if ( !m_AudioUnit )
		return false;
	
	if ( m_bFirstInit )
		m_hThread = CreateSimpleThread( StartAudio, this );
	else 
		AudioOutputUnitStart( m_AudioUnit );

	m_SampleBufferReadPos = m_SampleBufferWritePos = 0;

	m_bRecordingAudio = true;
	//printf( "VoiceRecord_AudioQueue::RecordStart\n" );
	return ( !m_bFirstInit || m_hThread != NULL );
}


void VoiceRecord_AudioQueue::RecordStop()
{
	// Stop capturing.
	if ( m_AudioUnit && m_bRecordingAudio )
	{
		AudioOutputUnitStop( m_AudioUnit );
		//printf( "AudioOutputUnitStop\n" );
	}

	m_SampleBufferReadPos = m_SampleBufferWritePos = 0;
	m_bRecordingAudio = false;

	if ( m_hThread )
		ReleaseThreadHandle( m_hThread );
	m_hThread = NULL;
}



OSStatus ComplexBufferFillPlayback( AudioConverterRef            inAudioConverter,
									UInt32                   *ioNumberDataPackets,
									AudioBufferList           *ioData,
									AudioStreamPacketDescription **outDataPacketDesc,
									void             *inUserData)
{
	VoiceRecord_AudioQueue *vr = (VoiceRecord_AudioQueue *)inUserData;
	if ( !vr->BRecording() )
		return noErr;

	if ( vr->m_nMicInputSamplesAvaialble )
	{
		int nBytesRequired = *ioNumberDataPackets * vr->m_InputBytesPerPacket;
		int nBytesAvailable = vr->m_nMicInputSamplesAvaialble*vr->m_InputBytesPerPacket;
		
		if ( nBytesRequired < nBytesAvailable )
		{
			ioData->mBuffers[0].mData = vr->m_MicInputBuffer.mBuffers[0].mData;
			ioData->mBuffers[0].mDataByteSize = nBytesRequired;
			vr->m_MicInputBuffer.mBuffers[0].mData = (char *)vr->m_MicInputBuffer.mBuffers[0].mData+nBytesRequired;
			vr->m_MicInputBuffer.mBuffers[0].mDataByteSize = nBytesAvailable - nBytesRequired;
		}
		else 
		{
			ioData->mBuffers[0].mData = vr->m_MicInputBuffer.mBuffers[0].mData;
			ioData->mBuffers[0].mDataByteSize = nBytesAvailable;
			vr->m_MicInputBuffer.mBuffers[0].mData = vr->m_pMicInputBuffer;
			vr->m_MicInputBuffer.mBuffers[0].mDataByteSize = vr->m_MicInputBufferSize;
		}
		
		*ioNumberDataPackets = ioData->mBuffers[0].mDataByteSize / vr->m_InputBytesPerPacket;
		vr->m_nMicInputSamplesAvaialble = nBytesAvailable / vr->m_InputBytesPerPacket - *ioNumberDataPackets;
	}
	else
	{
		*ioNumberDataPackets = 0;
		return -1;
	}
	
	return noErr;
}




static OSStatus recordingCallback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, 
                                  UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData) 
{	
	VoiceRecord_AudioQueue *vr = (VoiceRecord_AudioQueue *)inRefCon;
	if ( !vr->BRecording() )
		return noErr;
	
	OSStatus err = noErr;
	if ( vr->m_nMicInputSamplesAvaialble == 0 )
	{
		err = AudioUnitRender( vr->GetAudioUnit(), ioActionFlags, inTimeStamp, 1, inNumberFrames, &vr->m_MicInputBuffer );
		if ( err == noErr )
			vr->m_nMicInputSamplesAvaialble = vr->m_MicInputBuffer.mBuffers[0].mDataByteSize / vr->m_InputBytesPerPacket;
	}
	
	if ( vr->m_nMicInputSamplesAvaialble > 0 )
	{
		UInt32 nConverterSamples = ceil(vr->m_nMicInputSamplesAvaialble/vr->m_flSampleRateConversion);
		vr->m_ConverterBuffer.mBuffers[0].mDataByteSize = vr->m_ConverterBufferSize;
		OSStatus err = AudioConverterFillComplexBuffer( vr->GetConverter(),
													   ComplexBufferFillPlayback, 
													   vr, 
													   &nConverterSamples, 
													   &vr->m_ConverterBuffer, 
													   NULL );
		if ( err == noErr || err == -1 )
			vr->RenderBuffer( (short *)vr->m_ConverterBuffer.mBuffers[0].mData, vr->m_ConverterBuffer.mBuffers[0].mDataByteSize/sizeof(short) );
	}
	
	return err;
}


void VoiceRecord_AudioQueue::RenderBuffer( const short *pszBuf, int nSamples )
{
	int samplePos = m_SampleBufferWritePos;
	int samplePosBefore = samplePos;
	int readPos = m_SampleBufferReadPos;
	bool bBeforeRead = false;
	if ( samplePos <  readPos )
		bBeforeRead = true;
	char *pOut = (char *)(m_SampleBuffer + samplePos);
	int nFirstCopy = MIN( nSamples*sizeof(short), m_SampleBufferSize - samplePos );
	memcpy( pOut, pszBuf, nFirstCopy );
	samplePos += nFirstCopy;
	if ( nSamples*sizeof(short) > nFirstCopy )
	{
		nSamples -= ( nFirstCopy / sizeof(short) );
		samplePos = 0;
		memcpy( m_SampleBuffer, pszBuf + nFirstCopy, nSamples * sizeof(short) );
		samplePos += nSamples * sizeof(short);
	}
	
	m_SampleBufferWritePos = samplePos%m_SampleBufferSize;
	if ( (bBeforeRead && samplePos > readPos) )
	{
		m_SampleBufferReadPos = (readPos+m_SampleBufferSize/2)%m_SampleBufferSize; // if we crossed the read pointer then bump it forward
		//printf( "Crossed %d %d (%d)\n", (int)samplePosBefore, (int)samplePos, readPos );
	}
}


bool VoiceRecord_AudioQueue::InitalizeInterfaces()
{	
	//printf( "Initializing audio queue recorder\n" );
	// Describe audio component
	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_HALOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;
	
	Component comp = FindNextComponent(NULL, &desc);
    if (comp == NULL) 
		return false;
	
	OSStatus status = OpenAComponent(comp, &m_AudioUnit);  
	if ( status != noErr )
		return false;

	// Enable IO for recording
	UInt32 flag = 1;
	status = AudioUnitSetProperty( m_AudioUnit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 
								  1, &flag, sizeof(flag));
	if ( status != noErr )
		return false;

	// disable output on the device
	flag = 0;
	status = AudioUnitSetProperty( m_AudioUnit,kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 
						 0, &flag,sizeof(flag));
	if ( status != noErr )
		return false;

	UInt32 size = sizeof(AudioDeviceID);	
    AudioDeviceID inputDevice;
    status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,&size, &inputDevice);
	if ( status != noErr )
		return false;

    status =AudioUnitSetProperty( m_AudioUnit, kAudioOutputUnitProperty_CurrentDevice,  kAudioUnitScope_Global, 
							  0,  &inputDevice, sizeof(inputDevice));
	if ( status != noErr )
		return false;
	
	// Describe format
	AudioStreamBasicDescription audioDeviceFormat;
	size = sizeof(AudioStreamBasicDescription);
	status = AudioUnitGetProperty( m_AudioUnit,
								kAudioUnitProperty_StreamFormat,
								kAudioUnitScope_Input,
								1,  // input bus 
								&audioDeviceFormat,
								&size);
	
	if ( status != noErr )
		return false;
	
	// we only want mono audio, so if they have a stero input ask for mono
	if ( audioDeviceFormat.mChannelsPerFrame == 2 )
	{
		audioDeviceFormat.mChannelsPerFrame = 1;
		audioDeviceFormat.mBytesPerPacket /= 2;
		audioDeviceFormat.mBytesPerFrame /= 2;
	}
	
	// Apply format
	status = AudioUnitSetProperty( m_AudioUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 
								  1, &audioDeviceFormat, sizeof(audioDeviceFormat) );
	if ( status != noErr )
		return false;
	
	AudioStreamBasicDescription audioOutputFormat;
	audioOutputFormat					= audioDeviceFormat;
	audioOutputFormat.mFormatID			= kAudioFormatLinearPCM;
	audioOutputFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    audioOutputFormat.mBytesPerPacket   = 2; // 16-bit samples * 1 channels
    audioOutputFormat.mFramesPerPacket  = 1;
    audioOutputFormat.mBytesPerFrame    = 2; // 16-bit samples * 1 channels
    audioOutputFormat.mChannelsPerFrame = 1;
    audioOutputFormat.mBitsPerChannel   = 16;
    audioOutputFormat.mReserved         = 0;
	
	audioOutputFormat.mSampleRate = m_nSampleRate;
	
	m_flSampleRateConversion = audioDeviceFormat.mSampleRate / audioOutputFormat.mSampleRate;
	
	// setup sample rate conversion
	status = AudioConverterNew( &audioDeviceFormat, &audioOutputFormat, &m_Converter );
	if ( status != noErr )
		return false;

	
	UInt32 primeMethod = kConverterPrimeMethod_None;
	status = AudioConverterSetProperty( m_Converter, kAudioConverterPrimeMethod, sizeof(UInt32), &primeMethod);
	if ( status != noErr )
		return false;

	UInt32 quality = kAudioConverterQuality_Medium;
	status = AudioConverterSetProperty( m_Converter, kAudioConverterSampleRateConverterQuality, sizeof(UInt32), &quality);
	if ( status != noErr )
		return false;
			
	// Set input callback
	AURenderCallbackStruct callbackStruct;
	callbackStruct.inputProc = recordingCallback;
	callbackStruct.inputProcRefCon = this;
	status = AudioUnitSetProperty( m_AudioUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 
								  0,  &callbackStruct, sizeof(callbackStruct) );
	if ( status != noErr )
		return false;
	
	UInt32 bufferFrameSize;
	size = sizeof(bufferFrameSize);
	status = AudioDeviceGetProperty( inputDevice, 1, 1, kAudioDevicePropertyBufferFrameSize, &size, &bufferFrameSize );
	if ( status != noErr )
		return false;
	
	m_nBufferFrameSize = bufferFrameSize;
	
	// allocate the input and conversion sound storage buffers
	m_MicInputBuffer.mNumberBuffers = 1;
	m_MicInputBuffer.mBuffers[0].mDataByteSize = m_nBufferFrameSize*audioDeviceFormat.mBitsPerChannel/8*audioDeviceFormat.mChannelsPerFrame;
	m_MicInputBuffer.mBuffers[0].mData = malloc( m_MicInputBuffer.mBuffers[0].mDataByteSize );
	m_MicInputBuffer.mBuffers[0].mNumberChannels = audioDeviceFormat.mChannelsPerFrame;
	m_pMicInputBuffer = m_MicInputBuffer.mBuffers[0].mData;
	m_MicInputBufferSize = m_MicInputBuffer.mBuffers[0].mDataByteSize;
	
	m_InputBytesPerPacket = audioDeviceFormat.mBytesPerPacket;

	m_ConverterBuffer.mNumberBuffers = 1;
	m_ConverterBuffer.mBuffers[0].mDataByteSize = m_nBufferFrameSize*audioOutputFormat.mBitsPerChannel/8*audioOutputFormat.mChannelsPerFrame;
	m_ConverterBuffer.mBuffers[0].mData = malloc( m_MicInputBuffer.mBuffers[0].mDataByteSize );
	m_ConverterBuffer.mBuffers[0].mNumberChannels = 1;
	
	m_ConverterBufferSize = m_ConverterBuffer.mBuffers[0].mDataByteSize;
	
	m_nMicInputSamplesAvaialble = 0;
	
	
	m_SampleBufferReadPos = m_SampleBufferWritePos = 0;
	m_SampleBufferSize = ceil( kNumSecAudioBuffer * m_nSampleRate * audioOutputFormat.mBytesPerPacket );
	m_SampleBuffer = (char *)malloc( m_SampleBufferSize ); 
	memset( m_SampleBuffer, 0x0, m_SampleBufferSize );

	DevMsg( "Initialized AudioQueue record interface\n" );
	return true;
}

bool VoiceRecord_AudioQueue::Init( int nSampleRate )
{
	if ( m_AudioUnit && m_nSampleRate != nSampleRate )
	{
		// Need to recreate interfaces with different sample rate
		ReleaseInterfaces();
		ClearInterfaces();
	}
	m_nSampleRate = nSampleRate;

	// Re-initialize the capture buffer if neccesary
	if ( !m_AudioUnit )
	{
		InitalizeInterfaces();
	}

	m_SampleBufferReadPos = m_SampleBufferWritePos = 0;

	//printf( "VoiceRecord_AudioQueue::Init()\n" );
	// Initialise
	OSStatus status = AudioUnitInitialize( m_AudioUnit );
	if ( status != noErr )
		return false;

	return true;
}


void VoiceRecord_AudioQueue::ReleaseInterfaces()
{
	AudioOutputUnitStop( m_AudioUnit );
	AudioConverterDispose( m_Converter );
	AudioUnitUninitialize( m_AudioUnit );
	m_AudioUnit = NULL;
	m_Converter = NULL;
}


void VoiceRecord_AudioQueue::ClearInterfaces()
{
	m_AudioUnit = NULL;
	m_Converter = NULL;
	m_SampleBufferReadPos = m_SampleBufferWritePos = 0;
	if ( m_SampleBuffer )
		free( m_SampleBuffer );
	m_SampleBuffer = NULL;

	if ( m_MicInputBuffer.mBuffers[0].mData )
		free( m_MicInputBuffer.mBuffers[0].mData );
	if ( m_ConverterBuffer.mBuffers[0].mData )
		free( m_ConverterBuffer.mBuffers[0].mData );
	m_MicInputBuffer.mBuffers[0].mData = NULL;
	m_ConverterBuffer.mBuffers[0].mData = NULL;
}


void VoiceRecord_AudioQueue::Idle()
{
}


int VoiceRecord_AudioQueue::GetRecordedData(short *pOut, int nSamples )
{
	if ( !m_SampleBuffer )
		return 0;
		
	int cbSamples = nSamples*2; // convert to bytes
	int writePos = m_SampleBufferWritePos;
	int readPos = m_SampleBufferReadPos;
	
	int nOutstandingSamples = ( writePos - readPos );
	if ( readPos > writePos ) // writing has wrapped around
	{
		nOutstandingSamples = writePos + ( m_SampleBufferSize - readPos );
	}
	
	if ( !nOutstandingSamples ) 
		return 0;

	if ( nOutstandingSamples < cbSamples )
		cbSamples = nOutstandingSamples; // clamp to the number of samples we have available
	
	memcpy( (char *)pOut, m_SampleBuffer + readPos, MIN( cbSamples, m_SampleBufferSize - readPos ) );
	if ( cbSamples > ( m_SampleBufferSize - readPos ) )
	{
		int offset = m_SampleBufferSize - readPos;
		cbSamples -= offset;
		readPos = 0;
		memcpy( (char *)pOut + offset, m_SampleBuffer, cbSamples );
	}
	readPos+=cbSamples;
	m_SampleBufferReadPos = readPos%m_SampleBufferSize;
	//printf( "Returning %d samples, %d %d (%d)\n", cbSamples/2, (int)m_SampleBufferReadPos, (int)m_SampleBufferWritePos, m_SampleBufferSize );
	return cbSamples/2;
}


VoiceRecord_AudioQueue g_AudioQueueVoiceRecord;
IVoiceRecord* CreateVoiceRecord_AudioQueue( int sampleRate )
{
	if ( g_AudioQueueVoiceRecord.Init( sampleRate ) )
	{
		return &g_AudioQueueVoiceRecord;
	}
	else
	{
		g_AudioQueueVoiceRecord.Release();
		return NULL;
	}
}
