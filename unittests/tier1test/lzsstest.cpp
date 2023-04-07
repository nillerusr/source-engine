#include "tier0/dbg.h"
#include "unitlib/unitlib.h"
#include "tier1/lzss.h"

#ifdef USING_ASAN
#include <sanitizer/asan_interface.h>
#endif

DEFINE_TESTSUITE( LZSSSafeUncompressTestSuite )

static void SafeUncompressTests()
{
	CLZSS lzss;
	char poision1[8192];
	unsigned char in[256];

	char poision2[8192];

	unsigned char out[256];
	char poision3[8192];

#ifdef USING_ASAN
	ASAN_POISON_MEMORY_REGION( poision1, 8192 );
	ASAN_POISON_MEMORY_REGION( poision2, 8192 );
	ASAN_POISON_MEMORY_REGION( poision3, 8192 );
#endif

	lzss_header_t *pHeader = (lzss_header_t*)in;
	pHeader->actualSize = sizeof(in)-sizeof(lzss_header_t);
	pHeader->id = LZSS_ID;

	uint result = 0;

	// 0xff bytes test
	memset(in+8, 0xFF, sizeof(in)-sizeof(lzss_header_t));
	result = lzss.SafeUncompress( in, sizeof(in), out, sizeof(out) );
	Shipping_Assert( result == 0 );

	// zero bytes test
	memset(in+8, 0x0, sizeof(in)-sizeof(lzss_header_t));
	result = lzss.SafeUncompress( in, sizeof(in), out, sizeof(out) );
	Shipping_Assert( result == 0 );

	// valid data, invalid header
	pHeader->actualSize = 1;
	pHeader->id = LZSS_ID;
	result = lzss.SafeUncompress( in, sizeof(in), out, sizeof(out) );
	Shipping_Assert( result == 0 );

	pHeader->actualSize = 999;
	pHeader->id = LZSS_ID;
	result = lzss.SafeUncompress( in, sizeof(in), out, sizeof(out) );
	Shipping_Assert( result == 0 );

	pHeader->actualSize = 999;
	pHeader->id = 1337;
	result = lzss.SafeUncompress( in, sizeof(in), out, sizeof(out) );
	Shipping_Assert( result == 0 );


	// valid header, valid data
	const unsigned char compressed[] = {0x4c,0x5a,0x53,0x53,0x1a,0x0,0x0,0x0,0x0,0x44,0x6f,0x20,0x79,0x6f,0x75,0x20,0x6c,0x0,0x69,0x6b,0x65,0x20,0x77,0x68,0x61,0x74,0x41,0x0,0xd4,0x73,0x65,0x65,0x3f,0x0,0x0,0x0};

	pHeader->actualSize = sizeof(compressed);
	pHeader->id = LZSS_ID;
	result = lzss.SafeUncompress( compressed, sizeof(compressed), out, sizeof(out) );

	const char data[] = "Do you like what you see?";
	Shipping_Assert( memcmp(out, data, 26) == 0 );
	Shipping_Assert( result == 26 );
}

DEFINE_TESTCASE( LZSSSafeUncompressTest, LZSSSafeUncompressTestSuite )
{
	Msg( "Running CLZSS::SafeUncompress tests\n" );

	SafeUncompressTests();
}
