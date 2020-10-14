#pragma once
#include "kutil.h"
#include "krb-interface.h"
#include "kAssetManager.h"
global_variable const v2f32 QUAD_DEFAULT_TEX_NORMS[] = 
	{ {0,0}, {0,1}, {1,1}, {1,0} };
global_variable const Color4f32 QUAD_WHITE[]  = 
	{ krb::WHITE, krb::WHITE, krb::WHITE, krb::WHITE };
internal void 
	kgtDrawTexture2d(
		KrbApi* krb, KAssetManager* kam, KAssetIndex kai, 
		const v2f32& position, const v2f32& ratioAnchor, 
		f32 counterClockwiseRadians, const v2f32& scale);
#define USE_IMAGE(krbApi, kAssetManager, kAssetIndex) \
	(krbApi)->useTexture(\
		kamGetTexture(kAssetManager, kAssetIndex), \
		kamGetTextureMetaData(kAssetManager, kAssetIndex));
/* useful drawing macros which can work with arbitrarily defined vertex 
 * structures */
#define DRAW_POINTS(krbApi, mesh, vertexAttribs) \
	(krbApi)->drawPoints(\
		mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_LINES(krbApi, mesh, vertexAttribs) \
	(krbApi)->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(krbApi, mesh, vertexAttribs) \
	(krbApi)->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS_DYNAMIC(krbApi, mesh, meshSize, vertexAttribs) \
	(krbApi)->drawTris(mesh, meshSize, sizeof(mesh[0]), vertexAttribs)
