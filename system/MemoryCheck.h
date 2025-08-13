//
//
//
#ifndef INC_MEMORYCHECK_H_
#define INC_MEMORYCHECK_H_
#include "security/obfuscatedtypes.h"
#define USING_OBFUSCATED_DATA_GUARDS 0
// The bit shift to get the memory-constraint bits.
#define ACTION_FLAG_MEM_CONSTRAINT_SHIFT 8
static u32 s_magicXorValue = 0xB7AC4B1C;
class MemoryCheck
{
public:
	enum ActionFlags
	{
		FLAG_Telemetry			 = 0x00000001,
		FLAG_Report				 = 0x00000002,
		FLAG_FrackDefrag		 = 0x00000004,
		FLAG_KickRandomly		 = 0x00000008,
		FLAG_NoMatchmake		 = 0x00000010,
		FLAG_Gameserver			 = 0x00000020,
		FLAG_Crash				 = 0x00000040,
		FLAG_MemRead			 = 0x00000200,
		FLAG_MemReadwrite		 = 0x00000400,
		FLAG_MemWrite			 = 0x00000800,
		FLAG_MemExecute			 = 0x00001000,
		FLAG_MemExecuteRead		 = 0x00002000,
		FLAG_MemExecuteWrite	 = 0x00004000,
		FLAG_MemExecuteReadwrite = 0x00008000,
		FLAG_SP_Processed		 = 0x08000000,
		FLAG_MP_Processed		 = 0x80000000
	};

	enum CheckType
	{
		// For the check to pass, dll name shouldn't be available
		CHECK_DllName = 4,

		// For the check to pass, process with name shouldn't be running on the system
		CHECK_ProcessNames = 5,

		// Modified version of chilead, that utilises the page siezs.
		CHECK_PageByteString = 7,

		// Modified version of chilead, that utilises the page siezs.
		CHECK_ExecutableChecker = 8
	};

	unsigned GetVersionAndType();
	unsigned GetAddressStart();
	unsigned GetSize();
	unsigned GetValue();
	unsigned GetFlags();
	unsigned GetXorCrc();

	void SetVersionAndType(unsigned param);
	void SetAddressStart(unsigned param);
	void SetSize(unsigned param);
	void SetValue(unsigned param);
	void SetFlags(unsigned param);
	void SetXorCrc(unsigned param);	
	static int GetMagicXorValue();


	static const u32 s_versionAndSkuMask = 0x00ffffff;
#if __BANK
	static const u32 s_typeMask = 0xff000000;
#endif //__BANK

	// Helper function to access the unobfuscated check type
	__forceinline CheckType getCheckType() const { return (CheckType)((nVersionAndType ^ nValue ^ s_magicXorValue) >> 24); }
	__forceinline bool IsValidType()
	{
		if(!(this->getCheckType() == CHECK_DllName ||
			this->getCheckType() == CHECK_ExecutableChecker ||
			this->getCheckType() == CHECK_PageByteString ||
			this->getCheckType() == CHECK_ProcessNames))
		{
			return false;
		}
		return true;
	}
	__forceinline static u32 makeVersionAndSku(int version, u8 sku)
	{
		return (((((u32)version)&0xffff)  | (((u32)sku) << 16)) ^ s_magicXorValue) & s_versionAndSkuMask;
	}

#if __BANK
	void setVersionAndType(int version, u8 sku, CheckType type) { nVersionAndType = ((((type << 24) ^ s_magicXorValue) & s_typeMask) | makeVersionAndSku(version, sku)) ^ nValue; }
#endif //__BANK

private:
	static const u32 s_magicXorValue = 0xb7ac4b1c;
#if USING_OBFUSCATED_DATA_GUARDS
	unsigned nVersionAndType;	// Top 8 bits = check type, bottom 24 bits = version
	unsigned nAddressStart;
	unsigned nSize;
	unsigned nValue;
	unsigned nFlags; 
	unsigned nXorCrc;
#else
	sysObfuscated<u32> nVersionAndType;	// Top 8 bits = check type, bottom 24 bits = version
	sysObfuscated<u32> nAddressStart;
	sysObfuscated<u32> nSize;
	sysObfuscated<u32> nValue;
	sysObfuscated<u32> nFlags; 
	sysObfuscated<u32> nXorCrc;
#endif
};

#endif // INC_MEMORYCHECK_H_