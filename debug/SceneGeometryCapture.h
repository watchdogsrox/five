// ============================
// debug/SceneGeometryCapture.h
// (c) 2013 Rockstar
// ============================

#ifndef _DEBUG_SCENEGEOMETRYCAPTURE_H_
#define _DEBUG_SCENEGEOMETRYCAPTURE_H_

#if __BANK

class CEntity;

namespace SceneGeometryCapture {

void AddWidgets();
void Update();

bool IsCapturingPanorama();
bool ShouldSkipEntity(const CEntity* pEntity);

} // namespace SceneGeometryCapture

#endif // __BANK
#endif // _DEBUG_SCENEGEOMETRYCAPTURE_H_
