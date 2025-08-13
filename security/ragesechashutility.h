#include "security/ragesecengine.h"
#include "security/ragesecenginetasks.h"

#ifndef SECURITY_HASH_UTILITY
#define SECURITY_HASH_UTILITY

#if USE_RAGESEC

// This must match the CRC function in the GenerateCRCs/MemoryHasher tool!
inline u32 crcRange(const u8* pBuff, const u8* pEnd)
{
	// Based on the Jenkins hash from http://en.wikipedia.org/wiki/Jenkins_hash_function, with different initial value
	// This happens to be the same as the hash we use with atDataHash, except not restricted to 4-byte blocks.
	u32 hash = 0x04c11db7;
	while(pBuff!=NULL && pBuff != pEnd)
	{
		hash += *pBuff++;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);
	return hash;
}

__forceinline u32 crcHashString(const char *string)
{
	size_t length = strlen(string);
	char c_copy[MAX_PATH];
	strcpy(c_copy, string);

#if RSG_PC
	_strupr(c_copy);
#elif RSG_ORBIS
	strupr(c_copy);
#endif

	return crcRange((u8*)c_copy, (u8*)c_copy+length);
}

#endif // USE_RAGESEC

#endif // SECURITY_HASH_UTILITY