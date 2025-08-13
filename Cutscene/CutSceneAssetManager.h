/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneAssetManager.h
// PURPOSE : An entity manager class for streaming of cutscene objects
// AUTHOR  : Thomas French
// STARTED : 20/07/2009
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef CUTSCENE_ASSETMGRENTITY_H 
#define CUTSCENE_ASSETMGRENTITY_H 

//Rage includes
#include "cutscene/cutsmanager.h"
#include "cutfile/cutfobject.h"
#include "atl/vector.h"

//Game includes
#include "cutscene/CutSceneDefine.h"
#include "cutscene/cutsentity.h"
#include "streaming/streamingrequest.h"

enum ELoadState
{
	LOAD_STATE_NONE,
	LOAD_STATE_STREAMING
};

enum eStreamingDataType
{
	INVALID_TYPE = -1,  
	SCALEFORM_OVERLAY_TYPE = 0,
	BINK_OVERLAY_TYPE,
	RAYFIRE_TYPE,
	MODEL_TYPE,
	MODEL_ITYPE_TYPE,
	SUBTITLES_TYPE, //unique should only have one load request for this. Generated 
	PTFX_TYPE,		//unique should only have one load request for this
	PED_VARIATION_TYPE,
	INTERIOR_TYPE
};

const char *GetStreamingDataTypeString(eStreamingDataType streamingDataType);

enum eCutsceneRequestType
{
	SINGLE_STR_REQUEST = 1,
	VARIATION_REQUEST, 
	MODEL_DEPENDENCY_REQUEST
};

enum eVaritionSlotStreaming
{
	TEXTURE_SLOT = 0,
	DRAWABLE_SLOT,
	CLOTH_SLOT
};

enum eVariationStreamingStatus
{
	NO_REQUEST_FOR_VARIATION =0, 
	LOADING_VARIATION,
	LOADED_VARIATION
};


#define NULL_STREAMING_INDEX -1
#define NULL_RAYFIRE_STREAMING_INDEX 0
#define MAX_TIME_SPEND_STREAMING 5000

#if !__NO_OUTPUT
#define cutsceneAssetMgrWarningf( fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_WARNING) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_WARNING) ) { char debugStr[512]; CCutSceneAssetMgrEntity::CommonDebugStr( debugStr); strcat(debugStr, fmt); cutsceneWarningf(debugStr,##__VA_ARGS__); }
#define cutsceneAssetMgrDebugf1( fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG1) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG1) ) { char debugStr[512]; CCutSceneAssetMgrEntity::CommonDebugStr( debugStr); strcat(debugStr, fmt); cutsceneDebugf1(debugStr,##__VA_ARGS__); }
#define cutsceneAssetMgrDebugf2( fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG2) ) { char debugStr[512]; CCutSceneAssetMgrEntity::CommonDebugStr( debugStr); strcat(debugStr, fmt); cutsceneDebugf2(debugStr,##__VA_ARGS__); }
#define cutsceneAssetMgrDebugf3( fmt,...) if ( (Channel_cutscene.TtyLevel >= DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel >= DIAG_SEVERITY_DEBUG3) ) { char debugStr[512]; CCutSceneAssetMgrEntity::CommonDebugStr( debugStr); strcat(debugStr, fmt); cutsceneDebugf3(debugStr,##__VA_ARGS__); }
#else
#define cutsceneAssetMgrWarningf(fmt,...) do {} while(false)
#define cutsceneAssetMgrDebugf1( fmt,...) do {} while(false)
#define cutsceneAssetMgrDebugf2( fmt,...) do {} while(false)
#define cutsceneAssetMgrDebugf3( fmt,...) do {} while(false)
#endif //!__NO_OUTPUT

class CCutSceneStreamingInfo
{
public: 
	CCutSceneStreamingInfo();
	CCutSceneStreamingInfo(s32 m_ObjectId, s32 m_Index, eStreamingDataType m_DataType, atHashString m_Hash); 
	
	virtual ~CCutSceneStreamingInfo();

	s32 GetObjectId() const { return m_ObjectId; }
	
	s32 GetGameIndex() const { return m_Index; }

	void SetObjectIndex(s32 index) { m_Index = index; }

	void AddRef() { m_ref += 1; }
	
	void RemoveRef(); 

	s32 GetRef() const { return m_ref; }

	eStreamingDataType GetDataType() const { return m_DataType; }

	atHashString* GetHashString() { return &m_Hash; }

	virtual bool HasLoaded()=0; 
	
	virtual void ClearRequiredFlags(u32 flags) =0; 
	virtual void SetRequiredFlags(u32 flags) =0; 

	virtual void Release()=0; 

	virtual u32 GetType() =0;
		
	virtual bool IsReleased() = 0; 

#if !__NO_OUTPUT
	virtual atString GetDebugString();
#endif // !__NO_OUTPUT

