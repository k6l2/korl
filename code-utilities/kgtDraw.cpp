#include "kgtDraw.h"
#include "kgtVertex.h"
internal void kgtDrawTexture2d(
	KgtAssetIndex kai, const v2f32& position, const v2f32& ratioAnchor, 
	f32 counterClockwiseRadians, const v2f32& scale)
{
	g_krb->setModelXform2d(
		position, q32{v3f32::Z, counterClockwiseRadians}, scale);
	g_krb->useTexture(
		kgtAssetManagerGetTexture(g_kam, kai), 
		kgtAssetManagerGetTextureMetaData(g_kam, kai));
	const v2u32 imageSize = kgtAssetManagerGetImageSize(g_kam, kai);
	const v2f32 quadSize = 
		{ static_cast<f32>(imageSize.x)
		, static_cast<f32>(imageSize.y) };
	g_krb->drawQuadTextured(quadSize, ratioAnchor, 
	                        QUAD_DEFAULT_TEX_NORMS, QUAD_WHITE);
}
internal void kgtDrawOrigin(const v3f32& scale)
{
	g_krb->setModelXform(v3f32::ZERO, q32::IDENTITY, scale);
	local_persist const KgtVertex MESH[] = 
		{ {{0,0,0}, {}, krb::RED  }, {{1,0,0}, {}, krb::RED  }
		, {{0,0,0}, {}, krb::GREEN}, {{0,1,0}, {}, krb::GREEN}
		, {{0,0,0}, {}, krb::BLUE }, {{0,0,1}, {}, krb::BLUE } };
	DRAW_LINES(MESH, KGT_VERTEX_ATTRIBS_NO_TEXTURE);
}
