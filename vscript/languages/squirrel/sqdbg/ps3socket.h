#ifdef _PS3
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "tier0/threadtools.h"
typedef int SOCKET;
#define INVALID_SOCKET -1
#endif
