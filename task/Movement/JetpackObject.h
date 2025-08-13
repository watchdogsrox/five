#ifndef __JETPACK_OBJECT_H__
#define __JETPACK_OBJECT_H__

#include "scene/RegdRefTypes.h"
#include "streaming/streamingrequest.h"
#include "fwanimation/clipsets.h"
#include "fwtl/pool.h"

#if !__FINAL && !__XENON
#define ENABLE_JETPACK 1 
#else
#define ENABLE_JETPACK 0
#endif

#if ENABLE_JETPACK
	# define JETPACK_ONLY(x) x
#else 
	# define JETPACK_ONLY(x) 
#endif

#if ENABLE_JETPACK

class CObject;
class CJetpack;

// Make a "forward declared" RegdRef for CJetpacks and add comparison of them
// to RegdEnt objects.
typedef	fwRegdRef<class CJetpack> RegdJetpack;
typedef	fwRegdRef<const class CJetpack> RegdConstJetpack;
MAKE_REGDPTR_TYPE_DIRECT_COMPARES(RegdJetpack, RegdEnt);

class CJetpackManager
{
public:

	static const s32 nMaxJetpacks = 16;

	friend class CJetpack;

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static CJetpack *CreateJetpackContainer();

	static void CreateJetpackPickup(const Matrix34 &initialTransform, CPed *UNUSED_PARAM(pDropPed));
};

//////////////////////////////////////////////////////////////////////////
// Class:	CJetpack
// Purpose:	Handles jetpack object (streaming, receiving parts etc).
class CJetpack : public fwRefAwareBase
{
public:

	FW_REGISTER_CLASS_POOL(CJetpack);

	CJetpack();
	~CJetpack();

	bool Request(u32 jetpackName);
	bool CreateObject();
	bool DestroyObject();
	bool HasLoaded();

	CObject *GetObject() const { return m_pJetpackObject; }
	
private:

	RegdObj m_pJetpackObject;

	fwClipSetRequestHelper	m_HoverClipSetRequestHelper;
	fwClipSetRequestHelper	m_FlightClipSetRequestHelper;

	strLocalIndex m_iJetpackModelIndex;
	strRequest m_ModelRequestHelper;
	strRequest m_PtFxRequestHelper;
};

#endif // ENABLE_JETPACK

#endif
