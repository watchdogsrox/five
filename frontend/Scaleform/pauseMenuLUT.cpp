// 
// scaleform/pauseMenuLUT.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "pauseMenuLUT.h"
#include "Frontend/PauseMenu.h"
#include "Frontend/ui_channel.h"
#include "Frontend/CMenuBase.h"

using namespace rage;


class CScaleformPauseMenuLUTClass : public sfScaleformFunctionHandler
{
public:

	enum Method
	{
		LUTMETHOD_Ctor = sfScaleformFunctionHandler::CONSTRUCTOR_ID,
		LUTMETHOD_LookUp
	};

	virtual ~CScaleformPauseMenuLUTClass() {}

	// One call method to handle a bunch of function calls means we don't have to create a bunch of GFxFunctionHandler subclasses
	virtual void Call(const Params& params)
	{
		size_t method = size_t(params.pUserData);
		switch(method)
		{
		case LUTMETHOD_Ctor:
			DoConstructor(params);
			break;
		case LUTMETHOD_LookUp:
			DoLookup(params);
			break;
		default:
			sfErrorf("Unknown method ID %" SIZETFMT "d", method);
			break;
		}
	}

	void DoConstructor(const Params& params)
	{
		CHECK_NUM_SF_ARGS("PauseMenuLookup", 0);
	}

	void DoLookup(const Params& params)
	{
		CHECK_NUM_SF_ARGS("lookUp", 1);
		GFxValue& id = params.pArgs[0];
		CHECK_SF_VAR(id, GFxValue::VT_Number);

		MenuScreenId idReference( static_cast<s32>(id.GetNumber()) - PREF_OPTIONS_THRESHOLD );

		GFxValue ctorArgs[4];
		if( idReference != MENU_UNIQUE_ID_INVALID 
			//&& uiVerifyf(CPauseMenu::DynamicMenuExists(),"Actionscript is requesting PauseMenuLUT data, but we've shut down the Pause Menu already!?") 
			&& CPauseMenu::DynamicMenuExists()
			&& CPauseMenu::IsScreenDataValid(idReference))
		{
			CMenuScreen& data = CPauseMenu::GetScreenData(idReference);
			ctorArgs[0] = !data.HasFlag(SF_NoClearRootColumns);
			ctorArgs[1] = !data.HasFlag(SF_NoChangeLayout);
			ctorArgs[2] = !data.HasFlag(SF_NoMenuAdvance);
			ctorArgs[3] = data.HasFlag(SF_CallImmediately);
		}
		else
		{
			uiWarningf("Couldn't find data for %s! You get default values.", idReference.GetParserName() );
			// @TODO data-drive these defaults somehow?
			ctorArgs[0] = false;
			ctorArgs[1] = true;
			ctorArgs[2] = true;
			ctorArgs[3] = false;
		}
		params.pMovie->CreateObject(params.pRetVal, "com.rockstargames.gtav.constants.PauseMenuLUT", ctorArgs, 4);
	}

	virtual void AddStaticMethods(GFxMovieView& movie, GFxValue& classObject, GFxValue& /*originalClass*/) 
	{
		AddMethodToAsObject(movie, classObject, "lookUp",		LUTMETHOD_LookUp);
	}

protected:

};



void CScaleformInstallPauseMenuLUT(sfScaleformMovieView& sfmovie)
{
	sfmovie.InstallFunctionHandler<CScaleformPauseMenuLUTClass>( "_global.com.rockstargames.gtav.constants.PauseMenuLookup", true, false);
}

