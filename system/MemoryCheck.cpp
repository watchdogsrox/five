#include "memorycheck.h"
#include "security/obfuscatedtypes.h"


#if USING_OBFUSCATED_DATA_GUARDS
// In the event to which we're using the data obfuscation guards, 
// lets setup the appropriate getters and setters

__declspec(noinline) int GetMagicXorValueGuardFunction()
{
	return s_magicXorValue;
}
__declspec(noinline) void SetMagicXorValueGuardFunction(int param)
{
	s_magicXorValue = param;
}
int MemoryCheck::GetMagicXorValue()
{
	// I have to insert some ridiculous logic here to make sure the SetMagic() function 
	// doesn't get compiled out of the binary
	if(s_magicXorValue == 0)
		SetMagicXorValueGuardFunction(0xb7ac4b1c);
	return GetMagicXorValueGuardFunction();
}
#else
int MemoryCheck::GetMagicXorValue()
{
	return s_magicXorValue;
}
#endif

#if USING_OBFUSCATED_DATA_GUARDS
void MemoryCheck::SetVersionAndType(unsigned param)
{
	FastObfuscatedPointerDataSet(&nVersionAndType, &param, sizeof(nVersionAndType));
}
void MemoryCheck::SetAddressStart(unsigned param)
{
	FastObfuscatedPointerDataSet(&nAddressStart, &param, sizeof(nAddressStart));
}
void MemoryCheck::SetSize(unsigned param)
{
	FastObfuscatedPointerDataSet(&nSize, &param, sizeof(nSize));
}
void MemoryCheck::SetValue(unsigned param)
{
	FastObfuscatedPointerDataSet(&nValue, &param, sizeof(nValue));
}
void MemoryCheck::SetFlags(unsigned param)
{
	FastObfuscatedPointerDataSet(&nFlags, &param, sizeof(nFlags));
}
void MemoryCheck::SetXorCrc(unsigned param)
{
	FastObfuscatedPointerDataSet(&nXorCrc, &param, sizeof(nXorCrc));
}

unsigned MemoryCheck::GetVersionAndType()
{
	unsigned temp = 0;
	FastObfuscatedPointerDataGet(&temp, &nVersionAndType, sizeof(nVersionAndType));
	return temp;
}
unsigned MemoryCheck::GetAddressStart()
{
	unsigned temp = 0;
	FastObfuscatedPointerDataGet(&temp, &nAddressStart, sizeof(nAddressStart));
	return temp;
}
unsigned MemoryCheck::GetSize()
{
	unsigned temp = 0;
	FastObfuscatedPointerDataGet(&temp, &nSize, sizeof(nSize));
	return temp;
}
unsigned MemoryCheck::GetValue()
{
	unsigned temp = 0;
	FastObfuscatedPointerDataGet(&temp, &nValue, sizeof(nValue));
	return temp;
}
unsigned MemoryCheck::GetFlags()
{
	unsigned temp = 0;
	FastObfuscatedPointerDataGet(&temp, &nFlags, sizeof(nFlags));
	return temp;
}
unsigned MemoryCheck::GetXorCrc()
{
	unsigned temp = 0;
	FastObfuscatedPointerDataGet(&temp, &nXorCrc, sizeof(nXorCrc));
	return temp;
}

#else
void MemoryCheck::SetVersionAndType(unsigned param)
{
	nVersionAndType.Set(param);
}
void MemoryCheck::SetAddressStart(unsigned param)
{
	nAddressStart.Set(param);
}
void MemoryCheck::SetSize(unsigned param)
{
	nSize.Set(param);
}
void MemoryCheck::SetValue(unsigned param)
{
	nValue.Set(param);
}
void MemoryCheck::SetFlags(unsigned param)
{
	nFlags.Set(param);
}
void MemoryCheck::SetXorCrc(unsigned param)
{
	nXorCrc.Set(param);
}

unsigned MemoryCheck::GetVersionAndType()
{
	return nVersionAndType.Get();
}
unsigned MemoryCheck::GetAddressStart()
{
	return nAddressStart.Get();
}
unsigned MemoryCheck::GetSize()
{
	return nSize.Get();
}
unsigned MemoryCheck::GetValue()
{
	return nValue.Get();
}
unsigned MemoryCheck::GetFlags()
{
	return nFlags.Get();
}

unsigned MemoryCheck::GetXorCrc()
{
	return nXorCrc.Get();
}
#endif