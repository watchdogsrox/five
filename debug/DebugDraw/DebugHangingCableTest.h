// =======================================
// debug/DebugDraw/DebugHangingCableTest.h
// (c) 2011 RockstarNorth
// =======================================

#ifndef _DEBUG_DEBUGDRAW_DEBUGHANGINGCABLETEST_H_
#define _DEBUG_DEBUGDRAW_DEBUGHANGINGCABLETEST_H_

#if __DEV

#define HANGING_CABLE_TEST (1)

#if HANGING_CABLE_TEST

class CHangingCableTestInterface
{
public:
	static void InitWidgets();
	static void Update();
	static void Render();
};

#endif // HANGING_CABLE_TEST
#endif // __DEV
#endif // _DEBUG_DEBUGDRAW_DEBUGHANGINGCABLETEST_H_
