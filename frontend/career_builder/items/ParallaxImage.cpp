/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ParallaxImage.cpp
// PURPOSE : Quick wrapper for the parallax image background
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#include "ParallaxImage.h"
#if UI_PAGE_DECK_ENABLED

// framework
#include "fwutil/xmacro.h"

// game
#include "frontend/Scaleform/ScaleFormMgr.h"
#include "frontend/Scaleform/ScaleFormComplexObjectMgr.h"
#include "frontend/ui_channel.h"

void CParallaxImage::PlayAnimation()
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "PLAY_ANIMATION");
		CScaleformMgr::EndMethod();
	}
}

void CParallaxImage::ResetAnimation()
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "PLAY_ANIMATION");
		CScaleformMgr::EndMethod();
	}
}

void CParallaxImage::StartNewParallax(char const * const backgroundImageTxtDictionary, char const * const backgroundImageTexture, char const * const frontImageTxtDictionary, char const * const frontImageTexture, eMotionType motionType)
{
	SetImage(backgroundImageTxtDictionary, backgroundImageTexture, frontImageTxtDictionary, frontImageTexture, motionType);
	ResetAnimation();
}

void CParallaxImage::SetImage(char const * const backgroundImageTxtDictionary, char const * const backgroundImageTexture, char const * const frontImageTxtDictionary, char const * const frontImageTexture, eMotionType motionType)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "ADD_PARALLAX_ITEM");
		CScaleformMgr::AddParamString(backgroundImageTxtDictionary, false);
		CScaleformMgr::AddParamString(backgroundImageTexture, false);
		CScaleformMgr::AddParamString(frontImageTxtDictionary, false);
		CScaleformMgr::AddParamString(frontImageTexture, false);
		CScaleformMgr::AddParamInt(motionType);
		CScaleformMgr::EndMethod();
	}
}
void CParallaxImage::SetBackgroundDetails(char const * const titleText, char const * const descriptionText)
{
	CComplexObject& displayObject = GetDisplayObject();
	if (displayObject.IsActive())
	{
		CScaleformMgr::BeginMethodOnComplexObject(displayObject.GetId(), SF_BASE_CLASS_GENERIC, "SET_GRID_ITEM_DETAILS");
		CScaleformMgr::AddParamString(titleText, false);
		CScaleformMgr::AddParamString(descriptionText, false);
		CScaleformMgr::EndMethod();
	}
}

#endif // UI_PAGE_DECK_ENABLED


