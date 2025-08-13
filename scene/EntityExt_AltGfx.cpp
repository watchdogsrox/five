// Title	:	EntityExt_AltGfx.cpp
// Author	:	John Whyte
// Started	:	15/4/2010

// self
#include "scene/EntityExt_AltGfx.h"

// rage

// framework
#include "fwUtil/KeyGen.h"

//game
#include "scene/scene_channel.h"
#include "streaming/streaming.h"
#include "Objects/object.h"


CEntityExt_AltGfxMgr gAltGfxMgr;

FW_INSTANTIATE_CLASS_POOL(CEntityExt_AltGfx, 2, atHashString("Entity Alt request data",0xeb64adf7));
AUTOID_IMPL(CEntityExt_AltGfx);


// Name			:	CEntityExt_AltGfxMgr::InitSystem
// Purpose		:	Do some setup required for starting the system. Initialise pools etc.
// Parameters	:	None
// Returns		:	Nothing
void CEntityExt_AltGfxMgr::InitSystem(void){
	CEntityExt_AltGfx::InitPool( MEMBUCKET_FX );
}

// Name			:	CEntityExt_AltGfxMgr::ShutdownSystem
// Purpose		:	Do some setup required for starting the system. Initialise pools etc.
// Parameters	:	None
// Returns		:	Nothing
void CEntityExt_AltGfxMgr::ShutdownSystem(void){
	CEntityExt_AltGfx::ShutdownPool();
}

// Name			:	CEntityExt_AltGfxMgr::ProcessExtensions
// Purpose		:	Go through the streaming pool - remove satisfied requests into entries in the render pool
//					Go through the rendering pool - deallocate render pool entries which are no longer used
// Parameters	:	None
// Returns		:	Nothing
void CEntityExt_AltGfxMgr::ProcessExtensions(void){

	// disabled for the time being
#if 0
	CEntityExt_AltGfx::ProcessExtensions();

	gAltGfxMgr.ProcessRenderListForAltGfx();
#endif //0
}

// go through the given render list and pick out those entities which are going to have alt gfx added
// Creates new altGfx extension which is added to entity & this will be processed to deal with streaming & rendering the altGfx
void CEntityExt_AltGfxMgr::ProcessRenderListForAltGfx(/*CRenderList list*/){

	// look for the target entity 
	CEntity* pTargetEntity = NULL;

	// pick it out of the dynamic pool for now - eventually we want to pull it out of the render list!
	s32 poolCount = (s32) CObject::GetPool()->GetSize();
	while(--poolCount >= 0){
		CObject* pObject = CObject::GetPool()->GetSlot(poolCount);
		if (pObject){
			CBaseModelInfo* pMI = pObject->GetBaseModelInfo();
			const u32 targetKey = ATSTRINGHASH("Prop_bin_03a", 0x0f833f411);
			if (pMI->GetHashKey() == targetKey){
				pTargetEntity = pObject;
				break;
			}
		}
	}

	if (pTargetEntity){
		// if entity is too far away then bail

		// if entity already has an altGfx extension then bail
		if (pTargetEntity->GetExtension<CEntityExt_AltGfx>() == NULL){
		
			// add a new altGfx extension to this entity for the desired files
			RequestAltGfxFiles(pTargetEntity);
		}
	}
}

// generate the name drawable & texture file names we want to request for the alternate graphic
void CEntityExt_AltGfxMgr::RequestAltGfxFiles(CEntity* UNUSED_PARAM(pEntity), s32 UNUSED_PARAM(streamFlags) /*=0*/){

// 	sceneAssert(pEntity);
// 	if (!pEntity) {
// 		return;
// 	}
// 
// 	//char	altDwdName[80];
// 	//char	altTxdName[80];
// 
// 	pEntity->DestroyExtensionType(CEntityExt_AltGfx::GetAutoId());			// strip all ped stream requests for this entity
// 
// 	CBaseModelInfo* pMI = pEntity->GetBaseModelInfo());
// 	sceneAssert(pMI);
// 	if (pMI){
// 		CEntityExt_AltGfx* pNewAltGfx = rage_new CEntityExt_AltGfx;
// 		sceneAssert(pNewAltGfx);
// 		if (pNewAltGfx)
// 		{
// 			pNewAltGfx->AttachToOwner(pEntity);
// 
// 			// generate txdName and dwdName from entity
// 			//sprintf(altTxdName,"%s_high",pMI->GetModelName());
// 			//sprintf(altDwdName,"%s_high",pMI->GetModelName());
// 
// 			//pNewAltGfx->AddTxd(altTxdName, streamFlags);
// 			//pNewAltGfx->AddDwd(altDwdName, streamFlags);
// 			//pNewAltGfx->ActivateRequest();		//activate this request
// 
// 			char altModelIdxName[80];
// 			sprintf(altModelIdxName,"%s_high",pMI->GetModelName());
	// 			pNewAltGfx->SetModel_Index(CModelInfo::GetModelIdFromName(altModelIdxName).GetModelIndex());
// 			pNewAltGfx->ActivateRequest();		//activate this request
// 		}
// 	}
}

