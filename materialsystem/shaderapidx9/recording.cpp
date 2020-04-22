//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "togl/rendermechanism.h"
#include "recording.h"
#include "shaderapi/ishaderutil.h"
#include "materialsystem/imaterialsystem.h"
#include "shaderapidx8_global.h"
#include "utlvector.h"
#include <stdio.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef RECORDING

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

static CUtlVector<unsigned char> g_pRecordingBuffer;
static int g_ArgsRemaining = 0;
static int g_CommandStartIdx = 0;

//-----------------------------------------------------------------------------
// Opens the recording file
//-----------------------------------------------------------------------------

static FILE* OpenRecordingFile()
{
#ifdef CRASH_RECORDING
	static FILE *fp = 0;
#else
	FILE* fp = 0;
#endif
	static bool g_CantOpenFile = false;
	static bool g_NeverOpened = true;
	if (!g_CantOpenFile)
	{
#ifdef CRASH_RECORDING
		if( g_NeverOpened )
		{
			fp = fopen( "shaderdx8.rec", "wbc" );
		}
#else
		fp = fopen( "shaderdx8.rec", g_NeverOpened ? "wb" : "ab" );
#endif
		if (!fp)
		{
			Warning("Unable to open recording file shaderdx8.rec!\n");
			g_CantOpenFile = true;			
		}
		g_NeverOpened = false;
	}
	return fp;
}

//-----------------------------------------------------------------------------
// Writes to the recording file
//-----------------------------------------------------------------------------

#define COMMAND_BUFFER_SIZE 32768

static void WriteRecordingFile()
{
	// Store the command size
	*(int*)&g_pRecordingBuffer[g_CommandStartIdx] = 
		g_pRecordingBuffer.Size() - g_CommandStartIdx;

#ifndef CRASH_RECORDING
	// When not crash recording, flush when buffer gets too big, 
	// or when Present() is called
	if ((g_pRecordingBuffer.Size() < COMMAND_BUFFER_SIZE) &&
		(g_pRecordingBuffer[g_CommandStartIdx+4] != DX8_PRESENT))
		return;
#endif

	FILE* fp = OpenRecordingFile();
	if (fp)
	{
		// store the command size
		fwrite( g_pRecordingBuffer.Base(), 1, g_pRecordingBuffer.Size(), fp );
		fflush( fp );
#ifndef CRASH_RECORDING
		fclose( fp );
#endif
	}

	g_pRecordingBuffer.RemoveAll();
}

// Write the buffered crap out on shutdown.
void FinishRecording()
{
#ifndef CRASH_RECORDING
	FILE* fp = OpenRecordingFile();
	if (fp)
	{
		// store the command size
		fwrite( g_pRecordingBuffer.Base(), 1, g_pRecordingBuffer.Size(), fp );
		fflush( fp );
	}

	g_pRecordingBuffer.RemoveAll();
#endif
}

// set this to true in the debugger to actually record commands.
static bool g_bDoRecord = true;

//-----------------------------------------------------------------------------
// Records a command
//-----------------------------------------------------------------------------

void RecordCommand( RecordingCommands_t cmd, int numargs )
{
	if( !g_bDoRecord )
	{
		return;
	}
	Assert( g_ArgsRemaining == 0 );

	g_CommandStartIdx = g_pRecordingBuffer.AddMultipleToTail( 6 );

	// save space for the total command size
	g_pRecordingBuffer[g_CommandStartIdx+4] = cmd;
	g_pRecordingBuffer[g_CommandStartIdx+5] = numargs;
	g_ArgsRemaining = numargs;
	if (g_ArgsRemaining == 0)
		WriteRecordingFile();
}

//-----------------------------------------------------------------------------
// Records an argument for a command, flushes when the command is done
//-----------------------------------------------------------------------------

void RecordArgument( void const* pMemory, int size )
{
	if( !g_bDoRecord )
	{
		return;
	}
	Assert( g_ArgsRemaining > 0 );
	int tail = g_pRecordingBuffer.Size();
	g_pRecordingBuffer.AddMultipleToTail( size );
	memcpy( &g_pRecordingBuffer[tail], pMemory, size );
	if (--g_ArgsRemaining == 0)
		WriteRecordingFile();
}


#endif // RECORDING
