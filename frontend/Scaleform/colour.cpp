// 
// scaleform/colour.cpp
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#include "colour.h"

#include "frontend/hud_colour.h"
#include "frontend/Scaleform/ScaleFormMgr.h"

using namespace rage;


class CScaleformColourClass : public sfScaleformFunctionHandler
{
public:

	enum Method
	{
		COLOURMETHOD_Ctor = sfScaleformFunctionHandler::CONSTRUCTOR_ID,
		COLOURMETHOD_Colourise,
		COLOURMETHOD_setHudColour,
		COLOURMETHOD_ColouriseB,
		COLOURMETHOD_RGBToHex,
		COLOURMETHOD_HexTORGB,
		COLOURMETHOD_ApplyHudColour,
		COLOURMETHOD_ApplyHudColourToTF,
	};

	virtual ~CScaleformColourClass() {}

	// One call method to handle a bunch of function calls means we don't have to create a bunch of GFxFunctionHandler subclasses
	virtual void Call(const Params& params)
	{
		size_t method = size_t(params.pUserData);
		switch(method)
		{
		case COLOURMETHOD_Ctor:
			DoConstructor(params);
			break;
		case COLOURMETHOD_Colourise:
			DoColourise(params);
			break;
		case COLOURMETHOD_setHudColour:
			DoSetHudColour(params);
			break;
		case COLOURMETHOD_ColouriseB:
			DoColouriseB(params);
			break;
		case COLOURMETHOD_RGBToHex:
			DoRGBToHex(params);
			break;
		case COLOURMETHOD_HexTORGB:
			DoHexToRGB(params);
			break;
		case COLOURMETHOD_ApplyHudColour:
			DoApplyHudColour(params);
			break;
		case COLOURMETHOD_ApplyHudColourToTF:
			DoApplyHudColourToTF(params);
			break;
		default:
			sfErrorf("Unknown method ID %" SIZETFMT "d", method);
			break;
		}
	}

	void DoConstructor(const Params& params)
	{
		CHECK_NUM_SF_ARGS("Colour", 0);
	}

	void ApplyColorXform(GFxValue& mc, float rf, float gf, float bf)
	{
		GRenderer::Cxform newXform;
		// 0,1,2,3 -> R,G,B,A
		//		Float   M_[4][2];   // [RGBA][mult, add]

		// mult is all 1.0f
		newXform.M_[0][0] = 1.0f;
		newXform.M_[1][0] = 1.0f;
		newXform.M_[2][0] = 1.0f;
		newXform.M_[3][0] = 1.0f;

		// add is (x-255)
		newXform.M_[0][1] = rf - 255.0f;
		newXform.M_[1][1] = gf - 255.0f;
		newXform.M_[2][1] = bf - 255.0f;
		newXform.M_[3][1] = 0.0f;

		mc.SetColorTransform(newXform);
	}

	void ApplyAlpha(GFxValue& mc, float af)
	{
		GFxValue::DisplayInfo dispInfo;
		if (mc.GetDisplayInfo(&dispInfo))
		{
			dispInfo.SetAlpha(af);
			mc.SetDisplayInfo(dispInfo);
		}

	}

	void DoApplyHudColour(const Params& params)
	{
		CHECK_NUM_SF_ARGS("ApplyHudColour", 2);

		GFxValue undefined;
		GFxValue& movieClip = params.ArgCount > 0 ? params.pArgs[0] : undefined;
		GFxValue& colourId = params.ArgCount > 1 ? params.pArgs[1] : undefined;

		CHECK_OPT_SF_VAR(movieClip, GFxValue::VT_DisplayObject);
		if(!CScaleformMgr::VerifyVarType(colourId, GFxValue::VT_Number, STRING(colourId), movieClip))
			return;

		if (!movieClip.IsDefined())
		{
			sfErrorf("%s: %s is undefined", "ApplyHudColour", "movieClip");
			return;
		}

		eHUD_COLOURS iHudColour = (eHUD_COLOURS)(s32)colourId.GetNumber();
		CRGBA colour = CHudColour::GetRGBA(iHudColour);

		ApplyColorXform(movieClip, colour.GetRed(), colour.GetGreen(), colour.GetBlue());
		ApplyAlpha(movieClip, colour.GetAlphaf() * 100.0f);
	}

