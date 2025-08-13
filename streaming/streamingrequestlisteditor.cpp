//
// streaming/streamingrequestlist.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#include "streaming/streamingrequestlisteditor.h"

#if SRL_EDITOR

#include "bank/group.h"
#include "modelinfo/ModelInfo.h"


CStreamingRequestListEditor::CStreamingRequestListEditor()
: m_Cursor(0)
, m_IsCursorInRequests(true)
{
	m_EntityName[0] = 0;
}

void CStreamingRequestListEditor::AddWidgets(bkGroup &group)
{
	bkGroup *editorGroup = group.AddGroup("Editor");
	editorGroup->AddButton("Cursor Up", datCallback(MFA(CStreamingRequestListEditor::OnCursorUp), this));
	editorGroup->AddButton("Cursor Down", datCallback(MFA(CStreamingRequestListEditor::OnCursorDown), this));
	editorGroup->AddButton("Move Earlier", datCallback(MFA(CStreamingRequestListEditor::OnMoveEarlier), this));
	editorGroup->AddButton("Move Later", datCallback(MFA(CStreamingRequestListEditor::OnMoveLater), this));
	editorGroup->AddSeparator();
	editorGroup->AddText("Entity", m_EntityName, sizeof(m_EntityName));
	editorGroup->AddButton("Add", datCallback(MFA(CStreamingRequestListEditor::OnAddEntity), this));
	editorGroup->AddSeparator();
	editorGroup->AddButton("REMOVE", datCallback(MFA(CStreamingRequestListEditor::OnRemove), this));
	editorGroup->AddSeparator();
	editorGroup->AddButton("Save Cutscene", datCallback(MFA(CStreamingRequestListEditor::OnSave), this));
}

CStreamingRequestFrame::EntityList *CStreamingRequestListEditor::GetCurrentEntityList() const
{
	CStreamingRequestRecord *record = gStreamingRequestList.GetCurrentRecord();

	if (record)
	{
		int index = (m_IsCursorInRequests) ? gStreamingRequestList.GetRequestListIndex() : gStreamingRequestList.GetUnrequestListIndex();

		if (index < record->GetFrameCount())
		{
			CStreamingRequestFrame &frame = record->GetFrame(index);

			return (m_IsCursorInRequests) ? &frame.GetRequestList() : &frame.GetUnrequestList();
		}
	}

	return NULL;
}

void CStreamingRequestListEditor::OnMoveEarlier()
{
	MoveCurrentBy(-1);
}

void CStreamingRequestListEditor::OnMoveLater()
{
	MoveCurrentBy(1);
}

void CStreamingRequestListEditor::MoveCurrentBy(int offset)
{
	CStreamingRequestRecord *record = gStreamingRequestList.GetCurrentRecord();
	CStreamingRequestFrame::EntityList *entityList = GetCurrentEntityList();

	if (entityList && m_Cursor < entityList->GetCount())
	{
		int frameIndex = (m_IsCursorInRequests) ? gStreamingRequestList.GetRequestListIndex() : gStreamingRequestList.GetUnrequestListIndex();
		int newFrame = frameIndex + offset;

		// Trying to move this object outside the cutscene?
		if (newFrame < 0 || newFrame >= record->GetFrameCount())
		{
			return;
		}
	
		// Take the model...
		u32 modelNameHash = (*entityList)[m_Cursor];
		entityList->Delete(m_Cursor);

		if (m_Cursor >= entityList->GetCount() && m_Cursor > 0)
		{
			m_Cursor--;
		}

		// And move it to the target frame.
		CStreamingRequestFrame &frame = record->GetFrame(newFrame);
		entityList = (m_IsCursorInRequests) ? &frame.GetRequestList() : &frame.GetUnrequestList();

		// Make sure we don't have it already there.
		fwModelId modelId;
		if (CModelInfo::GetBaseModelInfoFromHashKey(modelNameHash, &modelId))
		{
			if (frame.FindIndex(*entityList, modelId) == -1)
			{
				entityList->Grow() = modelNameHash;
			}
		}
	}

}

void CStreamingRequestListEditor::OnCursorUp()
{
	CStreamingRequestFrame::EntityList *currentList = GetCurrentEntityList();

	// Bow out if we're not editing something.
	if (!currentList)
	{
		// If we're editing the unrequest list, go up to the request list.
		m_IsCursorInRequests = true;
		m_Cursor = 0;
		return;
	}

	m_Cursor--;

	if (m_Cursor >= currentList->GetCount())
	{
		m_Cursor = currentList->GetCount() - 1;
	}

	// Move to the other list if we're on the top of the unrequests.
	if (m_Cursor < 0)
	{
		if (m_IsCursorInRequests)
		{
			m_Cursor = 0;
		}
		else
		{
			m_IsCursorInRequests = true;
			m_Cursor = 0;
			currentList = GetCurrentEntityList();

			if (currentList && currentList->GetCount() > 0)
			{
				m_Cursor = currentList->GetCount() - 1;
			}
		}
	}
}

void CStreamingRequestListEditor::OnCursorDown()
{
	CStreamingRequestFrame::EntityList *currentList = GetCurrentEntityList();

	// Bow out if we're not editing something.
	if (!currentList)
	{
		return;
	}

	m_Cursor++;

	// Did we hit the end of the list?
	if (m_Cursor >= currentList->GetCount())
	{
		if (m_IsCursorInRequests)
		{
			// Then move to the unrequests.
			m_IsCursorInRequests = false;
			m_Cursor = 0;
		}
		else
		{
			// Or stay where we are.
			m_Cursor--;
		}
	}
}

void CStreamingRequestListEditor::OnRemove()
{
	CStreamingRequestFrame::EntityList *currentList = GetCurrentEntityList();

	if (currentList && currentList->GetCount() > m_Cursor)
	{
		// Delete the current entry.
		currentList->Delete(m_Cursor);
		gStreamingRequestList.MarkModified();

		// Move back if this was the last entry.
		if (m_Cursor > 0 && m_Cursor >= currentList->GetCount())
		{
			m_Cursor--;
		}
	}
}

void CStreamingRequestListEditor::OnAddEntity()
{
	CStreamingRequestFrame::EntityList *currentList = GetCurrentEntityList();

	if (currentList && m_EntityName[0])
	{
		// Add it.
		currentList->Grow() = m_EntityName;
		gStreamingRequestList.MarkModified();
	}
}

void CStreamingRequestListEditor::OnSave()
{
	gStreamingRequestList.SaveCurrentList();
}


#endif // SRL_EDITOR
