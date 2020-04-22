/*++

Copyright (c) 1990  Microsoft Corporation

Module Name:

    dbmon.c

Abstract:

    A simple program to print strings passed to OutputDebugString when
    the app printing the strings is not being debugged.

Author:

    Kent Forschmiedt (kentf) 30-Sep-1994

Revision History:

--*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int _cdecl
main(
    int,
    char **
    )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
    HANDLE AckEvent;
    HANDLE ReadyEvent;
    HANDLE SharedFile;
    LPVOID SharedMem;
    LPSTR  String;
    DWORD  ret;
    DWORD  LastPid;
    LPDWORD pThisPid;
    BOOL    DidCR;

    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = &sd;

    if(!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
        fprintf(stderr,"unable to InitializeSecurityDescriptor, err == %d\n",
            GetLastError());
        exit(1);
    }

    if(!SetSecurityDescriptorDacl(&sd, TRUE, (PACL)NULL, FALSE)) {
        fprintf(stderr,"unable to SetSecurityDescriptorDacl, err == %d\n",
            GetLastError());
        exit(1);
    }

    AckEvent = CreateEvent(&sa, FALSE, FALSE, "DBWIN_BUFFER_READY");

    if (!AckEvent) {
        fprintf(stderr,
                "dbmon: Unable to create synchronization object, err == %d\n",
                GetLastError());
        exit(1);
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        fprintf(stderr, "dbmon: already running\n");
        exit(1);
    }

    ReadyEvent = CreateEvent(&sa, FALSE, FALSE, "DBWIN_DATA_READY");

    if (!ReadyEvent) {
        fprintf(stderr,
                "dbmon: Unable to create synchronization object, err == %d\n",
                GetLastError());
        exit(1);
    }

    SharedFile = CreateFileMapping(
                        (HANDLE)-1,
                        &sa,
                        PAGE_READWRITE,
                        0,
                        4096,
                        "DBWIN_BUFFER");

    if (!SharedFile) {
        fprintf(stderr,
                "dbmon: Unable to create file mapping object, err == %d\n",
                GetLastError());
        exit(1);
    }

    SharedMem = MapViewOfFile(
                        SharedFile,
                        FILE_MAP_READ,
                        0,
                        0,
                        512);

    if (!SharedMem) {
        fprintf(stderr,
                "dbmon: Unable to map shared memory, err == %d\n",
                GetLastError());
        exit(1);
    }

    String = (LPSTR)SharedMem + sizeof(DWORD);
    pThisPid = (LPDWORD)SharedMem;

    LastPid = 0xffffffff;
    DidCR = TRUE;

    SetEvent(AckEvent);

    for (;;) {

        ret = WaitForSingleObject(ReadyEvent, INFINITE);

        if (ret != WAIT_OBJECT_0) {

            fprintf(stderr, "dbmon: wait failed; err == %d\n", GetLastError());
            exit(1);

        } else {

            if (LastPid != *pThisPid) {
                LastPid = *pThisPid;
                if (!DidCR) {
                    putchar('\n');
                    DidCR = TRUE;
                }
            }

            if (DidCR) {
                printf("%3u: ", LastPid);
            }

            printf("%s", String);
            DidCR = (*String && (String[strlen(String) - 1] == '\n'));
            SetEvent(AckEvent);

        }

    }
}
