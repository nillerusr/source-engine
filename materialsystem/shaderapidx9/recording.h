//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef RECORDING_H
#define RECORDING_H
#pragma once

//-----------------------------------------------------------------------------
// Use this to put us into a 'recording' mode
//-----------------------------------------------------------------------------

//#define RECORDING

//-----------------------------------------------------------------------------
// Uncomment these to record special frames in the recording
// that reset the entire render state.
//-----------------------------------------------------------------------------

//#define RECORD_KEYFRAMES 1
#define KEYFRAME_INTERVAL 100	// number of actual frames between each keyframe
 
//-----------------------------------------------------------------------------
// Use this to allow us to record crashes (write every command immediately)
//-----------------------------------------------------------------------------

//#define CRASH_RECORDING
 
//-----------------------------------------------------------------------------
// Use this to record textures (checkboards are used for textures otherwise)
//-----------------------------------------------------------------------------

#define RECORD_TEXTURES

//-----------------------------------------------------------------------------
// Use this to record debug strings . .these are only useful if you are doing "playback -list"
//-----------------------------------------------------------------------------

#define RECORD_DEBUG_STRINGS

//-----------------------------------------------------------------------------
// Recording state, if you change this, change the table in playback/playback.cpp
//-----------------------------------------------------------------------------

enum RecordingCommands_t
{
	DX8_CREATE_DEVICE = 0,
	DX8_DESTROY_DEVICE,
	DX8_RESET,
	DX8_SHOW_CURSOR,
	DX8_BEGIN_SCENE,
	DX8_END_SCENE,
	DX8_PRESENT,
	DX8_CREATE_TEXTURE,
	DX8_DESTROY_TEXTURE,
	DX8_SET_TEXTURE,
	DX8_SET_TRANSFORM,
	DX8_CREATE_VERTEX_SHADER,
	DX8_CREATE_PIXEL_SHADER,
	DX8_DESTROY_VERTEX_SHADER,
	DX8_DESTROY_PIXEL_SHADER,
	DX8_SET_VERTEX_SHADER,
	DX8_SET_PIXEL_SHADER,
	DX8_SET_VERTEX_SHADER_CONSTANT,
	DX8_SET_PIXEL_SHADER_CONSTANT,
	DX8_SET_MATERIAL,
	DX8_LIGHT_ENABLE,
	DX8_SET_LIGHT,
	DX8_SET_VIEWPORT,
	DX8_CLEAR,
	DX8_VALIDATE_DEVICE,
	DX8_SET_RENDER_STATE,
	DX8_SET_TEXTURE_STAGE_STATE,

	DX8_CREATE_VERTEX_BUFFER,
	DX8_DESTROY_VERTEX_BUFFER,
	DX8_LOCK_VERTEX_BUFFER,
	DX8_VERTEX_DATA,
	DX8_UNLOCK_VERTEX_BUFFER,

	DX8_CREATE_INDEX_BUFFER,
	DX8_DESTROY_INDEX_BUFFER,
	DX8_LOCK_INDEX_BUFFER,
	DX8_INDEX_DATA,
	DX8_UNLOCK_INDEX_BUFFER,

	DX8_SET_STREAM_SOURCE,
	DX8_SET_INDICES,
	DX8_DRAW_PRIMITIVE,
	DX8_DRAW_INDEXED_PRIMITIVE,

	DX8_LOCK_TEXTURE,
	DX8_UNLOCK_TEXTURE,

	DX8_KEYFRAME,	// isn't actually a dx8 command, used to help find particular frames

	DX8_SET_TEXTURE_DATA,
	DX8_BLIT_TEXTURE_BITS,
	
	DX8_GET_DEVICE_CAPS,
	DX8_GET_ADAPTER_IDENTIFIER,

	DX8_HARDWARE_SYNC,

	DX8_COPY_FRAMEBUFFER_TO_TEXTURE,
	DX8_DEBUG_STRING,
	DX8_CREATE_DEPTH_TEXTURE,
	DX8_DESTROY_DEPTH_TEXTURE,
	DX8_SET_RENDER_TARGET,

	DX8_TEST_COOPERATIVE_LEVEL,
	
	DX8_SET_VERTEX_BUFFER_FORMAT, // isn't actually a dx8 command. . let's playback know what format a buffer is for listing info
	
	DX8_SET_SAMPLER_STATE,
	DX8_SET_VERTEX_DECLARATION,
	DX8_CREATE_VERTEX_DECLARATION,
	DX8_SET_FVF,
	DX8_SET_CLIP_PLANE,

	DX8_SYNC_TOKEN,

	DX8_LOCK_VERTEX_TEXTURE,
	DX8_UNLOCK_VERTEX_TEXTURE,

	DX8_SET_SCISSOR_RECT,

	DX8_NUM_RECORDING_COMMANDS
};

#ifdef RECORDING

void RecordCommand( RecordingCommands_t cmd, int numargs );
void RecordArgument( void const* pMemory, int size );
void FinishRecording( void );

inline void RecordInt( int i )
{
	RecordArgument( &i, sizeof(int) );
}

inline void RecordFloat( float f )
{
	RecordArgument( &f, sizeof(float) );
}

#	define RECORD_COMMAND( _cmd, _numargs )		RecordCommand( _cmd, _numargs )
#	define RECORD_INT( _int )					RecordInt( _int )
#	define RECORD_FLOAT( _float )				RecordFloat( _float )
#	define RECORD_STRING( _string )				RecordArgument( _string, strlen(_string) + 1 )
#	define RECORD_STRUCT( _struct, _size )		RecordArgument( _struct, _size )

#	define RECORD_RENDER_STATE( _state, _val )		\
		RECORD_COMMAND( DX8_SET_RENDER_STATE, 2 );	\
		RECORD_INT( _state );						\
		RECORD_INT( _val )

#	define RECORD_TEXTURE_STAGE_STATE( _stage, _state, _val )	\
		RECORD_COMMAND( DX8_SET_TEXTURE_STAGE_STATE, 3 );		\
		RECORD_INT( _stage );									\
		RECORD_INT( _state );									\
		RECORD_INT( _val )

#	define RECORD_SAMPLER_STATE( _stage, _state, _val )	\
		RECORD_COMMAND( DX8_SET_SAMPLER_STATE, 3 );		\
		RECORD_INT( _stage );									\
		RECORD_INT( _state );									\
		RECORD_INT( _val )

#	ifdef RECORD_DEBUG_STRINGS
#		define RECORD_DEBUG_STRING( _str )			\
			RECORD_COMMAND( DX8_DEBUG_STRING, 1 );	\
			RECORD_STRING( _str )
#	else
#		define RECORD_DEBUG_STRING( _str )			0
#	endif

#else // not RECORDING

#	undef RECORD_TEXTURES

#	define RECORD_COMMAND( _cmd, _numargs )		0
#	define RECORD_INT( _int )					0
#	define RECORD_FLOAT( _float )				0
#	define RECORD_STRING( _string )				0
#	define RECORD_STRUCT( _struct, _size )		0
#	define RECORD_RENDER_STATE( _state, _val )	0
#	define RECORD_TEXTURE_STAGE_STATE( _stage, _state, _val )	0
#	define RECORD_SAMPLER_STATE( _stage, _state, _val )	0
#	define RECORD_DEBUG_STRING( _str )			0

#endif // RECORDING

#endif // RECORDING_H