//---------------------------------------------------------------------
// CEntityExt_AltGfx

CEntityExt_AltGfx::CEntityExt_AltGfx() 
{ 
	m_dwdIdx = -1;
	m_txdIdx = -1;
	m_pOwningEntity=NULL; 
	m_state=INIT; 

	m_modelIdx = fwModelId::MI_INVALID;
}

CEntityExt_AltGfx::~CEntityExt_AltGfx() 
{
	Release();
	Assert(m_state == FREED);
}

void CEntityExt_AltGfx::AddTxd(const char* txdName, s32 streamFlags) {

	strLocalIndex txdSlotId = g_TxdStore.FindSlot(txdName);

	if (txdSlotId != -1){
		m_txdRequest.Request(txdSlotId, g_TxdStore.GetStreamingModuleId(), streamFlags);
	} else {
		sceneAssertf(false,"Streamed .txd request for ped is not found : %s\n", txdName);
	}
}

void CEntityExt_AltGfx::AddDwd(const char* dwdName, s32 streamFlags) {

	strLocalIndex dwdSlotId = strLocalIndex(g_DwdStore.FindSlot(dwdName));

	if (dwdSlotId != -1){
		m_dwdRequest.Request(dwdSlotId, g_DwdStore.GetStreamingModuleId(), streamFlags);
	}else {
		sceneAssertf(false,"Streamed .dwd request for ped is not found : %s\n", dwdName);
	}
}

void CEntityExt_AltGfx::Release(void){

	m_txdRequest.Release();
	m_dwdRequest.Release();

	m_state = FREED;
}

void CEntityExt_AltGfx::AttachToOwner(CEntity *pEntity) { 

	Assert(pEntity);
	if (pEntity){
		// add this request extension to the extension list
		pEntity->GetExtensionList().Add(*this);
		m_pOwningEntity = pEntity; 
	}
}

// debug code - request and load an object & use that for the time being
bool CEntityExt_AltGfx::HasModelIndexLoaded(void){

	if ((m_modelIdx != fwModelId::MI_INVALID) && (m_state==REQ))
	{
		fwModelId modelId(m_modelIdx);
		if (!CModelInfo::HaveAssetsLoaded(modelId))
		{
			CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		} else {
			m_state = USE;
			return(true);
		}
	}
	return(false);
}

void CEntityExt_AltGfx::ProcessExtensions(void)
{
	CEntityExt_AltGfx::Pool* pStreamReqPool = CEntityExt_AltGfx::GetPool();

	if (pStreamReqPool->GetNoOfUsedSpaces() == 0){
		return;												// early out if no requests outstanding
	}

	CEntityExt_AltGfx* pStreamReq = NULL;
	CEntityExt_AltGfx* pStreamReqNext = pStreamReqPool->GetSlot(0);
	s32 poolSize=pStreamReqPool->GetSize();

	s32 i = 0;
	while(i<poolSize)
	{
		pStreamReq = pStreamReqNext;

		// spin to next valid entry
		while((++i < poolSize) && (pStreamReqPool->GetSlot(i) == NULL));

		if (i != poolSize)
			pStreamReqNext = pStreamReqPool->GetSlot(i);

		//if (pStreamReq && pStreamReq->HasLoaded()){
		if (pStreamReq && pStreamReq->HasModelIndexLoaded()){
			// apply the files we requested and which have now all loaded to the target ped
			//CEntity* pEntity =  pStreamReq->GetOwningEntity();
			//ApplyStreamPedFiles(pPlayer, &(pPlayer->GetVarData()), pStreamReq);

			//pEntity->DestroyExtension(pStreamReq);		// will remove from pool & from extension list
		}
	}
}

grcTexture* CEntityExt_AltGfx::GetTexture(void)		
{ 
	if (m_txdIdx != -1){
		return(g_TxdStore.Get(strLocalIndex(m_txdIdx))->GetEntry(0)); 
	} else {
		return(NULL);
	}
}

rmcDrawable* CEntityExt_AltGfx::GetDrawable(void)   
{ 
	if (m_dwdIdx != -1){
		return(g_DwdStore.Get(strLocalIndex(m_dwdIdx))->GetEntry(0)); 
	} else {
		return(NULL);
	}
}