	s32 m_ObjectId;
	s32 m_Index; 
	s32 m_ref; 
	float m_LoadTime; 
	eStreamingDataType m_DataType; 
	atHashString m_Hash; 
	
};

class CCutSceneSingleRequestStreamingInfo : public CCutSceneStreamingInfo
{
public:
	CCutSceneSingleRequestStreamingInfo();
	~CCutSceneSingleRequestStreamingInfo(); 

	CCutSceneSingleRequestStreamingInfo(s32 m_ObjectId, s32 m_Index,eStreamingDataType m_DataType, atHashString m_Hash);

	strRequest* GetRequest() { return &m_request; }

	const strRequest* GetRequest() const  { return &m_request; }
	
	bool HasLoaded() { return m_request.HasLoaded(); }

	void ClearRequiredFlags (u32 flags) { m_request.ClearRequiredFlags(flags); }
	void SetRequiredFlags (u32 flags) { m_request.SetRequiredFlags(flags); }

	void Release() {m_request.Release();}

	u32 GetType() { return SINGLE_STR_REQUEST; }
	
	bool IsReleased() { return !m_request.IsValid(); }

#if __BANK
	virtual atString GetDebugString();
#endif // __BANK

	strRequest m_request; 
};

class CCutsceneModelRequestStreamingInfo : public CCutSceneStreamingInfo
{
public: 

	CCutsceneModelRequestStreamingInfo(); 
	~CCutsceneModelRequestStreamingInfo(); 
		
	CCutsceneModelRequestStreamingInfo(s32 m_ObjectId, s32 m_Index,eStreamingDataType m_DataType, atHashString m_Hash);
	
	strModelRequest* GetRequest() { return &m_request; }

	const strModelRequest* GetRequest() const  { return &m_request; }

	bool HasLoaded() { return m_request.HasLoaded(); }
	
	u32 GetType() { return MODEL_DEPENDENCY_REQUEST; }
	
	void Release() { m_request.Release();}
	
	void ClearRequiredFlags(u32 flags) { m_request.ClearRequiredFlags(flags); }
	void SetRequiredFlags (u32 flags) { m_request.SetRequiredFlags(flags); }

	bool IsReleased() { return !m_request.IsValid(); }

#if __BANK
	virtual atString GetDebugString();
#endif // __BANK

	strModelRequest m_request; 
};

class CCutsceneVariationStreamingInfo : public CCutSceneStreamingInfo
{
public:

	CCutsceneVariationStreamingInfo(); 
	~CCutsceneVariationStreamingInfo(); 
	
	CCutsceneVariationStreamingInfo(s32 m_ObjectId, s32 m_Index,eStreamingDataType m_DataType, atHashString m_Hash); 

	strRequest& GetRequest(u32 index) { return m_request.GetRequest(index); }

	const strRequest& GetRequest(u32 index) const  { return m_request.GetRequest(index); }
	
	bool HasLoaded() { return m_request.HaveAllLoaded(); }
	
	void ClearRequiredFlags (u32 flags) { m_request.ClearRequiredFlags(flags); }
	void SetRequiredFlags (u32 flags) { m_request.SetRequiredFlags(flags); }

	void Release() {m_request.ReleaseAll();}

	u32 GetType() { return VARIATION_REQUEST; }
	
	bool IsReleased(); 

#if __BANK
	virtual atString GetDebugString();
#endif // __BANK

	strRequestArray<3> m_request; 

	u32 m_Component; 
	u32 m_Texture; 
	u32 m_Drawable; 
};




/////////////////////////////////////////////////////////////////////////////////
//	cutf objects are Rage side objects that are generated from the data file. Each has a unique ID. 
//	Deals with the streaming of assets from the streaming system. At the moment just deals with models for peds, vehicles and objects. 

class CutSceneManager; 

class CCutSceneAssetMgrEntity : public cutsUniqueEntity
{
public:
	friend class CutSceneManager; 
	friend class CCutSceneScaleformOverlayEntity;  
	friend class CCutSceneBinkOverlayEntity; 

	CCutSceneAssetMgrEntity( const cutfObject* pObject );
	~CCutSceneAssetMgrEntity();
	
	//Dispatches loading events that come through from the cut scene manager. Creates our cut scene object. 
	virtual void DispatchEvent(cutsManager* pManager, const cutfObject* pObject, s32 iEventID, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0);
	
	//Get a pointer to our cut scene Creature.
	//CCutSceneCreature* FindEntityObject( const cutfObject *pObject );

	const ELoadState GetLoadState() const { return m_loadState; }
	
	bool GetModelStreamingRequestId(s32 iObjectId, const atHashString& Hash,  s32 &iRequestId ) ; //Get the id (model index) of the object

	bool HasRequestedModelLoaded(s32 iObjectId, const atHashString& Hash);	//check that the model has loaded
	
	bool IsModelInRequestList(s32 iObjectId, const atHashString& Hash) const; //check if the request model is in the streamed list

