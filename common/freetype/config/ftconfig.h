#ifndef __FTCONFIG_H__MULTILIB
#define __FTCONFIG_H__MULTILIB

#ifdef ANDROID
#include <sys/cdefs.h>
#elif defined(PLATFORM_HAIKU)
#include <BeBuild.h>
# if B_HAIKU_64_BIT
#  define __WORDSIZE 64
# elif B_HAIKU_32_BIT
#  define __WORDSIZE 32
# else
#  error "Unknown Haiku bit size"
# endif
#elif defined(OSX) || defined(PLATFORM_BSD)
#include <stdint.h>
#else
#include <bits/wordsize.h>
#endif

#if __WORDSIZE == 32
# include "ftconfig-32.h"
#elif __WORDSIZE == 64
# include "ftconfig-64.h"
#else
# error "unexpected value for __WORDSIZE macro"
#endif

#endif 
