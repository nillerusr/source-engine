//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Unit test program for CommandBuffer
//
// $NoKeywords: $
//=============================================================================//

#include "unitlib/unitlib.h"
#include "tier1/commandbuffer.h"
#include "tier1/strtools.h"


DEFINE_TESTSUITE( CommandBufferTestSuite )

DEFINE_TESTCASE( CommandBufferTestSimple, CommandBufferTestSuite )
{
	Msg( "Simple command buffer test...\n" );

	CCommandBuffer buffer;
	buffer.AddText( "test_command test_arg1 test_arg2" );
	buffer.AddText( "test_command2 test_arg3; test_command3 test_arg4" );
	buffer.AddText( "test_command4\ntest_command5" );
	buffer.AddText( "test_command6 // Comment; test_command7" );
	buffer.AddText( "test_command8 // Comment; test_command9\ntest_command10" );
	buffer.AddText( "test_command11 \"test_arg5 test_arg6\"" );
	buffer.AddText( "test_command12 \"\"" );
	buffer.AddText( "// Comment\ntest_command13\t\t\"test_arg7\"" );
	buffer.AddText( "test_command14\"test_arg8\"test_arg9" );
	buffer.AddText( "test_command15 test_arg10" );
	buffer.AddText( "test_command16 test_arg11:test_arg12" );

	int argc;
	const char **argv;
	buffer.BeginProcessingCommands( 1 );
								   
	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 3 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg1" ) );
	Shipping_Assert( !Q_stricmp( argv[2], "test_arg2" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "test_arg1 test_arg2" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 2 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command2" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg3" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "test_arg3" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 2 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command3" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg4" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "test_arg4" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 1 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command4" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 1 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command5" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 1 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command6" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 1 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command8" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 1 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command10" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 2 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command11" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg5 test_arg6" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "\"test_arg5 test_arg6\"" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 2 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command12" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "\"\"" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 2 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command13" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg7" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "\"test_arg7\"" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 3 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command14" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg8" ) );
	Shipping_Assert( !Q_stricmp( argv[2], "test_arg9" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "\"test_arg8\"test_arg9" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 2 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command15" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg10" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "test_arg10" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 4 );
	Shipping_Assert( !Q_stricmp( argv[0], "test_command16" ) );
	Shipping_Assert( !Q_stricmp( argv[1], "test_arg11" ) );
	Shipping_Assert( !Q_stricmp( argv[2], ":" ) );
	Shipping_Assert( !Q_stricmp( argv[3], "test_arg12" ) );
	Shipping_Assert( !Q_stricmp( buffer.ArgS(), "test_arg11:test_arg12" ) );

	argc = buffer.DequeueNextCommand( argv );
	Shipping_Assert( argc == 0 );

	buffer.EndProcessingCommands( );
}


DEFINE_TESTCASE( CommandBufferTestTiming, CommandBufferTestSuite )
{
	Msg( "Delayed command buffer test...\n" );

	CCommandBuffer buffer;

	buffer.AddText( "test_command test_arg1 test_arg2" );
	buffer.AddText( "test_command2 test_arg1 test_arg2 test_arg3", 1 );
	buffer.AddText( "test_command3;wait;test_command4;wait 2;test_command5" );

	int argc;
	const char **argv;
	{
		buffer.BeginProcessingCommands( 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 3 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( argv[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( argv[2], "test_arg2" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 1 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command3" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 4 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command2" ) );
		Shipping_Assert( !Q_stricmp( argv[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( argv[2], "test_arg2" ) );
		Shipping_Assert( !Q_stricmp( argv[3], "test_arg3" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 1 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command4" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 1 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command5" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
}


DEFINE_TESTCASE( CommandBufferTestNested, CommandBufferTestSuite )
{
	Msg( "Nested command buffer test...\n" );

	CCommandBuffer buffer;
	buffer.AddText( "test_command test_arg1 test_arg2" );
	buffer.AddText( "test_command2 test_arg3 test_arg4 test_arg5", 2 );

	int argc;
	const char **argv;
	{
		buffer.BeginProcessingCommands( 2 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 3 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( argv[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( argv[2], "test_arg2" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.AddText( "test_command3;test_command4", 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 1 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command3" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 1 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command4" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 4 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command2" ) );
		Shipping_Assert( !Q_stricmp( argv[1], "test_arg3" ) );
		Shipping_Assert( !Q_stricmp( argv[2], "test_arg4" ) );
		Shipping_Assert( !Q_stricmp( argv[3], "test_arg5" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
}


DEFINE_TESTCASE( CommandBufferTestOverflow, CommandBufferTestSuite )
{
	Msg( "Command buffer overflow test...\n" );

	CCommandBuffer buffer;

	buffer.LimitArgumentBufferSize( 40 );
	bool bOk = buffer.AddText( "test_command test_arg1 test_arg2" );	// 32 chars
	Shipping_Assert( bOk );
	bOk = buffer.AddText( "test_command2 test_arg3 test_arg4 test_arg5", 2 );	// 43 chars
	Shipping_Assert( !bOk );

	int argc;
	const char **argv;
	{
		buffer.BeginProcessingCommands( 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 3 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( argv[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( argv[2], "test_arg2" ) );

		bOk = buffer.AddText( "test_command3 test_arg6;wait;test_command4" );
		Shipping_Assert( bOk );

		// This makes sure that AddText doesn't cause argv to become bogus after
		// compacting memory
		Shipping_Assert( !Q_stricmp( argv[0], "test_command" ) );
		Shipping_Assert( !Q_stricmp( argv[1], "test_arg1" ) );
		Shipping_Assert( !Q_stricmp( argv[2], "test_arg2" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 2 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command3" ) );
		Shipping_Assert( !Q_stricmp( argv[1], "test_arg6" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
	{
		buffer.BeginProcessingCommands( 1 );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 1 );
		Shipping_Assert( !Q_stricmp( argv[0], "test_command4" ) );

		argc = buffer.DequeueNextCommand( argv );
		Shipping_Assert( argc == 0 );

		buffer.EndProcessingCommands( );
	}
}