	bool HasPTFXListLoaded(const atHashString& Hash) const; 

	s32 GetGameIndex(s32 iObjectid) const; 
	
	void GenerateModifiedModelName(const char* pModelName, char* ModelName, int iStringLength); 
	
	void RemoveVariationStreamRequest(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, s32 iObjectId); 
	eStreamingRequestReturnValue SetActorVariationStreamingRequest(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, u32 ObjectId); 
	
	eVariationStreamingStatus GetVariationLoadingState(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, u32 ObjectId); 
	
	bool HaveAllVariationsLoaded(eStreamingDataType type); 

	//void DumpAllVariationsLoaded(s32 ObjectID);

	void SetModelStreamingRequest(atHashString ModelName, s32 ObjectId, eStreamingDataType type); 
	
	void SetItypeModelStreamingRequest(atHashString ModelName, atHashString Itype, s32 ObjectId); 

	void RemoveStreamingRequest(s32 Id, bool RespectRefs, eStreamingDataType type, atHashString Modelhash); 
	
	void RemoveStreamingRequest(s32 objectId, bool RespectRefs, eStreamingDataType dataType);

	//bool IsTextBlockLoaded(return TheText.HasThisAdditionalTextLoaded(m_TextBlockName, MISSION_DIALOGUE_TEXT_SLOT) ); 
#if __ASSERT
	bool IsLoadingSubtitles() { return stricmp(m_TextBlockName, "")!=0;}
	bool IsLoadingAdditionalText(const char * textName);
	const char * GetAdditionalTextBlockName(){ return m_TextBlockName; }
#endif // __ASSERT

protected:
	void RemoveAllStreamingRequests(); 
	
	void RemoveAllStreamingRequestsOfType(eStreamingDataType type, bool RespectRefs); 
	
	void ResetStreamingTime() { m_TimeSpentStreaming = 0; }

private: 
	
	void RemoveInteriorStreamingRequest(); 

	void ReleaseStreamingRequests(CCutSceneStreamingInfo* StreaminRequest); 
	
	bool ShouldAddRequestRefForObject(s32 ObjectId);
	
	bool ShouldAddRequestRefForObjectOfType(eStreamingDataType Type); 

	bool ShouldAddRequestRefForObject(s32 Objectid, atHashString m_hash, eStreamingDataType Type);

	s32 RequestRayFireObject(cutsManager* pManager, u32 ObjectId); 

	void SetScaleformStreamingRequest(const cutfObject* pLoadObject); 	

	void SetBinkStreamingRequest(const cutfOverlayObject* pLoadObject);

	void SetSubtitleStreamingRequest(const char* TextBlockName, eStreamingDataType Type); 

	void SetParticleFxStreamingRequest(cutsManager* pManager, const cutfObject* pObject, eStreamingDataType Type ); 
	
	s32 GetVariationStreamingIndexFromRequestList(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, s32 iObjectId);

	eStreamingRequestReturnValue SetActorComponentVariationStreamingRequest(u32 iComp, u32 iDrawable, u32 iTexture, atHashString& ModelName, u32 ObjectId); 

	eStreamingRequestReturnValue SetActorPropVariationStreamingRequest(u32 iComp, u32 propID, u32 texID, atHashString& ModelName, u32 ObjectId); 

	bool CanRemoveCutSceneRequiredFlag(s32 ModuleIndex); 
	
	void SetInteriorStreamingRequest(s32 ObjectId,  s32 StreamingIndex, atHashString InteriorName, float time); 

	bool UpdateModelLoading(CCutSceneStreamingInfo &streamingRequest, CutSceneManager* pCutSceneManager); 
	bool UpdatePtfxLoading(CCutSceneStreamingInfo &streamingRequest); 
	bool UpdateRayfireLoading(CCutSceneStreamingInfo &streamingRequest, CutSceneManager* pCutSceneManager); 
	bool UpdateScaleformLoading(CCutSceneStreamingInfo &streamingRequest); 
	bool UpdateBinkLoading(CCutSceneStreamingInfo &streamingRequest); 
	bool UpdateSubtitleLoading(CCutSceneStreamingInfo &streamingRequest); 
	bool UpdateVariationLoading(CCutSceneStreamingInfo &streamingRequest); 
	bool UpdateInteriorLoading(CCutSceneStreamingInfo &streamingRequest);
	
	void RemoveStreamingRequest(s32 index); 

#if __BANK
	virtual void DebugDraw() const;
	void PrintStreamingRequests() const;
#endif // __BANK

private: 
	atVector<CCutSceneStreamingInfo*> m_StreamingRequests; 

	u32 m_TimeSpentStreaming; 

	//Loading state of our cut scene system
	ELoadState m_loadState;
	
	//name of text block
	char m_TextBlockName[TEXT_KEY_SIZE]; 
#if !__NO_OUTPUT	
	static void CommonDebugStr(char * debugStr);
#endif
};


#endif
