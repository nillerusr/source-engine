//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Defines the entry point for the console application
//
//===============================================================================

//#include <sys/stat.h>
//#include <time.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h >
#include "tier1/utlvector.h"
#include "tier0/icommandline.h"
#include "tier2/tier2.h"
#include "tier2/fileutils.h"
#include "../../dx9sdk/include/d3d9.h"
#include "../../dx9sdk/include/d3dx9.h"

#define DEFAULT_PORT			"20000"
#define DEFAULT_SEND_BUF_LEN	40000
#define DEFAULT_RECV_BUF_LEN	40000

char g_pPathBase[MAX_PATH];
bool g_bPrintDisassembly;

// This guy just spins and compiles when a command comes in from the game
void ServerThread( void * )
{
	WSADATA wsaData;
	if( WSAStartup( 0x101, &wsaData ) != 0 )
		return;

	struct addrinfo *result = NULL, hints;

	ZeroMemory( &hints, sizeof(hints) );
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	int nResult = getaddrinfo( NULL, DEFAULT_PORT, &hints, &result );
	if ( nResult != 0 )
	{
		printf( "getaddrinfo failed: %d\n", nResult );
		WSACleanup();
		return;
	}

	// Create a SOCKET for connecting to server
	SOCKET ListenSocket = socket( result->ai_family, result->ai_socktype, result->ai_protocol );
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return;
	}

	// Setup the TCP listening socket
	nResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen );
	if (nResult == SOCKET_ERROR)
	{
		printf("bind failed: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}

	freeaddrinfo(result);


	nResult = listen( ListenSocket, SOMAXCONN );
	if ( nResult == SOCKET_ERROR )
	{
		printf( "listen failed: %d\n", WSAGetLastError() );
		closesocket( ListenSocket );
		WSACleanup();
		return;
	}

	printf( "Waiting for initial connection...\n" );

	// Accept a client socket
	SOCKET ClientSocket = accept( ListenSocket, NULL, NULL );
	if ( ClientSocket == INVALID_SOCKET )
	{
		printf( "accept failed: %d\n", WSAGetLastError() );
		closesocket( ListenSocket );
		WSACleanup();
		return;
	}

	// First connection
	printf( "Game connected\n" );

	char pRecbuf[DEFAULT_RECV_BUF_LEN];		// Text in command file from game
	uint32 pSendbuf[DEFAULT_SEND_BUF_LEN];	// Error string or binary shader blob in reply

	while ( true )
	{
		nResult = recv( ClientSocket, pRecbuf, DEFAULT_RECV_BUF_LEN, 0 );

		if ( nResult > 0 )
		{
			char *pShaderFilename = strtok ( pRecbuf, "\n");
			char pFullFilename[MAX_PATH];

			// If we took in a path on the commandline, we concatenate the incoming filenames with it
			if ( V_strlen( g_pPathBase ) > 0 )
			{
				V_strncpy( pFullFilename, g_pPathBase, MAX_PATH ); // base path
				V_strncat( pFullFilename, pShaderFilename, MAX_PATH );
				pShaderFilename = pFullFilename;
			}
			char *pShaderModel = strtok ( NULL, "\n");
			int nSendBufLen = 0;

			// Only try to compile if we have a recognized profile
			if ( !stricmp( pShaderModel, "vs_2_0" ) || !stricmp( pShaderModel, "ps_2_0" ) || !stricmp( pShaderModel, "ps_2_b" ) )
			{
				char *pNumMacros = strtok ( NULL, "\n");
				int nNumMacros = atoi( pNumMacros );

				// Read macros from the command file
				CUtlVector<D3DXMACRO> macros;
				D3DXMACRO macro;
				for ( int i=0; i<nNumMacros-1; i++ ) // The last one is the (null) one, so don't bother reading it
				{
					// Allocate and populate strings
					macro.Name = strtok( NULL, "\n");
					macro.Definition = strtok( NULL, "\n");
					macros.AddToTail( macro );
				}

				// Null macro at the end
				macro.Name = NULL;
				macro.Definition = NULL;
				macros.AddToTail( macro );

				LPD3DXBUFFER pShader, pErrorMessages;

				// This is the shader compiler we use for pre-ps30 shaders.
				// This utility needs to change if we want to do ps30 shaders (see logic in vertexshaderdx8.cpp)
				HRESULT hr = D3DXCompileShaderFromFile( pShaderFilename, macros.Base(), NULL /* LPD3DXINCLUDE */, "main",
														pShaderModel, 0, &pShader, &pErrorMessages,
														NULL /* LPD3DXCONSTANTTABLE *ppConstantTable */ );
				if ( hr != D3D_OK )
				{
					pSendbuf[0] = 0;
					
					printf( "Compilation error in %s\n", pShaderFilename );

					if ( pErrorMessages )
					{
						memcpy( pSendbuf+1, pErrorMessages->GetBufferPointer(), pErrorMessages->GetBufferSize() ); // Null-terminated string
						printf("%s\n", (const char*)(pSendbuf + 1 ));
						nSendBufLen = pErrorMessages->GetBufferSize() + 4; // account for uint32 at front of the buffer
					}
					else
					{
						((uint8*)(pSendbuf+1))[0] = '?';
						((uint8*)(pSendbuf+1))[1] = '\0';
						nSendBufLen = 2 + 4; // account for uint32 at front of the buffer
					}
					
					
				}
				else  // Success!
				{
//					printf( "Compiled %s\n", pShaderFilename );
					pSendbuf[0] = pShader->GetBufferSize();
					memcpy( pSendbuf+1, pShader->GetBufferPointer(), pShader->GetBufferSize() );
					nSendBufLen = pShader->GetBufferSize() + 4; // account for uint32 at front of the buffer

					if ( g_bPrintDisassembly )
					{
						printf( "Filename: %s\n", pShaderFilename );
						printf( "Shader model: %s\n", pShaderModel );
						printf( "Macros: " );
						for ( int i = 0; i < nNumMacros - 1; i++ )
							printf( "  %s\n", macros[i].Name );
						LPD3DXBUFFER pDisassembly = NULL;
						D3DXDisassembleShader( (const DWORD*)pShader->GetBufferPointer(), FALSE, "", &pDisassembly );
						if ( pDisassembly )
						{
							printf( "Disassembled shader:\n");
							puts( (const char*)pDisassembly->GetBufferPointer() );
							printf("\n");

							pDisassembly->Release();
						}
					}
				}

				if (pErrorMessages)
				{
					pErrorMessages->Release();
				}

				if (pShader)
				{
					pShader->Release();
				}

				// Purge the macro buffer
				macros.RemoveMultipleFromTail( nNumMacros );
			}
			else // Not a supported shader profile
			{
				pSendbuf[0] = 0;
				char *pCharSendbuff = (char *) (pSendbuf+1);
				V_snprintf( pCharSendbuff, DEFAULT_SEND_BUF_LEN, "Unsupported shader profile: %s\n", pShaderModel );
				nSendBufLen = strlen( pCharSendbuff ) + 4; // account for uint32 at front of the buffer
			}

			// Send the compiled shader back to the game
			int nSendResult = send( ClientSocket, (const char *)pSendbuf, nSendBufLen, 0 );
			if ( nSendResult == SOCKET_ERROR )
			{
				printf( "send failed: %d\n", WSAGetLastError() );
				closesocket( ClientSocket );
				WSACleanup();
				return;
			}
		}
		else // We had a game talking to us but it went away
		{
			printf( "Game went away, waiting for new connection...\n" );

			// Block again waiting to accept a connection
			ClientSocket = accept( ListenSocket, NULL, NULL );

			printf( "Game connected\n" );

			if ( ClientSocket == INVALID_SOCKET )
			{
				printf( "accept failed: %d\n", WSAGetLastError() );
				Assert( 0 );
				closesocket( ListenSocket );
				WSACleanup();
				return;
			}
		}
	}

	nResult = shutdown( ClientSocket, SD_SEND );
	if ( nResult == SOCKET_ERROR )
	{
		printf("shutdown failed: %d\n", WSAGetLastError());
		closesocket( ClientSocket );
		WSACleanup();
		return;
	}

	// cleanup
	closesocket( ClientSocket );
	WSACleanup();
}


void CheckPath( char *pPath )
{
	int len = V_strlen( pPath );

	// If we don't have a path separator at the end of the path, put one there
	if ( ( pPath[len-1] != '\\' ) && ( pPath[len-1] != '/' ) )
	{
		V_strncat( pPath, CORRECT_PATH_SEPARATOR_S, MAX_PATH );
	}
}


int main(int argc, char* argv[])
{
	if ( argc < 2 )
	{
		printf( "============================================================\n" );
		printf( " Please provide full path to shader directory.  For example:\n" );
		printf( "    U:\\piston\\staging\\src\\materialsystem\\stdshaders\\ \n");
		printf( "============================================================\n" );
		printf( " remoteshadercompiler will now exit!!! \n" );
		printf( "============================================================\n" );
		return 0;
	}

	printf( "========================================================\n");
	printf( "Remote shader compiler is running.  Press ESCAPE to exit\n" );
	printf( "========================================================\n");

	// If we have a path specified on the commandline, we expect
	// that the remote machine is going to send base filenames only
	// and that we'll want to strcat this path onto the filename from the worker.
	//
	// For example, if you have your shader source on your Windows machine, you can use something like this:
	//
	//   U:\piston\staging\src\materialsystem\stdshaders\
	//
	strcpy(	g_pPathBase, argv[1] );

	if ( argc == 3 )
	{
		g_bPrintDisassembly = true;
	}

	CheckPath( g_pPathBase );

	// Kick off compile server thread
	_beginthread( ServerThread, 0, NULL );

	// Spin until escape
	while( _getch() != 27 )
		;

	return 0;
}
