/*
 * $History: DK2WIN32.H $
 * 
 * *****************  Version 1  *****************
 * User: Alun         Date: 1/07/99    Time: 11:31
 * Created in $/DK2/Software/C Drivers/Windows/API/DLL/DK2Win32
 * Initial version added to Source Safe version control
 */
#ifdef __cplusplus
extern "C"
 {
#endif

/////////////////////////////////////////////////////////////////
// Error Codes
//
// All codes are returned from DK2GetLastError and can be formated
// into a message string by calling DK2FormatError

/////////////////////////////////////////////////////////////////
// DK2ERR_SUCCESS
//
// The command was successfull

#define DK2ERR_SUCCESS                 0x0000


/////////////////////////////////////////////////////////////////
// DK2ERR_TOOMANUUSERS
//
// One or mode DK2 network servers were found but they were all 
// full.

#define DK2ERR_TOOMANYUSERS            0x0001

/////////////////////////////////////////////////////////////////
// DK2ERR_ACCESS_DENIED
//
// A DK2 network servers were found but access was denied, either
// due to user restrictions, or invalid login memory.
// See FindDK2Ex

#define DK2ERR_ACCESS_DENIED           0x0002


/////////////////////////////////////////////////////////////////
// DK2ERR_DESKEY_NOTFOUND
//
// A DK2 command failed because a DK2 was not found, either 
// locally or accross the network

#define DK2ERR_DESKEY_NOTFOUND         0x0003

/////////////////////////////////////////////////////////////////
// DK2ERR_NORESPONSE
//
// A DK2 oommand to a server failed becase the server did not 
// respond.

#define DK2ERR_NORESPONSE              0x0004

/////////////////////////////////////////////////////////////////
// DK2ERR_NOSERVERS
//
// The drivers searched for a DK2 a network server, but none were
// found.

#define DK2ERR_NOSERVERS               0x0005

/////////////////////////////////////////////////////////////////
// DK2ERR_DRIVERNOTINSTALLED
//
// The DK2 drivers are not installed

#define DK2ERR_DRIVERNOTINSTALLED      0x0006

/////////////////////////////////////////////////////////////////
// DK2ERR_COMMANDNOTSUPPORTED
//
// The DK2 does not support the requested command   

#define DK2ERR_COMMANDNOTSUPPORTED     0x0007

/////////////////////////////////////////////////////////////////
// DK2ERR_ALREADYNETWORK
//
// A local DK2 command failed because there is a server runnning
// the command must be carried out over the network.

#define DK2ERR_ALREADYNETWORK          0x1001

/////////////////////////////////////////////////////////////////
// DK2ERR_COMMANDNOTNETWORK
//
// A DK2 command failed because the command cannot operate over
// the network

#define DK2ERR_COMMANDNOTNETWORK       0x1002

/////////////////////////////////////////////////////////////////
// DK2ERR_TOOMANYPROGS
//
// The maximum possible programs using the DK2 drivers has been
// reached

#define DK2ERR_TOOMANYPROGS            0x1004

/////////////////////////////////////////////////////////////////
// DK2ERR_BADOS
//
// The DK2 command will not function on the current operating system

#define DK2ERR_BADOS                   0x1005

/////////////////////////////////////////////////////////////////
// DK2ERR_NETWORKONLY
//
// The DK2 command failed because it can only be performed across
// the network and a local connection was specified

#define DK2ERR_NETWORKONLY             0x1006

/////////////////////////////////////////////////////////////////
// DK2ERR_CANCELLED
//
// Returned the GDI/ECP Window is cancelled

#define DK2ERR_CANCELLED               0x1007

/////////////////////////////////////////////////////////////////
// DK2ERR_FAILURE
//
// The command failed due to an error comunicating with the protocol

#define DK2ERR_FAILURE                 0x8000

////////////////////////////////////////////////////////////////
// DK2ERR_PROTOCOLFAILURE
//
// The DK2 command faied due to a problem in the protocol

#define DK2ERR_PROTOCOLFAILURE         0x8001

////////////////////////////////////////////////////////////////
// DK2ERR_BADPARAMETER
//
// The DK2 command failed due to an invalid parameter passed to the 
// function

#define DK2ERR_BADPARAMETER            0x8002

////////////////////////////////////////////////////////////////
// DK2ERR_NOMEMORY
//
// The DK2 command failed because the function could not allocate
// enough memory

#define DK2ERR_NOMEMORY                0x8003

////////////////////////////////////////////////////////////////
// DK2ERR_STARTPROTOCOL
//
// The DK2 command failed because the current protocol did not
// start

#define DK2ERR_STARTPROTOCOL           0x8004

////////////////////////////////////////////////////////////////
// DK2ERR_NOPROTOCOL
//
// The DK2 command failed be cause the current protocol does not
// exist or is not loaded
 
#define DK2ERR_NOPROTOCOL              0x8005

////////////////////////////////////////////////////////////////
// DK2ERR_NOSERVERMEMORY
//
// The DK2 command failed because the server could not allocate
// enough memory

#define DK2ERR_NOSERVERMEMORY          0x8006

////////////////////////////////////////////////////////////////
// DK2ERR_INVALIDCONNECTION
//
// The DK2 command failed because the specified connection is
// invalid

#define DK2ERR_INVALIDCONNECTION       0x8007


////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////

#pragma pack( 1 )

#define DK2MEMORYMAP
typedef struct _tDK2Memory
{
    WORD  wAddress;
    WORD  wSeed;
    WORD  wCount;
    LPSTR lpBytes;
    WORD  wModule;
} DK2MEMORY, FAR *LPDK2MEMORY;

typedef struct _tDateTime
{
    WORD wDay;
    WORD wMonth;
    WORD wYear;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} DATETIME, *NPDATETIME, FAR *LPDATETIME;

#pragma pack()


////////////////////////////////////////////////////////////////
// DK2 Functions
////////////////////////////////////////////////////////////////

BOOL APIENTRY DK2DriverInstalled( void );

WORD APIENTRY FindDK2( LPSTR Id, LPSTR PKey );

WORD APIENTRY FindDK2Ex( LPSTR Id, LPSTR PKey, LPDK2MEMORY lpDK2Memory );

WORD APIENTRY FindDK2ExP( LPSTR Id, LPSTR PKey, WORD Address, WORD Seed, WORD Count, LPSTR Bytes, WORD Module );

void APIENTRY DK2LogoutFromServer( WORD DataReg );

WORD APIENTRY DK2FindLPTPort( WORD Port );

WORD APIENTRY DK2GetDelayTime ( void );

void APIENTRY DK2SetDelayTime( WORD Delay );

void APIENTRY DK2ReadRandomNumbers( WORD  DataReg,
                    LPSTR Id,
                    WORD  Seed,
                    LPSTR Buffer,
                    WORD  BytesToRead );

void APIENTRY DK2ThroughEncryption( WORD  DataReg,
                    LPSTR Id,
                    WORD  Seed,
                    LPSTR Buffer,
                    WORD  BytesToEncrypt );


void APIENTRY DK2ReadMemory( WORD  DataReg,
                 LPSTR Id,
                 WORD  Seed,
                 WORD  Address,
                 LPSTR Buffer,
                 WORD  BytesToRead );

void APIENTRY DK2WriteMemory( WORD  DataReg,
                  LPSTR Id,
                  WORD  Seed,
                  WORD  Address,
                  LPSTR Buffer,
                  WORD  BytesToWrite,
                  LPSTR Password );

void APIENTRY DK2ReadDownCounter( WORD  DataReg,
                  LPSTR Id,
                  DWORD *DownCounter );


void APIENTRY DK2DecrementDownCounter( WORD  DataReg,
                       LPSTR Id );

BOOL APIENTRY DK2RegisterModule( WORD  DataReg, WORD wModule );

BOOL APIENTRY DK2UnregisterModule( WORD  DataReg, WORD wModule );

void APIENTRY DK2RestartDownCounter( WORD  DataReg,
                     LPSTR Id,
                     DWORD NewCounter,
                     LPSTR Password );

void APIENTRY DK2ReadDUSN( WORD   DataReg,
                           LPSTR  Id,
                           LPSTR  Password,
                           LPWORD SecCount,
                           LPSTR  DUSN );

void APIENTRY DK2SendAlgorithmString( WORD  DataReg,
                      LPSTR Id,
                      WORD  Iteration1,
                      WORD  Iteration2,
                      LPSTR Buffer1,
                      LPSTR Buffer2 );

void APIENTRY DK2SendAlgorithmBuffer( WORD  DataReg,
                      LPSTR Id,
                      LPWORD Iteration,
                      LPSTR Buffer,
                      WORD  BufferCount );

void APIENTRY DK2SendAndReceive( WORD DataReg, LPSTR Id, LPSTR lpFirst, WORD wFirst, LPSTR lpSend, WORD wSend, LPSTR lpReceive, WORD wReceive, WORD wCount );

BOOL APIENTRY DK2Success( void );

void APIENTRY DK2AllowChangeInterrupts( WORD Change );

WORD APIENTRY DK2DetectSpeed( WORD  DataReg,
                  LPSTR Id );

WORD APIENTRY DK2SubDetectSpeed( WORD  DataReg,
                 LPSTR Id,
                 LPSTR PKey,
                 LPSTR Bytes );

////////////////////////////////////////////////////////////////
// time function
void APIENTRY DK2GetSystemTime( WORD DateReg, LPSTR Id, LPDATETIME lpDateTime );

////////////////////////////////////////////////////////////////
// DK2 Flags 

#define DK2_BITRONICS      0x00000001
#define DK2_HASBITRONICS   0x00000002

DWORD APIENTRY DK2GetFlags ( WORD DataReg,
                 LPSTR Id );

VOID APIENTRY DK2SetFlags ( WORD  DataReg,
                LPSTR Id,
                DWORD Flags );

////////////////////////////////////////////////////////////////

WORD APIENTRY DK2Encode( LPSTR lpszData,
              WORD  cbData,
              LPSTR lpszEncode,
              WORD  cbEncode );

WORD APIENTRY DK2Decode( LPSTR lpszData,
              LPSTR lpszDecode );

////////////////////////////////////////////////////////////////
// DK2 Access Flags - Override Searching Network or Local

#define DNET_NETWORK            0x0001
#define DNET_LOCAL              0x0002

void APIENTRY DK2SetAccessFlags( WORD wFlags );

////////////////////////////////////////////////////////////////

DWORD APIENTRY DK2GetLastError( void );

void APIENTRY DK2FormatError( DWORD Error, LPSTR ErrorString, int MaxLen  );

WORD APIENTRY DK2GetServerName( WORD DataReg, LPSTR lpszServerName, LPSTR lpszComputerName );

#ifdef __cplusplus
 }
#endif
