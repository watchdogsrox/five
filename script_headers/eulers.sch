// Defines the rotation order to be used for Euler composition and decomposition.
// NOTE: This is NOT the order in which the angles are specified in a vector, but rather the order in which the rotations should be applied.
// NOTE: This enumeration must match the code equivalent in script/script.h, which in turn matches the RAGE enumeration(s).
ENUM EULER_ROT_ORDER
	EULER_XYZ = 0,
	EULER_XZY,
	EULER_YXZ,
	EULER_YZX,
	EULER_ZXY,
	EULER_ZYX,
	EULER_MAX
ENDENUM
