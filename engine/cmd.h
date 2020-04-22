//========= Copyright Valve Corporation, All rights reserved. ============//
//
// cmd.h -- Command buffer and command execution
// Any number of commands can be added in a frame, from several different sources.
// Most commands come from either keybindings or console line input, but remote
// servers can also send across commands and entire text files can be execed.
// 
// The + command line options are also added to the command buffer.
// 
// The game starts with a Cbuf_AddText ("exec quake.rc\n"); Cbuf_Execute ();
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef CMD_H
#define CMD_H

#ifdef _WIN32
#pragma once
#endif


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CCommand;
class ConCommandBase;

#define MAX_EXECUTION_MARKERS 2048

typedef enum
{
	eCmdExecutionMarker_Enable_FCVAR_SERVER_CAN_EXECUTE='a',
	eCmdExecutionMarker_Disable_FCVAR_SERVER_CAN_EXECUTE='b',
	
	eCmdExecutionMarker_Enable_FCVAR_CLIENTCMD_CAN_EXECUTE='c',
	eCmdExecutionMarker_Disable_FCVAR_CLIENTCMD_CAN_EXECUTE='d'
} ECmdExecutionMarker;


//-----------------------------------------------------------------------------
// Initialization, shutdown of the command buffer
//-----------------------------------------------------------------------------
void Cbuf_Init (void);
void Cbuf_Shutdown( void );


//-----------------------------------------------------------------------------
// Clears the command buffer
//-----------------------------------------------------------------------------
void Cbuf_Clear(void);

//-----------------------------------------------------------------------------
// Escape an argument for a command. This *can* fail as many characters cannot
// actually be passed through the old command syntax...
//-----------------------------------------------------------------------------
bool Cbuf_EscapeCommandArg( const char *pText, char *pOut, unsigned int nOut );

//-----------------------------------------------------------------------------
// as new commands are generated from the console or keybindings,
// the text is added to the end of the command buffer.
//-----------------------------------------------------------------------------
void Cbuf_AddText (const char *text);


//-----------------------------------------------------------------------------
// when a command wants to issue other commands immediately, the text is
// inserted at the beginning of the buffer, before any remaining unexecuted
// commands.
//-----------------------------------------------------------------------------
void Cbuf_InsertText( const char *text );


//-----------------------------------------------------------------------------
// Surround a command with two execution markers. The operation is performed atomically.
//
// These allow you to create blocks in the command stream where certain rules apply.
// ONLY use Cbuf_AddText in between execution markers. If you use Cbuf_InsertText,
// it will put that stuff before the execution marker and the execution marker won't apply.
//
// cl_restrict_server_commands uses this. It inserts a marker that says, "don't run 
// anything unless it's marked with FCVAR_SERVER_CAN_EXECUTE", then inserts some commands,
// then removes the execution marker. That way, ANYTIME Cbuf_Execute() is called, 
// it will apply the cl_restrict_server_commands rules correctly.
//-----------------------------------------------------------------------------
bool Cbuf_AddTextWithMarkers( ECmdExecutionMarker markerLeft, const char *text, ECmdExecutionMarker markerRight );


// Returns whether or not the execution marker stack has room for N more.
bool Cbuf_HasRoomForExecutionMarkers( int cExecutionMarkers );


// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function!
//-----------------------------------------------------------------------------
void Cbuf_Execute();


//===========================================================================

/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

Commands can come from three sources, but the handler functions may choose
to dissallow the action or forward it to a remote server if the source is
not apropriate.

*/

enum cmd_source_t
{
	src_client,		// came in over a net connection as a clc_stringcmd
					// host_client will be valid during this state.
	src_command		// from the command buffer
};


// FIXME: Move these into a field of CCommand?
extern cmd_source_t cmd_source;
extern int			cmd_clientslot;


//-----------------------------------------------------------------------------
// Initialization, shutdown
//-----------------------------------------------------------------------------
void Cmd_Init (void);
void Cmd_Shutdown( void );


//-----------------------------------------------------------------------------
// Executes a command given a CCommand argument structure
//-----------------------------------------------------------------------------
const ConCommandBase *Cmd_ExecuteCommand( const CCommand &command, cmd_source_t src, int nClientSlot = -1 );


//-----------------------------------------------------------------------------
// Dispatches a command with the requested arguments
//-----------------------------------------------------------------------------
void Cmd_Dispatch( const ConCommandBase *pCommand, const CCommand &args );


//-----------------------------------------------------------------------------
// adds the current command line as a clc_stringcmd to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.
// If bReliable is true, it goes into cls.netchan.message.
// If bReliable is false, it goes into cls.datagram.
//-----------------------------------------------------------------------------
void Cmd_ForwardToServer( const CCommand &args, bool bReliable = true );



// This is a list of cvars that are in the client DLL that we want FCVAR_CLIENTCMD_CAN_EXECUTE set on.
// In order to avoid patching the client DLL, we setup this list. Whenever the client DLL has gone out with the
// FCVAR_CLIENTCMD_CAN_EXECUTE flag set, we can get rid of this list.
void Cmd_AddClientCmdCanExecuteVar( const char *pName );

// Used to allow cheats even if cheats aren't theoretically allowed
void Cmd_SetRptActive( bool bActive );
bool Cmd_IsRptActive();


#endif // CMD_H
