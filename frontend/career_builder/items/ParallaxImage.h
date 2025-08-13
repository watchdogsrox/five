/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ParallaxImage.h
// PURPOSE : Quick wrapper for the parallax image background
//
// AUTHOR  : Charalampos.Koundourakis
// STARTED : July 2021
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef UI_MAIN_IMAGE_H
#define UI_MAIN_IMAGE_H

#include "frontend/page_deck/uiPageConfig.h"
#if UI_PAGE_DECK_ENABLED

// game
#include "frontend/page_deck/layout/PageLayoutItem.h"

enum eMotionType
{
	DEFAULT_MOTION_TYPE = 0,
	STORY_MOTION_TYPE,
	ONLINE_MOTION_TYPE,
	SUMMARY_MOTION_TYPE,
	MIGRATION_MOTION_TYPE,
	ERROR_MOTION_TYPE
};

class CParallaxImage final : public CPageLayoutItem
{
public:
	CParallaxImage()
	{}
	~CParallaxImage() {}
	
	void PlayAnimation();
	void ResetAnimation();
	void StartNewParallax(char const * const backgroundImageTxtDictionary, char const * const backgroundImageTexture, char const * const frontImageTxtDictionary, char const * const frontImageTexture, eMotionType motionType);
	void SetImage(char const * const backgroundImageTxtDictionary, char const * const backgroundImageTexture, char const * const frontImageTxtDictionary, char const * const frontImageTexture, eMotionType motionType);
	void SetBackgroundDetails(char const * const titleText, char const * const descriptionText);

private: // declarations and variables
	NON_COPYABLE(CParallaxImage);

private: // methods
	char const * GetSymbol() const override { return "gridItem3x4Parallax"; }
	
};

#endif // UI_PAGE_DECK_ENABLED

#endif // UI_MAIN_IMAGE_H
