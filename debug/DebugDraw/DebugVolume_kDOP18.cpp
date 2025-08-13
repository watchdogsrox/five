// ======================================
// debug/DebugDraw/DebugVolume_kDOP18.cpp
// (c) 2011 RockstarNorth
// ======================================

#if __BANK

#include "bank/bank.h"

#include "fwmaths/vectorutil.h"
#include "fwutil/xmacro.h"

#include "renderer/Util/Util.h" // sadly, this relies on old Vector3 utility functions
#include "renderer/color.h"
#include "debug/DebugDraw/DebugVolume_kDOP18.h"

RENDER_OPTIMISATIONS()

// ================================================================================================

CkDOP18Face::CkDOP18Face()
{
	m_plane      = Vector4(0,0,0,0);
	m_pointCount = 0;
}

CkDOP18Face::CkDOP18Face(
	const Vector4& plane,
	const Vector3& p0,
	const Vector3& p1,
	const Vector3& p2,
	const Vector3& p3,
	const Vector3& p4,
	const Vector3& p5,
	const Vector3& p6,
	const Vector3& p7,
	float          tolerance,
	bool           bFlipped
	)
{
	m_plane      = plane;
	m_pointCount = 0;

	if (bFlipped)
	{
		AddPoint(p7, tolerance);
		AddPoint(p6, tolerance);
		AddPoint(p5, tolerance);
		AddPoint(p4, tolerance);
		AddPoint(p3, tolerance);
		AddPoint(p2, tolerance);
		AddPoint(p1, tolerance);
		AddPoint(p0, tolerance);
	}
	else
	{
		AddPoint(p0, tolerance);
		AddPoint(p1, tolerance);
		AddPoint(p2, tolerance);
		AddPoint(p3, tolerance);
		AddPoint(p4, tolerance);
		AddPoint(p5, tolerance);
		AddPoint(p6, tolerance);
		AddPoint(p7, tolerance);
	}

	while (m_pointCount > 0 && MaxElement(Abs(RCC_VEC3V(m_points[0])) - Abs(RCC_VEC3V(m_points[m_pointCount - 1]))).Getf() <= tolerance) // remove degenerate points
	{
		m_pointCount--;
	}
}

void CkDOP18Face::AddPoint(const Vector3& p, float tolerance)
{
	if (m_pointCount < kDOP18_MAX_POINTS)
	{
		if (m_pointCount == 0 || MaxElement(Abs(RCC_VEC3V(p)) - Abs(RCC_VEC3V(m_points[m_pointCount - 1]))).Getf() > tolerance)
		{
			m_points[m_pointCount++] = p;
		}
	}
}

bool CkDOP18Face::HasArea(float tolerance) const // true if face has non-zero area
{
	if (m_pointCount > 2)
	{
		Vector3 p0 = m_points[m_pointCount - 2];
		Vector3 p1 = m_points[m_pointCount - 1];

		for (int i = 0; i < m_pointCount; i++)
		{
			Vector3 p2 = m_points[i];

			Vector3 c;
			c.Cross(p2 - p1, p1 - p0);

			if (c.Mag2() > tolerance)
			{
				return true;
			}

			p0 = p1;
			p1 = p2;
		}
	}

	return false;
}

CkDOP18::CkDOP18()
{
	Clear();
}

void CkDOP18::Clear()
{
	bInited = false;
}

void CkDOP18::AddPoints(const Vector3* points, int pointCount)
{
	for (int i = 0; i < pointCount; i++)
	{
		AddPoint(points[i]);
	}
}

void CkDOP18::AddPoint(const Vector3& p)
{
	const float x = p.x;
	const float y = p.y;
	const float z = p.z;

	if (!bInited)
	{
		bInited = true;

		// faces
		px = +x;
		nx = -x;
		py = +y;
		ny = -y;
		pz = +z;
		nz = -z;

		// edges
		pxpy = +x+y;
		nxpy = -x+y;
		pxny = +x-y;
		nxny = -x-y;
		pypz = +y+z;
		nypz = -y+z;
		pynz = +y-z;
		nynz = -y-z;
		pzpx = +z+x;
		nzpx = -z+x;
		pznx = +z-x;
		nznx = -z-x;

		// corners
		//pxpypz = +x+y+z;
		//nxpypz = -x+y+z;
		//pxnypz = +x-y+z;
		//nxnypz = -x-y+z;
		//pxpynz = +x+y-z;
		//nxpynz = -x+y-z;
		//pxnynz = +x-y-z;
		//nxnynz = -x-y-z;
	}
	else
	{
		// faces
		px = Max<float>(px, +x);
		nx = Max<float>(nx, -x);
		py = Max<float>(py, +y);
		ny = Max<float>(ny, -y);
		pz = Max<float>(pz, +z);
		nz = Max<float>(nz, -z);

		// edges
		pxpy = Max<float>(pxpy, +x+y);
		nxpy = Max<float>(nxpy, -x+y);
		pxny = Max<float>(pxny, +x-y);
		nxny = Max<float>(nxny, -x-y);
		pypz = Max<float>(pypz, +y+z);
		nypz = Max<float>(nypz, -y+z);
		pynz = Max<float>(pynz, +y-z);
		nynz = Max<float>(nynz, -y-z);
		pzpx = Max<float>(pzpx, +z+x);
		nzpx = Max<float>(nzpx, -z+x);
		pznx = Max<float>(pznx, +z-x);
		nznx = Max<float>(nznx, -z-x);

		// corners
		//pxpypz = Max<float>(pxpypz, +x+y+z);
		//nxpypz = Max<float>(nxpypz, -x+y+z);
		//pxnypz = Max<float>(pxnypz, +x-y+z);
		//nxnypz = Max<float>(nxnypz, -x-y+z);
		//pxpynz = Max<float>(pxpynz, +x+y-z);
		//nxpynz = Max<float>(nxpynz, -x+y-z);
		//pxnynz = Max<float>(pxnynz, +x-y-z);
		//nxnynz = Max<float>(nxnynz, -x-y-z);
	}
}