	void DoApplyHudColourToTF(const Params& params)
	{
		CHECK_NUM_SF_ARGS("ApplyHudColourToTF", 2);

		GFxValue undefined;
		GFxValue& textField = params.ArgCount > 0 ? params.pArgs[0] : undefined;
		GFxValue& colourId = params.ArgCount > 1 ? params.pArgs[1] : undefined;

		CHECK_OPT_SF_VAR(textField, GFxValue::VT_DisplayObject);
		CHECK_SF_VAR(colourId, GFxValue::VT_Number);

		if (!textField.IsDefined())
		{
			sfErrorf("%s: %s is undefined", "ApplyHudColourToTF", "movieClip");
			return;
		}

		eHUD_COLOURS iHudColour = (eHUD_COLOURS)(s32)colourId.GetNumber();
		CRGBA colour = CHudColour::GetRGBA(iHudColour);

		if(textField.IsDisplayObject())
		{

			u32 result = ((int)colour.GetRed() << 16) |
				((int)colour.GetGreen() << 8) |
				((int)colour.GetBlue());

			textField.SetMember("textColor", (Double)result);
			ApplyAlpha(textField, colour.GetAlphaf() * 100.0f);
		}
	}

	void DoColourise(const Params& params)
	{
		if (!sfVerifyf(params.ArgCount == 1 || params.ArgCount == 2 || params.ArgCount == 4 || params.ArgCount == 5,
			"Wrong number of arguments to \"%s\", expected 1, 2, 4 or 5 and got %d", "Colourise", params.ArgCount))
		{											
			return;										
		}											

		GFxValue undefined;

		GFxValue& movieClip = params.ArgCount > 0 ? params.pArgs[0] : undefined;
		GFxValue& r = params.ArgCount > 1 ? params.pArgs[1] : undefined;
		GFxValue& g = params.ArgCount > 2 ? params.pArgs[2] : undefined;
		GFxValue& b = params.ArgCount > 3 ? params.pArgs[3] : undefined;
		GFxValue& a = params.ArgCount > 4 ? params.pArgs[4] : undefined;

		CHECK_OPT_SF_VAR(movieClip, GFxValue::VT_DisplayObject);
		CHECK_OPT_SF_VAR(r, GFxValue::VT_Number);
		CHECK_OPT_SF_VAR(g, GFxValue::VT_Number);
		CHECK_OPT_SF_VAR(b, GFxValue::VT_Number);
		CHECK_OPT_SF_VAR(a, GFxValue::VT_Number);

		// Could make this a macro
		if (!movieClip.IsDefined())
		{
			sfErrorf("%s: %s is undefined", "Colourise", "movieClip");
			return;
		}

		if (r.IsNumber() && g.IsNumber() && b.IsNumber())
		{
			ApplyColorXform(movieClip, (float)r.GetNumber(), (float)g.GetNumber(),  (float)b.GetNumber());
		}
		if (a.IsNumber())
		{
			ApplyAlpha(movieClip, (float)a.GetNumber());
		}
	}

	void DoSetHudColour(const Params& params)
	{
		CHECK_NUM_SF_ARGS("setHudColour", 2);

		GFxValue& hudColourEnum = params.pArgs[0];
		GFxValue& colourObj = params.pArgs[1];

		CHECK_OPT_SF_VAR(hudColourEnum, GFxValue::VT_Number);
		CHECK_SF_VAR(colourObj, GFxValue::VT_Object);

		if (!hudColourEnum.IsDefined())
		{
			sfDebugf1("setHudColor - hudColorEnum is undefined");
			return;
		}

		eHUD_COLOURS iHudColour = (eHUD_COLOURS)(s32)hudColourEnum.GetNumber();

		if (iHudColour > HUD_COLOUR_INVALID && iHudColour < HUD_COLOUR_MAX_COLOURS)
		{
			if (colourObj.IsObject() &&
				colourObj.HasMember("r") &&
				colourObj.HasMember("g") &&
				colourObj.HasMember("b") &&
				colourObj.HasMember("a"))
			{
				CRGBA colour = CHudColour::GetRGBA(iHudColour);

				colourObj.SetMember("r", (Double)colour.GetRed());
				colourObj.SetMember("g", (Double)colour.GetGreen());
				colourObj.SetMember("b", (Double)colour.GetBlue());
				colourObj.SetMember("a", (Double)(colour.GetAlphaf() * 100.0f));
			}
		}
	}

