/*
 * User code must define the following global variables to use this module:
 * - KrbApi* g_krb
 * - KgtAssetManager* g_kam
 */
#pragma once
#include "kutil.h"
#include "krb-interface.h"
global_variable const RgbaF32 KGT_DRAW_QUAD_WHITE[]  = 
	{ krb::WHITE, krb::WHITE, krb::WHITE, krb::WHITE };
internal void 
	kgtDrawTexture2d(
		KgtAssetIndex kai, const v2f32& position, const v2f32& ratioAnchor, 
		f32 counterClockwiseRadians, const v2f32& scale, 
		const RgbaF32 colors[4] = KGT_DRAW_QUAD_WHITE);
internal void 
	kgtDrawTexture2d(
		KrbTextureHandle kth, const v2u32& imageSize, 
		const v2f32& position, const v2f32& ratioAnchor, 
		f32 counterClockwiseRadians, const v2f32& scale);
internal void 
	kgtDrawAxes(const v3f32& scale);
/**
 * draw a small circle on the screen at the closest point of the world-space 
 * origin 
 */
internal void 
	kgtDrawOrigin(
		const v2u32& windowDimensions, const v3f32& camForward, 
		const v3f32& camPosition);
/**
 * draw a small circle on the screen at the closest point of the world-space 
 * origin 
 */
internal void 
	kgtDrawOrigin(
		const v2u32& windowDimensions, const v3f32& camForward, 
		const v2f32& camPosition2d);
internal void 
	kgtDrawCompass(u32 squareSize, const v3f32& camForward);
internal void 
	kgtDrawBoxLines2d(
		const v2f32& cornerA, const v2f32& cornerB, const RgbaF32& color);
internal void 
	kgtDrawBox2d(
		const v2f32& cornerMinusXPlusY, const v2f32& size, 
		const RgbaF32& color);
#define KGT_USE_IMAGE(kgtAssetIndex) \
	g_krb->useTexture(\
		kgtAssetManagerGetTexture        (g_kam, kgtAssetIndex), \
		kgtAssetManagerGetTextureMetaData(g_kam, kgtAssetIndex));
/* useful drawing macros which can work with arbitrarily defined vertex 
 * structures */
#define KGT_DRAW_POINTS(mesh, vertexAttribs) \
	g_krb->drawPoints(\
		mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define KGT_DRAW_LINES(mesh, vertexAttribs) \
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define KGT_DRAW_TRIS(mesh, vertexAttribs) \
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), vertexAttribs)
#define KGT_DRAW_TRIS_DYNAMIC(mesh, meshSize, vertexAttribs) \
	g_krb->drawTris(mesh, meshSize, sizeof(mesh[0]), vertexAttribs)
