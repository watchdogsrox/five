#ifndef WATERMARK_H
#define WATERMARK_H

#define WATERMARKED_BUILD ( RSG_PC && !__BANK && !__FINAL && !(!__DEV && !__FINAL))

#if WATERMARKED_BUILD 
//need to turn off optimizations, as this string is not referenced anyway and would have been dead stripped otherwise
#pragma optimize("",off) // PRAGMA-OPTIMIZE-ALLOW
//this has to match values at the top of gta5/tools/scripts/watermarking/watermarker.py
static const char* watermarkDummy = "255,255,255 040,040 0ce168e9add4baa830e2249afca6a63af33b192e1636d4efadb85bbd2aeb4f32c4b083a5d83c3222b052be5bf403ea4a3d34a133c598b36ac14ed50c5d04ea41";
#pragma optimize("",on)

static Color32 GetWatermarkColour()
{
	atString watermarkString(watermarkDummy);
	atString colourAsString;

	colourAsString.Set(watermarkString, 0, 11);

	atArray<atString> values;

	colourAsString.Split(values, ',');

	return Color32(atoi(values[0].c_str()), atoi(values[1].c_str()), atoi(values[2].c_str()));
}

static Vec2f GetWatermarkPostion()
{
	atString watermarkString(watermarkDummy);
	atString posAsString;

	posAsString.Set(watermarkString, 11, 18);

	atArray<atString> values;

	posAsString.Split(values, ',');

	return Vec2f((float)atoi(values[0].c_str()) / 100.0f, (float)atoi(values[1].c_str()) / 100.0f);
}


#endif //WATERMARKED_BUILD

#endif //WATERMARK_H