	void DoColouriseB(const Params& params)
	{
		CHECK_NUM_SF_ARGS("ColouriseB", 5);

		GFxValue& movieClip = params.pArgs[0];
		GFxValue& r = params.pArgs[1];
		GFxValue& g = params.pArgs[2];
		GFxValue& b = params.pArgs[3];
		GFxValue& a = params.pArgs[4];

		CHECK_SF_VAR(movieClip, GFxValue::VT_DisplayObject);
		CHECK_SF_VAR(r, GFxValue::VT_Number);
		CHECK_SF_VAR(g, GFxValue::VT_Number);
		CHECK_SF_VAR(b, GFxValue::VT_Number);
		CHECK_SF_VAR(a, GFxValue::VT_Number);

		sfErrorf("ColouriseB is deprecated, don't use it!");
	}

	void DoRGBToHex(const Params& params)
	{
		CHECK_NUM_SF_ARGS("RGBToHex", 3);

		GFxValue& r = params.pArgs[0];
		GFxValue& g = params.pArgs[1];
		GFxValue& b = params.pArgs[2];

		CHECK_SF_VAR(r, GFxValue::VT_Number);
		CHECK_SF_VAR(g, GFxValue::VT_Number);
		CHECK_SF_VAR(b, GFxValue::VT_Number);

		u32 result = ((int)r.GetNumber() << 16) |
					 ((int)g.GetNumber() << 8) |
					 ((int)b.GetNumber());

		params.pRetVal->SetNumber((Double)result);
	}

	void DoHexToRGB(const Params& params)
	{
		CHECK_NUM_SF_ARGS("HexToRGB", 1);

		GFxValue& number = params.pArgs[0];

		CHECK_SF_VAR(number, GFxValue::VT_Number);

		u32 hexVal = (u32)number.GetNumber();

		params.pMovie->CreateObject(params.pRetVal);
		params.pRetVal->SetMember("r", (Double)((hexVal >> 16) & 0xFF));
		params.pRetVal->SetMember("g", (Double)((hexVal >> 8) & 0xFF));
		params.pRetVal->SetMember("b", (Double)((hexVal) & 0xFF));
	}

	virtual void AddStaticMethods(GFxMovieView& movie, GFxValue& classObject, GFxValue& /*originalClass*/) 
	{
		AddMethodToAsObject(movie, classObject, "Colourise",			COLOURMETHOD_Colourise);
		AddMethodToAsObject(movie, classObject, "setHudColour",			COLOURMETHOD_setHudColour);
		AddMethodToAsObject(movie, classObject, "ColouriseB",			COLOURMETHOD_ColouriseB);
		AddMethodToAsObject(movie, classObject, "RGBToHex",				COLOURMETHOD_RGBToHex);
		AddMethodToAsObject(movie, classObject, "HexTORGB",				COLOURMETHOD_HexTORGB);
		AddMethodToAsObject(movie, classObject, "ApplyHudColour",		COLOURMETHOD_ApplyHudColour);
		AddMethodToAsObject(movie, classObject, "ApplyHudColourToTF",	COLOURMETHOD_ApplyHudColourToTF);
	}

protected:

};



void CScaleformInstallColour(sfScaleformMovieView& sfmovie)
{
	sfmovie.InstallFunctionHandler<CScaleformColourClass>("_global.com.rockstargames.ui.utils.Colour", true, false);
}

