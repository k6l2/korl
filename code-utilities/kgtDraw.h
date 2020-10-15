/*
 * User code must define a global KrbApi* called `g_krb` to use this module!
 */
#pragma once
#include "kutil.h"
#include "krb-interface.h"
#include "kgtAssetManager.h"
global_variable const v2f32 QUAD_DEFAULT_TEX_NORMS[] = 
	{ {0,0}, {0,1}, {1,1}, {1,0} };
global_variable const Color4f32 QUAD_WHITE[]  = 
	{ krb::WHITE, krb::WHITE, krb::WHITE, krb::WHITE };
internal void 
	kgtDrawTexture2d(
		KgtAssetManager* kam, KgtAssetIndex kai, 
		const v2f32& position, const v2f32& ratioAnchor, 
		f32 counterClockwiseRadians, const v2f32& scale);
internal void 
	kgtDrawOrigin(const v3f32& scale);
#define USE_IMAGE(kgtAssetManager, kgtAssetIndex) \
	g_krb->useTexture(\
		kgtAssetManagerGetTexture        (kgtAssetManager, kgtAssetIndex), \
		kgtAssetManagerGetTextureMetaData(kgtAssetManager, kgtAssetIndex));
/* useful drawing macros which can work with arbitrarily defined vertex 
 * structures */
#define DRAW_POINTS(mesh, vertexAttribs) \
	g_krb->drawPoints(\
		mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS(mesh, vertexAttribs) \
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define DRAW_TRIS_DYNAMIC(mesh, meshSize, vertexAttribs) \
	g_krb->drawTris(mesh, meshSize, sizeof(mesh[0]), vertexAttribs)
