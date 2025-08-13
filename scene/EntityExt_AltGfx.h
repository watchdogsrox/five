// Title	:	EntityExt_AltGfx.h
// Author	:	John Whyte
// Started	:	15/4/2010

#ifndef _ENTITYEXT_ALTGFX_H_
#define _ENTITYEXT_ALTGFX_H_

#include "fwtl/pool.h"
#include "scene/Entity.h"
#include "streaming/streamingRequest.h"

// management class for processing the entries in the AltGfx extension pools
class CEntityExt_AltGfxMgr{
public:
	CEntityExt_AltGfxMgr() {}
	~CEntityExt_AltGfxMgr() {}

	bool Initialise(void) { return(true); }
	void ShutdownLevel(void) {}
	void InitSystem(void);
	void ShutdownSystem(void);

	static void  ProcessExtensions(void);
	void RequestAltGfxFiles(CEntity* pEntity, s32 streamFlags = 0);
	void ProcessRenderListForAltGfx(void);
};

// entity extension which handles the alternate graphics streaming request
class CEntityExt_AltGfx : public fwExtension {
	
public:
	CEntityExt_AltGfx();
	~CEntityExt_AltGfx();

	EXTENSIONID_DECL(CEntityExt_AltGfx, fwExtension);
	FW_REGISTER_CLASS_POOL(CEntityExt_AltGfx); 

	void RequestsComplete();
	void Release();
	bool HasLoaded() { return(m_dwdRequest.HasLoaded() && m_txdRequest.HasLoaded() && (m_state == REQ)); }

	void AddTxd(const char* txdName, s32 streamFlags);
	void AddDwd(const char* dwdName, s32 streamFlags);
	void SetModel_Index(u32 idx) { m_modelIdx = idx; }
	void ActivateRequest() { Assert((m_state == INIT) || (m_state == FREED)); m_state = REQ; }

	grcTexture* GetTexture();
	rmcDrawable* GetDrawable();

	void AttachToOwner(CEntity* pEntity);
	CEntity* GetOwningEntity(void) { return(m_pOwningEntity); }

	static void ProcessExtensions(void);

	bool HasModelIndexLoaded();

private:
	enum	state{ INIT, REQ, USE, FREED };
	state	m_state;

	strRequest			m_dwdRequest;					// streaming request data
	strRequest			m_txdRequest;

	strLocalIndex		m_dwdIdx;						// rendering data
	strLocalIndex		m_txdIdx;					

	strLocalIndex		m_modelIdx;						// some debug data - streaming in a model index for now...

	CEntity*			m_pOwningEntity;
};


extern CEntityExt_AltGfxMgr gAltGfxMgr;

#endif // _ENTITYEXT_ALTGFX_H_
