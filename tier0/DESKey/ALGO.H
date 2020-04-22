
#ifdef __cplusplus
extern "C"
 {
#endif

#ifndef APIENTRY
#define APIENTRY FAR PASCAL
#endif

void APIENTRY DK2SetupAlgorithmString ( LPSTR String, WORD Cmd );

void APIENTRY DK2SetMaximumIterations( WORD MaxIter );

void APIENTRY DK2Sub_ReadRandomNumbers( WORD    DataReg,
                                        LPSTR   Id, 
                                        LPSTR   PKey,
                                        WORD    Seed,
                                        LPSTR   Buffer );

void APIENTRY DK2Sub_ReadMemory( WORD  DataReg,
                                 LPSTR Id, 
                                 LPSTR PKey,
                                 WORD  Seed,
                                 WORD  Address,
                                 LPSTR Buffer );

void APIENTRY DK2Sub_WriteMemory( WORD    DataReg,
                                  LPSTR   Id, 
                                  LPSTR   PKey,
                                  WORD    Seed,
                                  WORD    Address,
                                  WORD    SecretCounter,
                                  LPSTR   Password,
                                  LPSTR   DUSN,
                                  LPSTR   Buffer );

void APIENTRY DK2Sub_ReadDownCounter( WORD     DataReg,
                                      LPSTR    Id, 
                                      LPSTR    PKey,
                                      LPDWORD  DownCounter );

void APIENTRY DK2Sub_SubtractDownCounter( WORD  DataReg,
                                          LPSTR   Id, 
                                          LPSTR   PKey,
                                          DWORD   SubValue,
                                          LPDWORD DownCounter );

void APIENTRY DK2Sub_RestartDownCounter( WORD   DataReg,
                                         LPSTR  Id, 
                                         LPSTR  PKey,
                                         WORD   SecretCounter,
                                         LPSTR  Password,
                                         LPSTR  DUSN,
                                         DWORD  DownCounter );

void APIENTRY DK2Sub_AccessNormalCommands( WORD   DataReg,
                                           LPSTR  Id, 
                                           LPSTR  PKey,
                                           WORD   Disable );

void APIENTRY DK2Algorithm( WORD Iterations, 
                            LPSTR AlgoStr, 
                            LPSTR PrivKey );

#ifdef __cplusplus
 }
#endif