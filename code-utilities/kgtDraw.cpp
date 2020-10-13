#include "kgtDraw.h"
internal void 
	kgtDrawTexture2d(
		KrbApi* krb, KAssetManager* kam, KAssetIndex kai, 
		const v2f32& position, const v2f32& ratioAnchor, 
		f32 counterClockwiseRadians, const v2f32& scale)
{
	krb->setModelXform2d(
		position, kQuaternion(v3f32::Z, counterClockwiseRadians), scale);
	krb->useTexture(kamGetTexture(kam, kai), kamGetTextureMetaData(kam, kai));
	const v2u32 imageSize = kamGetImageSize(kam, kai);
	const v2f32 quadSize = 
		{ static_cast<f32>(imageSize.x)
		, static_cast<f32>(imageSize.y) };
	krb->drawQuadTextured(quadSize, ratioAnchor, 
	                      QUAD_DEFAULT_TEX_NORMS, QUAD_WHITE);
}
