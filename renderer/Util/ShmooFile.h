// ======================
// renderer/Util/ShmooFile.h
// (c) 2013 RockstarNorth
// ======================

#ifndef _SHMOOFILE_H_
#define _SHMOOFILE_H_

#include "parser/macros.h"
#include "atl/array.h"
#include "atl/string.h"
#include "grcore/shmoo.h"

struct ShmooItem
{
    atString m_TechniqueName;
    int m_Pass;
    int m_RegisterCount;
    
    PAR_SIMPLE_PARSABLE;
};

struct ShmooFile
{
	atArray<ShmooItem> m_ShmooItems;
	
#if __BANK
	void Set(const atArray<grcShmoo::Result> &results);
	bool Save(const char *shaderName);
#endif // __BANK

	bool Load(const char *shaderName);
	void Apply(grmShader *shader);
	
	PAR_SIMPLE_PARSABLE;
};

#define APPLY_SHMOO (__PPU)
#define GENERATE_SHMOO (__BANK && APPLY_SHMOO)

#if GENERATE_SHMOO
#define GENSHMOO_ONLY(x) x
#else
#define GENSHMOO_ONLY(x)
#endif // PFX_SHMOO


namespace ShmooHandling {
#if APPLY_SHMOO

int Register(const char* shaderName, grmShader *shader, bool shouldExist, float improvementDelta = 0.25f);

#else // APPLY_SHMOO
__forceinline int Register(const char* , grmShader *, bool, float = 0.25f)  { return 0; }

#endif // APPLY_SHMOO

#if GENERATE_SHMOO

void AddWidgets(bkBank& bank);

bool IsShmooEnable(int shmooIdx);

void BeginShmoo(int shmooIdx, const grmShader *shader, int pass);
void EndShmoo(int shmooIdx);

#else

__forceinline void AddWidgets(bkBank& ) { /* NoOp */ }

__forceinline bool IsShmooEnable(int ) { return false; }

__forceinline void BeginShmoo(int , const grmShader *, int, float ) { /* NoOp */ }
__forceinline void BeginShmoo(int , const grmShader *, int ) { /* NoOp */ }
__forceinline void EndShmoo(int ) { /* NoOp */ }

#endif // APPLY_SHMOO

};

#endif // _SHMOOFILE_H_