void CkDOP18::RemoveDiagonalZFaces()
{
//	pxpy = +px+py;
//	nxpy = +nx+py;
//	pxny = +px+ny;
//	nxny = +nx+ny;
	pypz = +py+pz;
	nypz = +ny+pz;
	pynz = +py+nz;
	nynz = +ny+nz;
	pzpx = +pz+px;
	nzpx = +nz+px;
	pznx = +pz+nx;
	nznx = +nz+nx;
}

static float CkDOP18_SimulateQuantisation_Diagonal(float x, float x0, float x1, float scale, float inv, bool bNormaliseDiagonals)
{
	if (bNormaliseDiagonals)
	{
		x = ceilf(Clamp<float>(256.0f*(x - x0)/(x1 - x0), 0.0f, 255.0f));
		x = x0 + (x/256.0f)*(x1 - x0);
		x = Clamp<float>(x, x0, x1);
	}
	else if (scale != 0.0f)
	{
		x = Clamp<float>(ceilf(x*inv)*scale, x0, x1);
	}

	return x;
}

void CkDOP18::SimulateQuantisation(float scale, bool bNormaliseDiagonals)
{
	float inv = 1.0f;

	if (scale != 0.0f)
	{
		inv = 1.0f/scale;

		px = ceilf(px*inv)*scale;
		nx = ceilf(nx*inv)*scale;
		py = ceilf(py*inv)*scale;
		ny = ceilf(ny*inv)*scale;
		pz = ceilf(pz*inv)*scale;
		nz = ceilf(nz*inv)*scale;
	}

	pxpy = CkDOP18_SimulateQuantisation_Diagonal(pxpy, -nx-ny, +px+py, scale, inv, bNormaliseDiagonals);
	nxpy = CkDOP18_SimulateQuantisation_Diagonal(nxpy, -px-ny, +nx+py, scale, inv, bNormaliseDiagonals);
	pxny = CkDOP18_SimulateQuantisation_Diagonal(pxny, -nx-py, +px+ny, scale, inv, bNormaliseDiagonals);
	nxny = CkDOP18_SimulateQuantisation_Diagonal(nxny, -px-py, +nx+ny, scale, inv, bNormaliseDiagonals);
	pypz = CkDOP18_SimulateQuantisation_Diagonal(pypz, -ny-nz, +py+pz, scale, inv, bNormaliseDiagonals);
	nypz = CkDOP18_SimulateQuantisation_Diagonal(nypz, -py-nz, +ny+pz, scale, inv, bNormaliseDiagonals);
	pynz = CkDOP18_SimulateQuantisation_Diagonal(pynz, -ny-pz, +py+nz, scale, inv, bNormaliseDiagonals);
	nynz = CkDOP18_SimulateQuantisation_Diagonal(nynz, -py-pz, +ny+nz, scale, inv, bNormaliseDiagonals);
	pzpx = CkDOP18_SimulateQuantisation_Diagonal(pzpx, -nz-nx, +pz+px, scale, inv, bNormaliseDiagonals);
	nzpx = CkDOP18_SimulateQuantisation_Diagonal(nzpx, -pz-nx, +nz+px, scale, inv, bNormaliseDiagonals);
	pznx = CkDOP18_SimulateQuantisation_Diagonal(pznx, -nz-px, +pz+nx, scale, inv, bNormaliseDiagonals);
	nznx = CkDOP18_SimulateQuantisation_Diagonal(nznx, -pz-px, +nz+nx, scale, inv, bNormaliseDiagonals);

	//pxpypz = CkDOP18_SimulateQuantisation_Diagonal(pxpypz, -nx-ny-nz, +px+py+pz, scale, inv, bNormaliseDiagonals);
	//nxpypz = CkDOP18_SimulateQuantisation_Diagonal(nxpypz, -px-ny-nz, +nx+py+pz, scale, inv, bNormaliseDiagonals);
	//pxnypz = CkDOP18_SimulateQuantisation_Diagonal(pxnypz, -nx-py-nz, +px+ny+pz, scale, inv, bNormaliseDiagonals);
	//nxnypz = CkDOP18_SimulateQuantisation_Diagonal(nxnypz, -px-py-nz, +nx+ny+pz, scale, inv, bNormaliseDiagonals);
	//pxpynz = CkDOP18_SimulateQuantisation_Diagonal(pxpynz, -nx-ny-pz, +px+py+nz, scale, inv, bNormaliseDiagonals);
	//nxpynz = CkDOP18_SimulateQuantisation_Diagonal(nxpynz, -px-ny-pz, +nx+py+nz, scale, inv, bNormaliseDiagonals);
	//pxnynz = CkDOP18_SimulateQuantisation_Diagonal(pxnynz, -nx-py-pz, +px+ny+nz, scale, inv, bNormaliseDiagonals);
	//nxnynz = CkDOP18_SimulateQuantisation_Diagonal(nxnynz, -px-py-pz, +nx+ny+nz, scale, inv, bNormaliseDiagonals);
}

static void CkDOP18_AddFace(CkDOP18Face* faces, int& faceCount, const CkDOP18Face& face, float tolerance)
{
	if (face.HasArea(tolerance))
	{
		faces[faceCount++] = face;
	}
}

