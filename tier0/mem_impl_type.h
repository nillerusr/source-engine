
#if ( (!defined( POSIX )||defined(_GAMECONSOLE)) && (defined(_DEBUG) || defined(USE_MEM_DEBUG) ) )
#define MEM_IMPL_TYPE_DBG 1
#else
#define MEM_IMPL_TYPE_STD 1
#endif
