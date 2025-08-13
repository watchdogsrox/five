// FILE :    ClipDictionaryStoreInterface.h

#ifndef CLIP_DICTIONARY_STORE_INTERFACE_H
#define CLIP_DICTIONARY_STORE_INTERFACE_H

// Rage headers
#include "fwscene/stores/clipdictionarystore.h"

////////////////////////////////////////////////////////////////////////////////
// CClipDictionaryStoreInterface
////////////////////////////////////////////////////////////////////////////////

class CClipDictionaryStoreInterface : public fwClipDictionaryStoreGameInterface
{

public:

	static CClipDictionaryStoreInterface& GetInstance() { FastAssert(sm_Instance); return *sm_Instance; }

private:

	CClipDictionaryStoreInterface();
	virtual ~CClipDictionaryStoreInterface();

public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

public:

	virtual void OnLoad(strLocalIndex iIndex);
	virtual void OnRemove(strLocalIndex iIndex);
	virtual void OnAddRef(strLocalIndex iIndex);
	virtual void OnRemoveRef(strLocalIndex iIndex);
	virtual void OnRemoveRefWithoutDelete(strLocalIndex iIndex);
	virtual void OnResetAllRefs(strLocalIndex iIndex);

private:

	static CClipDictionaryStoreInterface* sm_Instance;

};

#endif //CLIP_DICTIONARY_STORE_INTERFACE_H
