/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneOverlayEntity.h
// PURPOSE : 
// AUTHOR  : Thomas French
// STARTED : 13/10/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_OVERLAY_ENTITY_H 
#define CUTSCENE_OVERLAY_ENTITY_H

//Rage Headers
#include "cutscene/cutsentity.h"

//Game Headers
#include "cutscene/CutSceneDefine.h"
#include "cutscene/CutSceneManagerNew.h"

class CutSceneManager; 

#if !__NO_OUTPUT
#define cutsceneBinkOverlayEntityDebugf(fmt,...) if ( ((Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3)) && GetObject() ) { char debugStr[512]; CCutSceneBinkOverlayEntity::CommonDebugStr(GetObject(), debugStr); cutsceneDebugf1("%s" fmt,debugStr,##__VA_ARGS__); }
#else
#define cutsceneBinkOverlayEntityDebugf(fmt,...) do {} while(false)
#endif //!__NO_OUTPUT

#if !__NO_OUTPUT
#define cutsceneScaleformOverlayEntityDebugf(fmt,...) if ( ((Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3)) && GetObject() ) { char debugStr[512]; CCutSceneScaleformOverlayEntity::CommonDebugStr(GetObject(), debugStr); cutsceneDebugf1("%s" fmt,debugStr,##__VA_ARGS__); }
#else
#define cutsceneScaleformOverlayEntityDebugf(fmt,...) do {} while(false)
#endif //!__NO_OUTPUT


class CCutSceneBinkOverlayEntity : public cutsUniqueEntity
{
public:	
	CCutSceneBinkOverlayEntity(const cutfObject* pObject);

	~CCutSceneBinkOverlayEntity();

	virtual void DispatchEvent(cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);

	s32 GetType() const { return CUTSCENE_BINK_OVERLAY_GAME_ENTITY; } 

	const cutfOverlayObject* GetOverlayObject() const { return m_pCutfOverlayObject; }

	void RenderBinkMovie() const; 

	static u32 LinkToRenderTarget(u32 OwnerHash, const char* RenderTargetName, u32 ModelHash); 

	u32 GetRenderTargetOwnerTag() const;

protected:

#if !__NO_OUTPUT
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif //!__NO_OUTPUT

private:
	const cutfOverlayObject* m_pCutfOverlayObject; 

	u32 m_RenderTargetId;	
	u32 m_BinkHandle;

	bool m_canRender : 1;
};

class CCutSceneScaleformOverlayEntity : public cutsUniqueEntity
{
public:	
	CCutSceneScaleformOverlayEntity(const cutfObject* pObject);

	~CCutSceneScaleformOverlayEntity();

	virtual void DispatchEvent(cutsManager *pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);

	s32 GetType() const { return CUTSCENE_SCALEFORM_OVERLAY_GAME_ENTITY; } 

	void RenderOverlayToMainRenderTarget() const; // Full Screen

	void RenderOverlayToRenderTarget() const; // Render Target

	u32 GetRenderTargetOwnerTag();
	
	//s32 GetOverlayId() const { return m_OverlayId; }	//get the overlay id, which comes from the scaleform manager
	
	u32 GetRenderId() const { return m_RenderTargetId; }

	bool CanUseMainRenderTarget() { return m_bUseMainRenderTarget; } //will this use an in game target or main render target
	
	void SetOverlayPos(Vector2 vPos) { m_vMin.x = vPos.x; m_vMin.y = vPos.y; }

	void SetOverlaySize(Vector2& vSize) {m_vMax.x = vSize.x; m_vMax.y = vSize.y;  } 

	const cutfOverlayObject* GetOverlayObject() const { return m_pCutfOverlayObject; }

	void SetOverlayId(s32 OverlayId) { m_OverlayId[CutSceneManager::GetCurrentBufferIndex()]=OverlayId; }
	s32 GetOverlayId() const {return m_OverlayId[CutSceneManager::GetCurrentBufferIndex()]; }

	void SetShouldRender(bool shouldRender) { m_ShouldRender[CutSceneManager::GetCurrentBufferIndex()]=shouldRender; }
	s32 GetShouldRender() const {return m_ShouldRender[CutSceneManager::GetCurrentBufferIndex()]; }

	void StreamOutOverlay(cutsManager* pManager,const cutfObject* pObject);
#if __BANK
	const Vector2 GetOverlayPos() const { return m_vMin; }

	const Vector2 GetOverlaySize() const { return m_vMax; }	
#endif

protected:

#if !__NO_OUTPUT
	void CommonDebugStr(const cutfObject* pObject, char * debugStr);
#endif //!__NO_OUTPUT

private:

	void RenderScaleformFullScreen() const;
	void RenderScaleformRenderTarget() const; 

	void SetUseMainRenderTarget(bool bUseMain) { m_bUseMainRenderTarget = bUseMain; }

	void GetAttributesFromArgs(const cutfEventArgs* pEventArgs, atArray<float>* fValue, atArray<const char*> *cValue,  
		atArray<int> *iValue, atArray<bool> *bValue); 

private:
	Vector2 m_vMin;			
	Vector2 m_vMax;

	u32 m_uRenderTargetSizeX;
	u32 m_uRenderTargetSizeY;

	const cutfOverlayObject* m_pCutfOverlayObject; 

	s32 m_OverlayId[2]; 
	bool m_ShouldRender[2]; 

	s32 m_OverlayIdLocal;				// Id of the overlay as set by the scale form manager
	bool m_LocalShouldRender; 

	u32 m_RenderTargetId;			// The id of this objects render target

	bool m_bUseMainRenderTarget;	// Use the main render target 

	bool m_bProcessShowEventPostSceneUpdate;
	bool m_bDisableShouldRenderInPostSceneUpdate;
	bool m_bStreamOutFullScreenRenderTargetNextUpdate; 
	bool m_bIsStreamedOut; 

	u32 m_streamOutFrameCount; 
};



#endif