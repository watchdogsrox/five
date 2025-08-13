//
// Filename:	CustomShaderEffect.h
// Description:	Class for customizable updating local shader variables during run-time at CEntity level
// Written by:	Andrzej
//
//
//	10/06/2005	-	Andrzej:	- initial;
//	30/09/2005	-	Andrzej:	- update to Rage WR146;
//
//
//
#ifndef __CCUSTOMSHADEREFFECTBASE_H__
#define __CCUSTOMSHADEREFFECTBASE_H__

// Rage includes:
#include "grcore/Effect_Typedefs.h"
#include "grcore/Effect_Config.h"
#include "grmodel/ShaderGroupVar.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "system/memops.h"

#include "fwdrawlist/drawlist.h"

#include "entity/drawdata.h"
#include "game/ModelIndices.h"
#include "modelinfo/BaseModelInfo.h"	// ModelInfoType

// forward declarations
namespace rage {
class rmcDrawable;
class grmShader;
class grmShaderGroup;
class grmShaderVar;
class grcTexture;
class grcVec4BufferInstanceBufferList;
class crAnimation;
class crClip;
#if __BANK
	class bkBank;
#endif
}
class CEntity;
class CVehicleModelInfo;
class InstancedEntityCache;

//
//
//
//
enum eCSE_TYPE
{
	CSE_BASE		=0,
	CSE_PED,
	CSE_ANIMUV,
	CSE_VEHICLE,
	CSE_TINT,
	CSE_PROP,
	CSE_TREE,
	CSE_GRASS,
	CSE_CABLE,
	CSE_MIRROR,
	CSE_WEAPON,
	CSE_INTERIOR,

	CSE_NUM_TYPES
};
CompileTimeAssert(CSE_NUM_TYPES <= fwCustomShaderEffect::CSE_MAX_TYPES);



class CCustomShaderEffectBase;

class CCustomShaderEffectBaseType : public fwCustomShaderEffectBaseType
{
public:
	CCustomShaderEffectBaseType(u32 type) : m_refCount(1) { Assign(m_type,type); }
	virtual bool Initialise(rmcDrawable *)					{ return true; }					
	virtual CCustomShaderEffectBase* CreateInstance(CEntity *pEntity) = 0;
	virtual bool RestoreModelInfoDrawable(rmcDrawable* )	{ return true;}

#if __DEV
	virtual bool AreShadersValid(rmcDrawable *)		{return true;}
#endif
	void AddRef();
	virtual void RemoveRef();
	s32 GetRefCount() const							{ return m_refCount; }

	// This function uses the 'type' parameter to figure out which concrete subclass to invoke,
	// either by calling rage_new directly, or a static Create member of the class.  Next, it
	// invokes Initialise on that just-created master object.  Later on, other code calls CreateInstance
	// every time we need to create a uniquely configured instance of that master object.
	static CCustomShaderEffectBaseType*	SetupForDrawable(rmcDrawable *pDrawable, ModelInfoType miType, CBaseModelInfo* pMI,
		u32 nObjectHashkey=0, s32 animDictFileIndex=-1, grmShaderGroup *pClonedShaderGroup=NULL);

	static CCustomShaderEffectBaseType* SetupMasterForModelInfo(CBaseModelInfo* pMI);
	static CCustomShaderEffectBaseType* SetupMasterVehicleHDInfo(CVehicleModelInfo* pVMI);
protected:
	virtual ~CCustomShaderEffectBaseType();
private:
	s32 m_refCount;

};

//
//
//
//
class CCustomShaderEffectBase : public fwCustomShaderEffect
{
public:

	CCustomShaderEffectBase(u32 size, eCSE_TYPE type);
	virtual	~CCustomShaderEffectBase();

protected:

public:
	/// virtual CCustomShaderEffectBaseType& GetType() const;

	// --------------------------------------------------------------------------------------
	// - CustomShaderEffect Interface:														-
	// --------------------------------------------------------------------------------------
	// these must be overloaded by every children of this class: (see lifetime description above)
	virtual void DeleteInstance()					{ delete this; }

	virtual void SetShaderVariables(rmcDrawable* pDrawable) =0;				// RT: instance setting variables called in BeforeDraw()
	virtual void AddToDrawList(u32 modelIndex, bool bExecute) =0;			// DLC: instance called in BeforeAddToDrawList()
	virtual void AddToDrawListAfterDraw()			{}						// DLC: instance called in AfterAddToDrawList()
	virtual u32  AddDataForPrototype(void * UNUSED_PARAM(address)) { return 0; }

	virtual bool AddKnownRefs()						{return false;}			// DLC: Add known refs on a newly-copied object
	virtual void RemoveKnownRefs()					{}						// DLC: Remove known refs on a temporary copy that's about to die
	// --------------------------------------------------------------------------------------

	//Instancing support!
protected:
	//Generic case: Matrix43 worldMat +  float4 globalParams
	static const u32 sNumRegsPerInst;

public:
	virtual bool HasPerBatchShaderVars() const			{ return false; }	//Does the CSE have shader vars that are global to all instances of the object type.
	virtual u32 GetNumRegistersPerInstance() const		{ return sNumRegsPerInst; }
	virtual Vector4 *WriteInstanceData(grcVec4BufferInstanceBufferList &ibList, Vector4 *pDest, const InstancedEntityCache &entityCache, u32 alpha) const;
	virtual void AddBatchRenderStateToDrawList() const			{ }
	virtual void AddBatchRenderStateToDrawListAfterDraw() const	{ }
};





#endif //__CCUSTOMSHADEREFFECTBASE_H__...

