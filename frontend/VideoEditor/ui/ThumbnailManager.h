/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ThumbnailManager.h
// PURPOSE : manages requests of thumbnails in the video editor ui
// AUTHOR  : Derek Payne
// STARTED : 05/06/2014
//
/////////////////////////////////////////////////////////////////////////////////

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_EDITOR_THUMBNAIL_MANAGER_H
#define VIDEO_EDITOR_THUMBNAIL_MANAGER_H


#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/VideoEditor/ui/Editor.h"

enum eTimelineThumbnailState
{
	THUMBNAIL_STATE_NOT_VISIBLE = 0,
	THUMBNAIL_STATE_PENDING_REQUEST,
	THUMBNAIL_STATE_REQUESTED,
	THUMBNAIL_STATE_LOADED,
	THUMBNAIL_STATE_PENDING_FAILED,
	THUMBNAIL_STATE_FAILED,
	THUMBNAIL_STATE_NEEDED,
	THUMBNAIL_STATE_IN_USE,
	THUMBNAIL_STATE_NO_LONGER_NEEDED
};

#define MIN_CLIP_SIZE_FOR_TEXTURE (25)  // 25 pixels

struct sTimelineItem
{
	CComplexObject object;
	ePLAYHEAD_TYPE type;
	u32 uIndexToProject;
	bool bUsesProjectTexture;

	eTimelineThumbnailState m_state;
};

class CVideoEditorThumbnailManager
{
public:
	CVideoEditorThumbnailManager()
	{

	}

	void Init();
	void Shutdown();
	void Update();

	bool SetupTextureOnComplexObject(const u32 uProjectIndex);

	void ReleaseThumbnail( u32 const uProjectIndex );
	void ReleaseAll();

private:

	virtual s32 GetNonVisibleBufferMs() const = 0;
	virtual u32 GetActiveListCount() const = 0;
	virtual sTimelineItem *GetActiveListItem(const u32 uIndex) const = 0;
	virtual void SetupThumbnail(const u32 uProjectIndex) const = 0;
	virtual void ReportFailed(const u32 uProjectIndex) const = 0;
	virtual s32 GetWindowPosMs() const = 0;
	virtual s32 GetWindowSizeMs() const = 0;
	virtual s32 GetLocalIndexFromProjectIndex(const u32 uProjectIndex) const = 0;
	virtual void SetSpinner(const u32 uProjectIndex, const bool bOnOff) const = 0;
	virtual bool CheckForExcludedClip(s32 const c_projectIndex) const = 0;
	virtual bool CanMakeRequests() const = 0;

	void CullTexturesNotInVisibleWindow();
	void ProcessThumbnailAwaitingSetup();
	void ProcessThumbnailsFailed();
	void ProcessThumbnailsNeeded();
	bool ProcessThumbnailsNotNeeded();
	void UnRequestThumbnail(const u32 uThumbnailIndex);
	void RequestAndUnrequestThumbnails();
	u32 GetFirstClipVisibility(u32 const c_startingProjectIndex, bool const c_searchForVisible);
	void CheckForVisibility(u32& out_startClip, u32& out_endClip);
	bool RemoveTextureOnComplexObject(const u32 uProjectIndex);
	CVideoEditorProject *GetProject() { return CVideoEditorUi::GetProject(); }

#if __BANK
	u8 m_Id;
#endif // __BANK
};


#endif // VIDEO_EDITOR_THUMBNAIL_MANAGER_H

#endif // #if defined( GTA_REPLAY ) && GTA_REPLAY

// eof