int CkDOP18::BuildFaces(CkDOP18Face* faces, Vector3* points96, float tolerance) const
{
	if (!bInited)
	{
		return 0;
	}

	// xy
	const float pxpy_px = pxpy - px;
	const float nxpy_nx = nxpy - nx;
	const float pxny_px = pxny - px;
	const float nxny_nx = nxny - nx;

	const float pxpy_py = pxpy - py;
	const float pxny_ny = pxny - ny;
	const float nxpy_py = nxpy - py;
	const float nxny_ny = nxny - ny;

	// yz
	const float pypz_py = pypz - py;
	const float nypz_ny = nypz - ny;
	const float pynz_py = pynz - py;
	const float nynz_ny = nynz - ny;

	const float pypz_pz = pypz - pz;
	const float pynz_nz = pynz - nz;
	const float nypz_pz = nypz - pz;
	const float nynz_nz = nynz - nz;

	// zx
	const float pzpx_pz = pzpx - pz;
	const float nzpx_nz = nzpx - nz;
	const float pznx_pz = pznx - pz;
	const float nznx_nz = nznx - nz;

	const float pzpx_px = pzpx - px;
	const float pznx_nx = pznx - nx;
	const float nzpx_px = nzpx - px;
	const float nznx_nx = nznx - nx;

	// x-y-z
	const float px_py_pz_z = Min<float>(pzpx_px, pypz - pxpy_px); const Vector3 px_py_pz_0(+px, +pxpy_px, +px_py_pz_z);
	const float nx_py_pz_z = Min<float>(pznx_nx, pypz - nxpy_nx); const Vector3 nx_py_pz_0(-nx, +nxpy_nx, +nx_py_pz_z);
	const float px_ny_pz_z = Min<float>(pzpx_px, nypz - pxny_px); const Vector3 px_ny_pz_0(+px, -pxny_px, +px_ny_pz_z);
	const float nx_ny_pz_z = Min<float>(pznx_nx, nypz - nxny_nx); const Vector3 nx_ny_pz_0(-nx, -nxny_nx, +nx_ny_pz_z);

	const float px_py_nz_z = Min<float>(nzpx_px, pynz - pxpy_px); const Vector3 px_py_nz_0(+px, +pxpy_px, -px_py_nz_z);
	const float nx_py_nz_z = Min<float>(nznx_nx, pynz - nxpy_nx); const Vector3 nx_py_nz_0(-nx, +nxpy_nx, -nx_py_nz_z);
	const float px_ny_nz_z = Min<float>(nzpx_px, nynz - pxny_px); const Vector3 px_ny_nz_0(+px, -pxny_px, -px_ny_nz_z);
	const float nx_ny_nz_z = Min<float>(nznx_nx, nynz - nxny_nx); const Vector3 nx_ny_nz_0(-nx, -nxny_nx, -nx_ny_nz_z);

	// y-x-z
	const float py_px_pz_z = Min<float>(pypz_py, pzpx - pxpy_py); const Vector3 py_px_pz_0(+pxpy_py, +py, +py_px_pz_z);
	const float ny_px_pz_z = Min<float>(nypz_ny, pzpx - pxny_ny); const Vector3 ny_px_pz_0(+pxny_ny, -ny, +ny_px_pz_z);
	const float py_nx_pz_z = Min<float>(pypz_py, pznx - nxpy_py); const Vector3 py_nx_pz_0(-nxpy_py, +py, +py_nx_pz_z);
	const float ny_nx_pz_z = Min<float>(nypz_ny, pznx - nxny_ny); const Vector3 ny_nx_pz_0(-nxny_ny, -ny, +ny_nx_pz_z);

	const float py_px_nz_z = Min<float>(pynz_py, nzpx - pxpy_py); const Vector3 py_px_nz_0(+pxpy_py, +py, -py_px_nz_z);
	const float ny_px_nz_z = Min<float>(nynz_ny, nzpx - pxny_ny); const Vector3 ny_px_nz_0(+pxny_ny, -ny, -ny_px_nz_z);
	const float py_nx_nz_z = Min<float>(pynz_py, nznx - nxpy_py); const Vector3 py_nx_nz_0(-nxpy_py, +py, -py_nx_nz_z);
	const float ny_nx_nz_z = Min<float>(nynz_ny, nznx - nxny_ny); const Vector3 ny_nx_nz_0(-nxny_ny, -ny, -ny_nx_nz_z);

	// z-x-y
	const float pz_px_py_y = Min<float>(pypz_pz, pxpy - pzpx_pz); const Vector3 pz_px_py_0(+pzpx_pz, +pz_px_py_y, +pz);
	const float nz_px_py_y = Min<float>(pynz_nz, pxpy - nzpx_nz); const Vector3 nz_px_py_0(+nzpx_nz, +nz_px_py_y, -nz);
	const float pz_nx_py_y = Min<float>(pypz_pz, nxpy - pznx_pz); const Vector3 pz_nx_py_0(-pznx_pz, +pz_nx_py_y, +pz);
	const float nz_nx_py_y = Min<float>(pynz_nz, nxpy - nznx_nz); const Vector3 nz_nx_py_0(-nznx_nz, +nz_nx_py_y, -nz);

	const float pz_px_ny_y = Min<float>(nypz_pz, pxny - pzpx_pz); const Vector3 pz_px_ny_0(+pzpx_pz, -pz_px_ny_y, +pz);
	const float nz_px_ny_y = Min<float>(nynz_nz, pxny - nzpx_nz); const Vector3 nz_px_ny_0(+nzpx_nz, -nz_px_ny_y, -nz);
	const float pz_nx_ny_y = Min<float>(nypz_pz, nxny - pznx_pz); const Vector3 pz_nx_ny_0(-pznx_pz, -pz_nx_ny_y, +pz);
	const float nz_nx_ny_y = Min<float>(nynz_nz, nxny - nznx_nz); const Vector3 nz_nx_ny_0(-nznx_nz, -nz_nx_ny_y, -nz);

	// x-z-y
	const float px_pz_py_y = Min<float>(pxpy_px, pypz - pzpx_px); const Vector3 px_pz_py_0(+px, +px_pz_py_y, +pzpx_px);
	const float nx_pz_py_y = Min<float>(nxpy_nx, pypz - pznx_nx); const Vector3 nx_pz_py_0(-nx, +nx_pz_py_y, +pznx_nx);
	const float px_nz_py_y = Min<float>(pxpy_px, pynz - nzpx_px); const Vector3 px_nz_py_0(+px, +px_nz_py_y, -nzpx_px);
	const float nx_nz_py_y = Min<float>(nxpy_nx, pynz - nznx_nx); const Vector3 nx_nz_py_0(-nx, +nx_nz_py_y, -nznx_nx);

	const float px_pz_ny_y = Min<float>(pxny_px, nypz - pzpx_px); const Vector3 px_pz_ny_0(+px, -px_pz_ny_y, +pzpx_px);
	const float nx_pz_ny_y = Min<float>(nxny_nx, nypz - pznx_nx); const Vector3 nx_pz_ny_0(-nx, -nx_pz_ny_y, +pznx_nx);
	const float px_nz_ny_y = Min<float>(pxny_px, nynz - nzpx_px); const Vector3 px_nz_ny_0(+px, -px_nz_ny_y, -nzpx_px);
	const float nx_nz_ny_y = Min<float>(nxny_nx, nynz - nznx_nx); const Vector3 nx_nz_ny_0(-nx, -nx_nz_ny_y, -nznx_nx);

	// y-z-x
	const float py_pz_px_x = Min<float>(pxpy_py, pzpx - pypz_py); const Vector3 py_pz_px_0(+py_pz_px_x, +py, +pypz_py);
	const float ny_pz_px_x = Min<float>(pxny_ny, pzpx - nypz_ny); const Vector3 ny_pz_px_0(+ny_pz_px_x, -ny, +nypz_ny);
	const float py_nz_px_x = Min<float>(pxpy_py, nzpx - pynz_py); const Vector3 py_nz_px_0(+py_nz_px_x, +py, -pynz_py);
	const float ny_nz_px_x = Min<float>(pxny_ny, nzpx - nynz_ny); const Vector3 ny_nz_px_0(+ny_nz_px_x, -ny, -nynz_ny);

	const float py_pz_nx_x = Min<float>(nxpy_py, pznx - pypz_py); const Vector3 py_pz_nx_0(-py_pz_nx_x, +py, +pypz_py);
	const float ny_pz_nx_x = Min<float>(nxny_ny, pznx - nypz_ny); const Vector3 ny_pz_nx_0(-ny_pz_nx_x, -ny, +nypz_ny);
	const float py_nz_nx_x = Min<float>(nxpy_py, nznx - pynz_py); const Vector3 py_nz_nx_0(-py_nz_nx_x, +py, -pynz_py);
	const float ny_nz_nx_x = Min<float>(nxny_ny, nznx - nynz_ny); const Vector3 ny_nz_nx_0(-ny_nz_nx_x, -ny, -nynz_ny);

	// z-y-x
	const float pz_py_px_x = Min<float>(pzpx_pz, pxpy - pypz_pz); const Vector3 pz_py_px_0(+pz_py_px_x, +pypz_pz, +pz);
	const float nz_py_px_x = Min<float>(nzpx_nz, pxpy - pynz_nz); const Vector3 nz_py_px_0(+nz_py_px_x, +pynz_nz, -nz);
	const float pz_ny_px_x = Min<float>(pzpx_pz, pxny - nypz_pz); const Vector3 pz_ny_px_0(+pz_ny_px_x, -nypz_pz, +pz);
	const float nz_ny_px_x = Min<float>(nzpx_nz, pxny - nynz_nz); const Vector3 nz_ny_px_0(+nz_ny_px_x, -nynz_nz, -nz);

	const float pz_py_nx_x = Min<float>(pznx_pz, nxpy - pypz_pz); const Vector3 pz_py_nx_0(-pz_py_nx_x, +pypz_pz, +pz);
	const float nz_py_nx_x = Min<float>(nznx_nz, nxpy - pynz_nz); const Vector3 nz_py_nx_0(-nz_py_nx_x, +pynz_nz, -nz);
	const float pz_ny_nx_x = Min<float>(pznx_pz, nxny - nypz_pz); const Vector3 pz_ny_nx_0(-pz_ny_nx_x, -nypz_pz, +pz);
	const float nz_ny_nx_x = Min<float>(nznx_nz, nxny - nynz_nz); const Vector3 nz_ny_nx_0(-nz_ny_nx_x, -nynz_nz, -nz);

	float d;

	// xy
	d = Min<float>(px - pxpy_py, pz - py_px_pz_z, (pzpx - pxpy_py - py_px_pz_z)/2); const Vector3 py_px_pz_1 = py_px_pz_0 + Vector3(+d,-d,+d);
	d = Min<float>(nx - nxpy_py, pz - py_nx_pz_z, (pznx - nxpy_py - py_nx_pz_z)/2); const Vector3 py_nx_pz_1 = py_nx_pz_0 + Vector3(-d,-d,+d);
	d = Min<float>(px - pxny_ny, pz - ny_px_pz_z, (pzpx - pxny_ny - ny_px_pz_z)/2); const Vector3 ny_px_pz_1 = ny_px_pz_0 + Vector3(+d,+d,+d);
	d = Min<float>(nx - nxny_ny, pz - ny_nx_pz_z, (pznx - nxny_ny - ny_nx_pz_z)/2); const Vector3 ny_nx_pz_1 = ny_nx_pz_0 + Vector3(-d,+d,+d);

	d = Min<float>(px - pxpy_py, nz - py_px_nz_z, (nzpx - pxpy_py - py_px_nz_z)/2); const Vector3 py_px_nz_1 = py_px_nz_0 + Vector3(+d,-d,-d);
	d = Min<float>(nx - nxpy_py, nz - py_nx_nz_z, (nznx - nxpy_py - py_nx_nz_z)/2); const Vector3 py_nx_nz_1 = py_nx_nz_0 + Vector3(-d,-d,-d);
	d = Min<float>(px - pxny_ny, nz - ny_px_nz_z, (nzpx - pxny_ny - ny_px_nz_z)/2); const Vector3 ny_px_nz_1 = ny_px_nz_0 + Vector3(+d,+d,-d);
	d = Min<float>(nx - nxny_ny, nz - ny_nx_nz_z, (nznx - nxny_ny - ny_nx_nz_z)/2); const Vector3 ny_nx_nz_1 = ny_nx_nz_0 + Vector3(-d,+d,-d);

	d = Min<float>(py - pxpy_px, pz - px_py_pz_z, (pypz - pxpy_px - px_py_pz_z)/2); const Vector3 px_py_pz_1 = px_py_pz_0 + Vector3(-d,+d,+d);
	d = Min<float>(ny - pxny_px, pz - px_ny_pz_z, (nypz - pxny_px - px_ny_pz_z)/2); const Vector3 px_ny_pz_1 = px_ny_pz_0 + Vector3(-d,-d,+d);
	d = Min<float>(py - nxpy_nx, pz - nx_py_pz_z, (pypz - nxpy_nx - nx_py_pz_z)/2); const Vector3 nx_py_pz_1 = nx_py_pz_0 + Vector3(+d,+d,+d);
	d = Min<float>(ny - nxny_nx, pz - nx_ny_pz_z, (nypz - nxny_nx - nx_ny_pz_z)/2); const Vector3 nx_ny_pz_1 = nx_ny_pz_0 + Vector3(+d,-d,+d);

	d = Min<float>(py - pxpy_px, nz - px_py_nz_z, (pynz - pxpy_px - px_py_nz_z)/2); const Vector3 px_py_nz_1 = px_py_nz_0 + Vector3(-d,+d,-d);
	d = Min<float>(ny - pxny_px, nz - px_ny_nz_z, (nynz - pxny_px - px_ny_nz_z)/2); const Vector3 px_ny_nz_1 = px_ny_nz_0 + Vector3(-d,-d,-d);
	d = Min<float>(py - nxpy_nx, nz - nx_py_nz_z, (pynz - nxpy_nx - nx_py_nz_z)/2); const Vector3 nx_py_nz_1 = nx_py_nz_0 + Vector3(+d,+d,-d);
	d = Min<float>(ny - nxny_nx, nz - nx_ny_nz_z, (nynz - nxny_nx - nx_ny_nz_z)/2); const Vector3 nx_ny_nz_1 = nx_ny_nz_0 + Vector3(+d,-d,-d);

	// yz
	d = Min<float>(py - pypz_pz, px - pz_py_px_x, (pxpy - pypz_pz - pz_py_px_x)/2); const Vector3 pz_py_px_1 = pz_py_px_0 + Vector3(+d,+d,-d);
	d = Min<float>(ny - nypz_pz, px - pz_ny_px_x, (pxny - nypz_pz - pz_ny_px_x)/2); const Vector3 pz_ny_px_1 = pz_ny_px_0 + Vector3(+d,-d,-d);
	d = Min<float>(py - pynz_nz, px - nz_py_px_x, (pxpy - pynz_nz - nz_py_px_x)/2); const Vector3 nz_py_px_1 = nz_py_px_0 + Vector3(+d,+d,+d);
	d = Min<float>(ny - nynz_nz, px - nz_ny_px_x, (pxny - nynz_nz - nz_ny_px_x)/2); const Vector3 nz_ny_px_1 = nz_ny_px_0 + Vector3(+d,-d,+d);

	d = Min<float>(py - pypz_pz, nx - pz_py_nx_x, (nxpy - pypz_pz - pz_py_nx_x)/2); const Vector3 pz_py_nx_1 = pz_py_nx_0 + Vector3(-d,+d,-d);
	d = Min<float>(ny - nypz_pz, nx - pz_ny_nx_x, (nxny - nypz_pz - pz_ny_nx_x)/2); const Vector3 pz_ny_nx_1 = pz_ny_nx_0 + Vector3(-d,-d,-d);
	d = Min<float>(py - pynz_nz, nx - nz_py_nx_x, (nxpy - pynz_nz - nz_py_nx_x)/2); const Vector3 nz_py_nx_1 = nz_py_nx_0 + Vector3(-d,+d,+d);
	d = Min<float>(ny - nynz_nz, nx - nz_ny_nx_x, (nxny - nynz_nz - nz_ny_nx_x)/2); const Vector3 nz_ny_nx_1 = nz_ny_nx_0 + Vector3(-d,-d,+d);

	d = Min<float>(pz - pypz_py, px - py_pz_px_x, (pzpx - pypz_py - py_pz_px_x)/2); const Vector3 py_pz_px_1 = py_pz_px_0 + Vector3(+d,-d,+d);
	d = Min<float>(nz - pynz_py, px - py_nz_px_x, (nzpx - pynz_py - py_nz_px_x)/2); const Vector3 py_nz_px_1 = py_nz_px_0 + Vector3(+d,-d,-d);
	d = Min<float>(pz - nypz_ny, px - ny_pz_px_x, (pzpx - nypz_ny - ny_pz_px_x)/2); const Vector3 ny_pz_px_1 = ny_pz_px_0 + Vector3(+d,+d,+d);
	d = Min<float>(nz - nynz_ny, px - ny_nz_px_x, (nzpx - nynz_ny - ny_nz_px_x)/2); const Vector3 ny_nz_px_1 = ny_nz_px_0 + Vector3(+d,+d,-d);

	d = Min<float>(pz - pypz_py, nx - py_pz_nx_x, (pznx - pypz_py - py_pz_nx_x)/2); const Vector3 py_pz_nx_1 = py_pz_nx_0 + Vector3(-d,-d,+d);
	d = Min<float>(nz - pynz_py, nx - py_nz_nx_x, (nznx - pynz_py - py_nz_nx_x)/2); const Vector3 py_nz_nx_1 = py_nz_nx_0 + Vector3(-d,-d,-d);
	d = Min<float>(pz - nypz_ny, nx - ny_pz_nx_x, (pznx - nypz_ny - ny_pz_nx_x)/2); const Vector3 ny_pz_nx_1 = ny_pz_nx_0 + Vector3(-d,+d,+d);
	d = Min<float>(nz - nynz_ny, nx - ny_nz_nx_x, (nznx - nynz_ny - ny_nz_nx_x)/2); const Vector3 ny_nz_nx_1 = ny_nz_nx_0 + Vector3(-d,+d,-d);

	// zx
	d = Min<float>(pz - pzpx_px, py - px_pz_py_y, (pypz - pzpx_px - px_pz_py_y)/2); const Vector3 px_pz_py_1 = px_pz_py_0 + Vector3(-d,+d,+d);
	d = Min<float>(nz - nzpx_px, py - px_nz_py_y, (pynz - nzpx_px - px_nz_py_y)/2); const Vector3 px_nz_py_1 = px_nz_py_0 + Vector3(-d,+d,-d);
	d = Min<float>(pz - pznx_nx, py - nx_pz_py_y, (pypz - pznx_nx - nx_pz_py_y)/2); const Vector3 nx_pz_py_1 = nx_pz_py_0 + Vector3(+d,+d,+d);
	d = Min<float>(nz - nznx_nx, py - nx_nz_py_y, (pynz - nznx_nx - nx_nz_py_y)/2); const Vector3 nx_nz_py_1 = nx_nz_py_0 + Vector3(+d,+d,-d);

	d = Min<float>(pz - pzpx_px, ny - px_pz_ny_y, (nypz - pzpx_px - px_pz_ny_y)/2); const Vector3 px_pz_ny_1 = px_pz_ny_0 + Vector3(-d,-d,+d);
	d = Min<float>(nz - nzpx_px, ny - px_nz_ny_y, (nynz - nzpx_px - px_nz_ny_y)/2); const Vector3 px_nz_ny_1 = px_nz_ny_0 + Vector3(-d,-d,-d);
	d = Min<float>(pz - pznx_nx, ny - nx_pz_ny_y, (nypz - pznx_nx - nx_pz_ny_y)/2); const Vector3 nx_pz_ny_1 = nx_pz_ny_0 + Vector3(+d,-d,+d);
	d = Min<float>(nz - nznx_nx, ny - nx_nz_ny_y, (nynz - nznx_nx - nx_nz_ny_y)/2); const Vector3 nx_nz_ny_1 = nx_nz_ny_0 + Vector3(+d,-d,-d);

	d = Min<float>(px - pzpx_pz, py - pz_px_py_y, (pxpy - pzpx_pz - pz_px_py_y)/2); const Vector3 pz_px_py_1 = pz_px_py_0 + Vector3(+d,+d,-d);
	d = Min<float>(nx - pznx_pz, py - pz_nx_py_y, (nxpy - pznx_pz - pz_nx_py_y)/2); const Vector3 pz_nx_py_1 = pz_nx_py_0 + Vector3(-d,+d,-d);
	d = Min<float>(px - nzpx_nz, py - nz_px_py_y, (pxpy - nzpx_nz - nz_px_py_y)/2); const Vector3 nz_px_py_1 = nz_px_py_0 + Vector3(+d,+d,+d);
	d = Min<float>(nx - nznx_nz, py - nz_nx_py_y, (nxpy - nznx_nz - nz_nx_py_y)/2); const Vector3 nz_nx_py_1 = nz_nx_py_0 + Vector3(-d,+d,+d);

	d = Min<float>(px - pzpx_pz, ny - pz_px_ny_y, (pxny - pzpx_pz - pz_px_ny_y)/2); const Vector3 pz_px_ny_1 = pz_px_ny_0 + Vector3(+d,-d,-d);
	d = Min<float>(nx - pznx_pz, ny - pz_nx_ny_y, (nxny - pznx_pz - pz_nx_ny_y)/2); const Vector3 pz_nx_ny_1 = pz_nx_ny_0 + Vector3(-d,-d,-d);
	d = Min<float>(px - nzpx_nz, ny - nz_px_ny_y, (pxny - nzpx_nz - nz_px_ny_y)/2); const Vector3 nz_px_ny_1 = nz_px_ny_0 + Vector3(+d,-d,+d);
	d = Min<float>(nx - nznx_nz, ny - nz_nx_ny_y, (nxny - nznx_nz - nz_nx_ny_y)/2); const Vector3 nz_nx_ny_1 = nz_nx_ny_0 + Vector3(-d,-d,+d);

	int faceCount = 0;

	if (points96)
	{
		int pointCount = 0;

		// x-y-z
		points96[pointCount++] = px_py_pz_0;
		points96[pointCount++] = nx_py_pz_0;
		points96[pointCount++] = px_ny_pz_0;
		points96[pointCount++] = nx_ny_pz_0;

		points96[pointCount++] = px_py_nz_0;
		points96[pointCount++] = nx_py_nz_0;
		points96[pointCount++] = px_ny_nz_0;
		points96[pointCount++] = nx_ny_nz_0;

		// y-x-z
		points96[pointCount++] = py_px_pz_0;
		points96[pointCount++] = ny_px_pz_0;
		points96[pointCount++] = py_nx_pz_0;
		points96[pointCount++] = ny_nx_pz_0;

		points96[pointCount++] = py_px_nz_0;
		points96[pointCount++] = ny_px_nz_0;
		points96[pointCount++] = py_nx_nz_0;
		points96[pointCount++] = ny_nx_nz_0;

		// z-x-y
		points96[pointCount++] = pz_px_py_0;
		points96[pointCount++] = nz_px_py_0;
		points96[pointCount++] = pz_nx_py_0;
		points96[pointCount++] = nz_nx_py_0;

		points96[pointCount++] = pz_px_ny_0;
		points96[pointCount++] = nz_px_ny_0;
		points96[pointCount++] = pz_nx_ny_0;
		points96[pointCount++] = nz_nx_ny_0;

		// x-z-y
		points96[pointCount++] = px_pz_py_0;
		points96[pointCount++] = nx_pz_py_0;
		points96[pointCount++] = px_nz_py_0;
		points96[pointCount++] = nx_nz_py_0;

		points96[pointCount++] = px_pz_ny_0;
		points96[pointCount++] = nx_pz_ny_0;
		points96[pointCount++] = px_nz_ny_0;
		points96[pointCount++] = nx_nz_ny_0;

		// y-z-x
		points96[pointCount++] = py_pz_px_0;
		points96[pointCount++] = ny_pz_px_0;
		points96[pointCount++] = py_nz_px_0;
		points96[pointCount++] = ny_nz_px_0;

		points96[pointCount++] = py_pz_nx_0;
		points96[pointCount++] = ny_pz_nx_0;
		points96[pointCount++] = py_nz_nx_0;
		points96[pointCount++] = ny_nz_nx_0;

		// z-y-x
		points96[pointCount++] = pz_py_px_0;
		points96[pointCount++] = nz_py_px_0;
		points96[pointCount++] = pz_ny_px_0;
		points96[pointCount++] = nz_ny_px_0;

		points96[pointCount++] = pz_py_nx_0;
		points96[pointCount++] = nz_py_nx_0;
		points96[pointCount++] = pz_ny_nx_0;
		points96[pointCount++] = nz_ny_nx_0;

		// xy
		points96[pointCount++] = py_px_pz_1;
		points96[pointCount++] = py_nx_pz_1;
		points96[pointCount++] = ny_px_pz_1;
		points96[pointCount++] = ny_nx_pz_1;

		points96[pointCount++] = py_px_nz_1;
		points96[pointCount++] = py_nx_nz_1;
		points96[pointCount++] = ny_px_nz_1;
		points96[pointCount++] = ny_nx_nz_1;

		points96[pointCount++] = px_py_pz_1;
		points96[pointCount++] = px_ny_pz_1;
		points96[pointCount++] = nx_py_pz_1;
		points96[pointCount++] = nx_ny_pz_1;

		points96[pointCount++] = px_py_nz_1;
		points96[pointCount++] = px_ny_nz_1;
		points96[pointCount++] = nx_py_nz_1;
		points96[pointCount++] = nx_ny_nz_1;

		// yz
		points96[pointCount++] = pz_py_px_1;
		points96[pointCount++] = pz_ny_px_1;
		points96[pointCount++] = nz_py_px_1;
		points96[pointCount++] = nz_ny_px_1;

		points96[pointCount++] = pz_py_nx_1;
		points96[pointCount++] = pz_ny_nx_1;
		points96[pointCount++] = nz_py_nx_1;
		points96[pointCount++] = nz_ny_nx_1;

		points96[pointCount++] = py_pz_px_1;
		points96[pointCount++] = py_nz_px_1;
		points96[pointCount++] = ny_pz_px_1;
		points96[pointCount++] = ny_nz_px_1;

		points96[pointCount++] = py_pz_nx_1;
		points96[pointCount++] = py_nz_nx_1;
		points96[pointCount++] = ny_pz_nx_1;
		points96[pointCount++] = ny_nz_nx_1;

		// zx
		points96[pointCount++] = px_pz_py_1;
		points96[pointCount++] = px_nz_py_1;
		points96[pointCount++] = nx_pz_py_1;
		points96[pointCount++] = nx_nz_py_1;

		points96[pointCount++] = px_pz_ny_1;
		points96[pointCount++] = px_nz_ny_1;
		points96[pointCount++] = nx_pz_ny_1;
		points96[pointCount++] = nx_nz_ny_1;

		points96[pointCount++] = pz_px_py_1;
		points96[pointCount++] = pz_nx_py_1;
		points96[pointCount++] = nz_px_py_1;
		points96[pointCount++] = nz_nx_py_1;

		points96[pointCount++] = pz_px_ny_1;
		points96[pointCount++] = pz_nx_ny_1;
		points96[pointCount++] = nz_px_ny_1;
		points96[pointCount++] = nz_nx_ny_1;
	}

	if (faces) // note: the plane normals are intentionally left un-normalised
	{
		// x,y,z
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(+1, 0, 0,px  ), px_py_nz_0,px_py_pz_0,px_pz_py_0,px_pz_ny_0,px_ny_pz_0,px_ny_nz_0,px_nz_ny_0,px_nz_py_0, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(-1, 0, 0,nx  ), nx_py_nz_0,nx_py_pz_0,nx_pz_py_0,nx_pz_ny_0,nx_ny_pz_0,nx_ny_nz_0,nx_nz_ny_0,nx_nz_py_0, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0,+1, 0,py  ), py_pz_nx_0,py_pz_px_0,py_px_pz_0,py_px_nz_0,py_nz_px_0,py_nz_nx_0,py_nx_nz_0,py_nx_pz_0, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0,-1, 0,ny  ), ny_pz_nx_0,ny_pz_px_0,ny_px_pz_0,ny_px_nz_0,ny_nz_px_0,ny_nz_nx_0,ny_nx_nz_0,ny_nx_pz_0, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0, 0,+1,pz  ), pz_px_ny_0,pz_px_py_0,pz_py_px_0,pz_py_nx_0,pz_nx_py_0,pz_nx_ny_0,pz_ny_nx_0,pz_ny_px_0, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0, 0,-1,nz  ), nz_px_ny_0,nz_px_py_0,nz_py_px_0,nz_py_nx_0,nz_nx_py_0,nz_nx_ny_0,nz_ny_nx_0,nz_ny_px_0, tolerance, false), tolerance);

		// xy,yz,zx
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(+1,+1, 0,pxpy), py_px_nz_0,py_px_pz_0,py_px_pz_1,px_py_pz_1,px_py_pz_0,px_py_nz_0,px_py_nz_1,py_px_nz_1, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(-1,+1, 0,nxpy), py_nx_nz_0,py_nx_pz_0,py_nx_pz_1,nx_py_pz_1,nx_py_pz_0,nx_py_nz_0,nx_py_nz_1,py_nx_nz_1, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(+1,-1, 0,pxny), ny_px_nz_0,ny_px_pz_0,ny_px_pz_1,px_ny_pz_1,px_ny_pz_0,px_ny_nz_0,px_ny_nz_1,ny_px_nz_1, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(-1,-1, 0,nxny), ny_nx_nz_0,ny_nx_pz_0,ny_nx_pz_1,nx_ny_pz_1,nx_ny_pz_0,nx_ny_nz_0,nx_ny_nz_1,ny_nx_nz_1, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0,+1,+1,pypz), pz_py_nx_0,pz_py_px_0,pz_py_px_1,py_pz_px_1,py_pz_px_0,py_pz_nx_0,py_pz_nx_1,pz_py_nx_1, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0,-1,+1,nypz), pz_ny_nx_0,pz_ny_px_0,pz_ny_px_1,ny_pz_px_1,ny_pz_px_0,ny_pz_nx_0,ny_pz_nx_1,pz_ny_nx_1, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0,+1,-1,pynz), nz_py_nx_0,nz_py_px_0,nz_py_px_1,py_nz_px_1,py_nz_px_0,py_nz_nx_0,py_nz_nx_1,nz_py_nx_1, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4( 0,-1,-1,nynz), nz_ny_nx_0,nz_ny_px_0,nz_ny_px_1,ny_nz_px_1,ny_nz_px_0,ny_nz_nx_0,ny_nz_nx_1,nz_ny_nx_1, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(+1, 0,+1,pzpx), px_pz_ny_0,px_pz_py_0,px_pz_py_1,pz_px_py_1,pz_px_py_0,pz_px_ny_0,pz_px_ny_1,px_pz_ny_1, tolerance, true ), tolerance); // flip
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(+1, 0,-1,nzpx), px_nz_ny_0,px_nz_py_0,px_nz_py_1,nz_px_py_1,nz_px_py_0,nz_px_ny_0,nz_px_ny_1,px_nz_ny_1, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(-1, 0,+1,pznx), nx_pz_ny_0,nx_pz_py_0,nx_pz_py_1,pz_nx_py_1,pz_nx_py_0,pz_nx_ny_0,pz_nx_ny_1,nx_pz_ny_1, tolerance, false), tolerance);
		CkDOP18_AddFace(faces, faceCount, CkDOP18Face(Vector4(-1, 0,-1,nznx), nx_nz_ny_0,nx_nz_py_0,nx_nz_py_1,nz_nx_py_1,nz_nx_py_0,nz_nx_ny_0,nz_nx_ny_1,nx_nz_ny_1, tolerance, true ), tolerance); // flip
	}

	return faceCount;
}

