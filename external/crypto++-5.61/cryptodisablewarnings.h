// turn off warnings that get generated when building Crypto++ in our environment

#ifdef _WIN32
#pragma warning( disable : 4100 ) // Warning C4100: unreferenced formal parameter
#pragma warning( disable : 4127 ) // Warning C4127: conditional expression is constant
#pragma warning( disable : 4189 ) // Warning C4189: local variable is initialized but not referenced
#pragma warning( disable : 4231 ) // Warning C4231: nonstandard extension used
#pragma warning( disable : 4244 ) // Warning C4244: conversion from 'unsigned int' to 'byte', possible loss of data
#pragma warning( disable : 4245 ) // Warning C4245: conversion from 'int' to 'DWORD', signed/unsigned mismatch
#pragma warning( disable : 4250 ) // Warning C4250: inherits via dominance
#pragma warning( disable : 4267 ) // Warning C4267: 'argument' : conversion, possible loss of data
#pragma warning( disable : 4355 ) // Warning C4355: 'this' : used in base member initializer list
#pragma warning( disable : 4389 ) // Warning C4389: signed/unsigned mismatch
#pragma warning( disable : 4505 ) // Warning C4505: unreferenced local function has been removed
#pragma warning( disable : 4511 ) // Warning C4511: copy constructor could not be generated
#pragma warning( disable : 4512 ) // Warning C4512: assignment operator could not be generated
#pragma warning( disable : 4516 ) // Warning C4516: access-declarations are deprecated; member using-declarations provide a better alternative
#pragma warning( disable : 4661 ) // Warning C4661: no suitable definition provided for explicit template instantiation request
#pragma warning( disable : 4701 ) // Warning C4701: local variable may be used without having been initialized
#pragma warning( disable : 4702 ) // Warning C4702: unreachable code
#pragma warning( disable : 4706 ) // Warning C4706: assignment within conditional expression
#endif // _WIN32
