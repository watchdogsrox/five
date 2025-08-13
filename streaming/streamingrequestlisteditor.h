//
// streaming/streamingrequestlisteditor.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef STREAMING_STREAMINGREQUESTLISTEDITOR_H
#define STREAMING_STREAMINGREQUESTLISTEDITOR_H

#define SRL_EDITOR		(__BANK)

#if SRL_EDITOR

#include "data/base.h"
#include "streaming/streamingrequestlist.h"


namespace rage
{
	class bkGroup;
}



class CStreamingRequestListEditor : public datBase
{
public:
	CStreamingRequestListEditor();



	void AddWidgets(bkGroup &group);

	int GetCursor() const				{ return m_Cursor; }

	bool IsCursorInRequests() const		{ return m_IsCursorInRequests; }

private:
	CStreamingRequestFrame::EntityList *GetCurrentEntityList() const;

	void OnCursorUp();

	void OnCursorDown();

	void OnMoveEarlier();

	void OnMoveLater();

	void MoveCurrentBy(int offset);

	void OnRemove();

	void OnAddEntity();

	void OnSave();


	char m_EntityName[256];

	// Index into the current list (request or unrequest, depending on m_IsCursorInRequests).
	int m_Cursor;

	// True if the cursor is in the request list, false if it's in the unrequest list.
	bool m_IsCursorInRequests;

};

#endif // SRL_EDITOR


#endif // STREAMING_STREAMINGREQUESTLISTEDITOR_H
