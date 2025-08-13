#ifndef CUTSCENE_ANIMMGRENTITY_H 
#define CUTSCENE_ANIMMGRENTITY_H 

//Rage
#include "atl/vector.h"
#include "cutscene/cutsanimentity.h"

//Game
#include "streaming/streamingrequest.h"

#if __BANK
#define cutsceneAnimMgrDebugf( fmt,...) if ( (Channel_cutscene.TtyLevel == DIAG_SEVERITY_DEBUG3) || (Channel_cutscene.FileLevel == DIAG_SEVERITY_DEBUG3) ) { char debugStr[256]; CCutSceneAnimMgrEntity::CommonDebugStr( debugStr); strcat(debugStr, fmt); cutsceneDebugf1(debugStr,##__VA_ARGS__); }
#else
#define cutsceneAnimMgrDebugf( fmt,...) do {} while(false)
#endif //__BANK

//Purpose: Derived class from rage for managing the loading of cuts dictionaries. This light weight class relies on the rage cuts animation 
//manager to deal with playing the animation of the entites and passes events to the anim manager to tell it to load/unload dictionaries.
class CCutSceneAnimMgrEntity : public cutsAnimationManagerEntity
{
public:	
	CCutSceneAnimMgrEntity (const cutfObject* pObject );
	~CCutSceneAnimMgrEntity();
	// base class overrides
	virtual void DispatchEvent( cutsManager* pManager, const cutfObject* pObject, s32 iEventId, const cutfEventArgs* pEventArgs=NULL, const float fTime = 0.0f, const u32 StickyId = 0 );
	
	//returns the currently loaded dictionary
	//const char * GetCurrentAnimDict();
	
#if __BANK
	bool m_HasUnloadDictionaryBeenCalledThisFrame; 
#endif

#if __BANK
	void PrintStreamingRequests();
#endif // __BANK


protected:
	//Requests an anim dictionary to load, currently doesn't fall back on base class function LoadDictionary file if it fails, as this could hide problems
	//Tells the cut scene manager that its waiting for a cut dictionary to load.
	virtual bool LoadDictionary( cutsManager *pManager, s32 iObjectId, const char* pName, int iBuffer );
	
	//Unloads a cuts dictionary.
	virtual bool UnloadDictionary( cutsManager *pManager, int iBuffer);

	//Waits for the a dictionary to be loaded and tells the cut scene manager. 
	bool HasDictionaryLoaded();

	virtual bool IsDictionaryLoaded(int iBuffer); 

	void UnloadAllDictionaries(); 

private:

	struct DictionaryRequests
	{
		DictionaryRequests()
		: RequestId(-1)
		{
			
		}
		int RequestId; 
		strRequest m_Request; 
	};


	strRequest m_request[c_numBuffers];	//a list of streaming requests for anim dictionaries. 
	atVector<int> m_requestId; 

	atVector<DictionaryRequests> m_RequestedDicts; 

#if __BANK	
	static void CommonDebugStr(char * debugStr);
#endif
};

#endif