bool CkDOP18::IntersectQ(const Vector3& point) const
{
	CkDOP18 kDOP18;
	kDOP18.AddPoint(point);
	return IntersectQ(kDOP18); // inefficient, debugging code
}

bool CkDOP18::IntersectQ(const CkDOP18& kDOP18) const
{
	if (px + kDOP18.nx < 0 ||
		nx + kDOP18.px < 0 ||
		py + kDOP18.ny < 0 ||
		ny + kDOP18.py < 0 ||
		pz + kDOP18.nz < 0 ||
		nz + kDOP18.pz < 0)
	{
		return false;
	}

	if (pxpy + kDOP18.nxny < 0 ||
		nxpy + kDOP18.pxny < 0 ||
		pxny + kDOP18.nxpy < 0 ||
		nxny + kDOP18.pxpy < 0 ||
		pypz + kDOP18.nynz < 0 ||
		nypz + kDOP18.pynz < 0 ||
		pynz + kDOP18.nypz < 0 ||
		nynz + kDOP18.pypz < 0 ||
		pzpx + kDOP18.nznx < 0 ||
		nzpx + kDOP18.pznx < 0 ||
		pznx + kDOP18.nzpx < 0 ||
		nznx + kDOP18.pzpx < 0)
	{
		return false;
	}

//	if (pxpypz + kDOP18.nxnynz < 0 ||
//		nxpypz + kDOP18.pxnynz < 0 ||
//		pxnypz + kDOP18.nxpynz < 0 ||
//		nxnypz + kDOP18.pxpynz < 0 ||
//		pxpynz + kDOP18.nxnypz < 0 ||
//		nxpynz + kDOP18.pxnypz < 0 ||
//		pxnynz + kDOP18.nxpypz < 0 ||
//		nxnynz + kDOP18.pxpypz < 0)
//	{
//		return false;
//	}

	return true;
}

#endif // __BANK
