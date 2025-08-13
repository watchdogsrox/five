// FILE :    ClipDictionaryStoreInterface.cpp

// File header
#include "animation/ClipDictionaryStoreInterface.h"

// Game headers
#include "streaming/PrioritizedClipSetStreamer.h"

ANIM_OPTIMISATIONS()

////////////////////////////////////////////////////////////////////////////////
// CClipDictionaryStoreInterface
////////////////////////////////////////////////////////////////////////////////

CClipDictionaryStoreInterface* CClipDictionaryStoreInterface::sm_Instance = NULL;

CClipDictionaryStoreInterface::CClipDictionaryStoreInterface()
{

}

CClipDictionaryStoreInterface::~CClipDictionaryStoreInterface()
{

}

void CClipDictionaryStoreInterface::Init(unsigned UNUSED_PARAM(initMode))
{
	//Create the instance.
	Assert(!sm_Instance);
	sm_Instance = rage_new CClipDictionaryStoreInterface;

	//Set the game interface.
	g_ClipDictionaryStore.SetGameInterface(sm_Instance);
}

void CClipDictionaryStoreInterface::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Reset the game interface.
	g_ClipDictionaryStore.ResetGameInterface();

	//Free the instance.
	Assert(sm_Instance);
	delete sm_Instance;
	sm_Instance = NULL;
}

void CClipDictionaryStoreInterface::OnLoad(strLocalIndex iIndex)
{
	CPrioritizedClipSetStreamer::GetInstance().OnClipDictionaryLoaded(iIndex);
}

void CClipDictionaryStoreInterface::OnRemove(strLocalIndex iIndex)
{
	CPrioritizedClipSetStreamer::GetInstance().OnClipDictionaryRemoved(iIndex);
}

void CClipDictionaryStoreInterface::OnAddRef(strLocalIndex iIndex)
{
	CPrioritizedClipSetStreamer::GetInstance().OnClipDictionaryRefAdded(iIndex);
}

void CClipDictionaryStoreInterface::OnRemoveRef(strLocalIndex iIndex)
{
	CPrioritizedClipSetStreamer::GetInstance().OnClipDictionaryRefRemoved(iIndex);
}

void CClipDictionaryStoreInterface::OnRemoveRefWithoutDelete(strLocalIndex iIndex)
{
	CPrioritizedClipSetStreamer::GetInstance().OnClipDictionaryRefRemovedWithoutDelete(iIndex);
}

void CClipDictionaryStoreInterface::OnResetAllRefs(strLocalIndex iIndex)
{
	CPrioritizedClipSetStreamer::GetInstance().OnClipDictionaryAllRefsReset(iIndex);
}
