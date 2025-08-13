//////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ThumbnailManager.h
// PURPOSE : manages requests of thumbnails in the video editor ui
// AUTHOR  : Derek Payne
// STARTED : 05/06/2014
//
/////////////////////////////////////////////////////////////////////////////////

#include "frontend/VideoEditor/ui/ThumbnailManager.h"
#include "frontend/Scaleform/ScaleFormMgr.h"

#include "Frontend/ui_channel.h"
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/VideoEditor/ui/Editor.h"
#include "frontend/VideoEditor/ui/Timeline.h"
#include "frontend/VideoEditor/VideoEditorInterface.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

FRONTEND_OPTIMISATIONS()

//OPTIMISATIONS_OFF()



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::Init
// PURPOSE:	initialises the thumbnail manager
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::Init()
{
#if __BANK
	m_Id = (u8)rage::ClampRange(m_Id + 1, 1, 100);
	uiDisplayf("ThumbnailManager %u - initialised", m_Id);
#endif // __BANK

}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::Shutdown
// PURPOSE:	shuts down the thumbnail manager
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::Shutdown()
{
#if __BANK
	uiDisplayf("ThumbnailManager %u - shut down", m_Id);
#endif // __BANK
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::Update
// PURPOSE:	updates the thumbnail manager
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::Update()
{
	RequestAndUnrequestThumbnails();
}

void CVideoEditorThumbnailManager::ReleaseThumbnail( u32 const uProjectIndex )
{
	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);
		if( pItem && pItem->uIndexToProject == uProjectIndex )
		{
			UnRequestThumbnail( uListCount );
			break;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::Update
// PURPOSE:	Release all thumbnails that we have requested
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::ReleaseAll()
{
	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		UnRequestThumbnail( uListCount );
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::CullTexturesNotInVisibleWindow
// PURPOSE: works out visible window and flags textures no longer needed or if we
//			now need them
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::CullTexturesNotInVisibleWindow()
{
	if (!GetProject())
		return;

	u32 uVisibleWindowStart = 0;
	u32 uVisibleWindowEnd = 0;

	CheckForVisibility(uVisibleWindowStart, uVisibleWindowEnd);

	const u32 uCullStartIndex = uVisibleWindowStart;
	const u32 uCullEndIndex = uVisibleWindowEnd;

	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);

		if (pItem)
		{
			const u32 uProjectIndex = pItem->uIndexToProject;

			const bool c_projectIndexIsInExcludedList = CheckForExcludedClip((s32)uProjectIndex);  // if this clip index is in the 'exclusion list' then we dont want to remove it, consider it still visible

			if (!c_projectIndexIsInExcludedList && (uProjectIndex < uCullStartIndex || uProjectIndex > uCullEndIndex))  // its outside of range
			{
				if (pItem->m_state == THUMBNAIL_STATE_LOADED ||
					pItem->m_state == THUMBNAIL_STATE_REQUESTED ||
					pItem->m_state == THUMBNAIL_STATE_IN_USE)
				{
					pItem->m_state = THUMBNAIL_STATE_NO_LONGER_NEEDED;
#if __BANK
					uiDisplayf("ThumbnailManager %u - Project index %u un-requested", m_Id, uProjectIndex);
#endif
				}
				else if (pItem->m_state != THUMBNAIL_STATE_NOT_VISIBLE)
				{
					pItem->m_state = THUMBNAIL_STATE_NOT_VISIBLE;
				}
			}
			else  // within the window
			{
				if (pItem->m_state == THUMBNAIL_STATE_REQUESTED ||
					pItem->m_state == THUMBNAIL_STATE_PENDING_REQUEST ||
					pItem->m_state == THUMBNAIL_STATE_LOADED ||
					pItem->m_state == THUMBNAIL_STATE_FAILED ||
					pItem->m_state == THUMBNAIL_STATE_IN_USE)
				{
					// do nothing, its already there
				}
				else
				{
					if( !GetProject()->DoesClipHavePendingBespokeCapture( uProjectIndex ) )
					{
						SetSpinner(uProjectIndex, true);

						pItem->m_state = THUMBNAIL_STATE_PENDING_REQUEST;

#if __BANK
						uiDisplayf("ThumbnailManager %u - Project index %u requested", m_Id, uProjectIndex);
#endif
					}
				}
			}
		}
	}

/*
#if __BANK
	// output how many thumbs are loaded vs how many clips we have:
	u32 uThumbnailInUseCount = 0;
	u32 uThumbnailLoading = 0;
	u32 uThumbnailNotNeeded = 0;
	u32 uThumbnailHidden = 0;

	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);

		if (pItem)
		{
			if (pItem->m_state == THUMBNAIL_STATE_IN_USE)
			{
				uThumbnailInUseCount++;
			}

			if (pItem->m_state == THUMBNAIL_STATE_REQUESTED ||
				pItem->m_state == THUMBNAIL_STATE_LOADED ||
				pItem->m_state == THUMBNAIL_STATE_NEEDED)
			{
				uThumbnailLoading++;
			}

			if (pItem->m_state == THUMBNAIL_STATE_NO_LONGER_NEEDED)
			{
				uThumbnailNotNeeded++;
			}

			if (pItem->m_state == THUMBNAIL_STATE_NOT_VISIBLE)
			{
				uThumbnailHidden++;
			}
		}
	}

	uiDisplayf("ThumbnailManager %u -  %u thumbnails in use", m_Id, uThumbnailInUseCount);
#endif // __BANK
*/
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ProcessThumbnailAwaitingSetup
// PURPOSE: goes through any thumbnails awaiting setup, and set them up
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::ProcessThumbnailAwaitingSetup()
{
	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);

		if (pItem && pItem->m_state == THUMBNAIL_STATE_LOADED)
		{
			SetSpinner(pItem->uIndexToProject, false);
			SetupThumbnail(pItem->uIndexToProject);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ProcessThumbnailsFailed
// PURPOSE: reports any thumbnails that may of failed
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::ProcessThumbnailsFailed()
{
	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);

		if (pItem && pItem->m_state == THUMBNAIL_STATE_PENDING_FAILED)
		{
			SetSpinner(pItem->uIndexToProject, false);
			ReportFailed(pItem->uIndexToProject);

			pItem->m_state = THUMBNAIL_STATE_FAILED;  
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ProcessThumbnailsNeeded
// PURPOSE: goes through all thumbnails that are needed and requests them
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::ProcessThumbnailsNeeded()
{
	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);

		if (pItem && pItem->m_state == THUMBNAIL_STATE_PENDING_REQUEST)
		{
			if (pItem->bUsesProjectTexture)
			{
				if ( !GetProject()->IsClipThumbnailRequested(pItem->uIndexToProject) )
				{
					GetProject()->RequestClipThumbnail(pItem->uIndexToProject);
				}
			}

			pItem->m_state = THUMBNAIL_STATE_REQUESTED;
		}
	}
};


/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::ProcessThumbnailsNotNeeded
// PURPOSE: goes through all thumbnails not needed and removes them
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorThumbnailManager::ProcessThumbnailsNotNeeded()
{
	bool bFoundItemToRemove = false;

	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);

		if (pItem && pItem->m_state == THUMBNAIL_STATE_NO_LONGER_NEEDED)
		{
			UnRequestThumbnail(uListCount);
			bFoundItemToRemove = true;
		}
	}

	return bFoundItemToRemove;
};



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::UnRequestThumbnail
// PURPOSE: request the thumnail is removed
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::UnRequestThumbnail(const u32 uThumbnailIndex)
{
	if (!GetProject())
		return;

	sTimelineItem *pItem = GetActiveListItem(uThumbnailIndex);

	if (pItem)
	{
		const u32 uProjectIndex = pItem->uIndexToProject;

#if __BANK
		uiDisplayf("ThumbnailManager %u - Project thumbnail %u un-requested", m_Id, uProjectIndex);
#endif
		RemoveTextureOnComplexObject(uProjectIndex);

		if (pItem->bUsesProjectTexture)
		{
			GetProject()->ReleaseClipThumbnail(uProjectIndex);
		}

		pItem->m_state = THUMBNAIL_STATE_NOT_VISIBLE;
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorTimeline::RequestAndUnrequestThumbnails
// PURPOSE: goes through the states of the thumbs and requests or removes them
//			based on 'visible window'
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::RequestAndUnrequestThumbnails()
{
	if (!GetProject())
		return;

	CullTexturesNotInVisibleWindow();

	// check if the requested thumbnail is now loaded:
	for (u32 uListCount = 0; uListCount < GetActiveListCount(); uListCount++)
	{
		sTimelineItem *pItem = GetActiveListItem(uListCount);

		if (pItem && pItem->m_state == THUMBNAIL_STATE_REQUESTED)
		{
			if (GetProject()->IsClipThumbnailLoaded(pItem->uIndexToProject) || (!pItem->bUsesProjectTexture))
			{
				pItem->m_state = THUMBNAIL_STATE_LOADED;
			}
			else if (GetProject()->HasClipThumbnailLoadFailed(pItem->uIndexToProject))
			{
				pItem->m_state = THUMBNAIL_STATE_PENDING_FAILED;
			}
#if __BANK
			else
			{
				atString textureNameString;
				GetProject()->getClipTextureName(pItem->uIndexToProject, textureNameString);

				uiDisplayf("ThumbnailManager %u - streaming %u - %s  REQ: %s   LOADED: %s   FAILED: %s",	m_Id, pItem->uIndexToProject, textureNameString.c_str(), 
																											GetProject()->IsClipThumbnailRequested(pItem->uIndexToProject) ? "TRUE" : "FALSE", 
																											GetProject()->IsClipThumbnailLoaded(pItem->uIndexToProject) ? "TRUE" : "FALSE", 
																											GetProject()->HasClipThumbnailLoadFailed(pItem->uIndexToProject) ? "TRUE" : "FALSE");
			}
#endif // __BANK
		}
	}

	if (!ProcessThumbnailsNotNeeded())
	{
		if (CanMakeRequests())
		{
			ProcessThumbnailsNeeded();
		}
	}

	ProcessThumbnailAwaitingSetup();
	ProcessThumbnailsFailed();
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorThumbnailManager::GetFirstClipVisibility
// PURPOSE:	finds and returns the first visible or non visible project index
/////////////////////////////////////////////////////////////////////////////////////////
u32 CVideoEditorThumbnailManager::GetFirstClipVisibility(u32 const c_startingProjectIndex, bool const c_searchForVisible)
{
	if (!GetProject())
		return 0;

	if (GetProject()->GetClipCount() == 0)
	{
		return 0;
	}

	s32 const c_containerPosition = GetWindowPosMs() - GetNonVisibleBufferMs();
	s32 const c_containerWidth = GetWindowSizeMs() + GetNonVisibleBufferMs();

	for (u32 uProjectIndex = c_startingProjectIndex; uProjectIndex < GetProject()->GetClipCount(); uProjectIndex++)
	{
		float const c_clipPosition = GetProject()->GetTrimmedTimeToClipMs(uProjectIndex);
		float const c_clipDuration = GetProject()->GetClipDurationTimeMs(uProjectIndex);

		if (c_clipPosition+c_clipDuration >= c_containerPosition && c_clipPosition <= c_containerPosition+c_containerWidth)
		{
			if (c_searchForVisible)
			{
				return uProjectIndex;
			}
		}
		else
		{
			if (!c_searchForVisible)
			{
				return uProjectIndex;
			}
		}
	}

	if (c_searchForVisible)
	{
		return c_startingProjectIndex;
	}
	else
	{
		return (GetProject()->GetClipCount() - 1);
	}
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorThumbnailManager::CheckForVisibility
// PURPOSE:	takes a start and end value and fills them in with the visible 'window'
/////////////////////////////////////////////////////////////////////////////////////////
void CVideoEditorThumbnailManager::CheckForVisibility(u32& out_startClip, u32& out_endClip)
{
	out_startClip = GetFirstClipVisibility(0, true);  // first visible
	out_endClip = GetFirstClipVisibility(out_startClip, false);  // last visible (first invisible minus 1)
}



/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorThumbnailManager::SetupTextureOnComplexObject
// PURPOSE:	sets up a texture on the linked complex object associated with this project
//			index
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorThumbnailManager::SetupTextureOnComplexObject(const u32 uProjectIndex)
{
	if (!GetProject())
		return false;

	const s32 iTimelineIndex = GetLocalIndexFromProjectIndex(uProjectIndex);

	if (iTimelineIndex != -1)
	{
		sTimelineItem *pItem = GetActiveListItem((u32)iTimelineIndex);

		if (pItem)
		{
			const COMPLEX_OBJECT_ID objectId = pItem->object.GetId();

			if ( (GetProject()->IsClipThumbnailLoaded(uProjectIndex)) || (!pItem->bUsesProjectTexture) )
			{
				atString textureNameString;
				GetProject()->getClipTextureName(uProjectIndex, textureNameString);

				if (CScaleformMgr::BeginMethodOnComplexObject(objectId, SF_BASE_CLASS_VIDEO_EDITOR, "loadComponent"))
				{
					if (pItem->bUsesProjectTexture)
					{
						CScaleformMgr::AddParamString(textureNameString.c_str(), false);
						CScaleformMgr::AddParamString(textureNameString.c_str(), false);
					}
					else
					{
						CScaleformMgr::AddParamString(VIDEO_EDITOR_UI_FILENAME, false);
						CScaleformMgr::AddParamString(VIDEO_EDITOR_DEFAULT_ICON, false);
					}
					CScaleformMgr::EndMethod();

					pItem->m_state = THUMBNAIL_STATE_IN_USE;

					return true;
				}
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CVideoEditorThumbnailManager::RemoveTextureOnComplexObject
// PURPOSE:	removes a texture on the linked complex object associated with this project
//			index
/////////////////////////////////////////////////////////////////////////////////////////
bool CVideoEditorThumbnailManager::RemoveTextureOnComplexObject(const u32 uProjectIndex)
{
	const s32 iTimelineIndex = GetLocalIndexFromProjectIndex(uProjectIndex);

	if (iTimelineIndex != -1)
	{
		sTimelineItem *pItem = GetActiveListItem((u32)iTimelineIndex);

		if (pItem)
		{
			const COMPLEX_OBJECT_ID objectId = pItem->object.GetId();

			if (CScaleformMgr::BeginMethodOnComplexObject(objectId, SF_BASE_CLASS_VIDEO_EDITOR, "unloadComponent"))
			{
				CScaleformMgr::EndMethod();

				return true;
			}
		}
	}

	return false;
}

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY

// eof
