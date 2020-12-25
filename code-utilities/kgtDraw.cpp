#include "kgtDraw.h"
#include "kgtVertex.h"
internal void 
	kgtDrawTexture2d(
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
internal void 
	kgtDrawTexture2d(
		KrbTextureHandle kth, const v2u32& imageSize, 
		const v2f32& position, const v2f32& ratioAnchor, 
		f32 counterClockwiseRadians, const v2f32& scale)
{
	g_krb->setModelXform2d(
		position, q32{v3f32::Z, counterClockwiseRadians}, scale);
	g_krb->useTexture(
		kth, 
		kgtAssetManagerGetTextureMetaData(g_kam, KgtAssetIndex::ENUM_SIZE));
	const v2f32 quadSize = 
		{ static_cast<f32>(imageSize.x)
		, static_cast<f32>(imageSize.y) };
	g_krb->drawQuadTextured(quadSize, ratioAnchor, 
	                        QUAD_DEFAULT_TEX_NORMS, QUAD_WHITE);
}
internal void 
	kgtDrawAxes(const v3f32& scale)
{
	g_krb->setModelXform(v3f32::ZERO, q32::IDENTITY, scale);
	local_persist const KgtVertex MESH[] = 
		{ {{0,0,0}, {}, krb::RED  }, {{1,0,0}, {}, krb::RED  }
		, {{0,0,0}, {}, krb::GREEN}, {{0,1,0}, {}, krb::GREEN}
		, {{0,0,0}, {}, krb::BLUE }, {{0,0,1}, {}, krb::BLUE } };
	DRAW_LINES(MESH, KGT_VERTEX_ATTRIBS_NO_TEXTURE);
}
internal void 
	kgtDrawOrigin(
		const v2u32& windowDimensions, const v3f32& camForward, 
		const v3f32& camPosition)
{
	const v2f32 originScreenPos = 
		g_krb->worldToScreen(v3f32::ZERO.elements, 3);
	v2f32 originScreenPosYUp = 
		{originScreenPos.x, windowDimensions.y - originScreenPos.y};
	/* check to see if the origin wrt the camera position is facing in the 
		opposite direction of the camera's forward vector */
	if(camForward.dot(-camPosition) < 0)
	{
		/* if this is the case, then the nearest spot on the screen to the 
			origin is GUARANTEED to be the edge of the screen, with inverted 
			coordinates with respect to the center of the screen */
		originScreenPosYUp.x = windowDimensions.x - originScreenPosYUp.x;
		originScreenPosYUp.y = windowDimensions.y - originScreenPosYUp.y;
		/* force the inverted screen position to the edge of the window by 
			multiplying the normal by the largest window dimension & adding 
			that to the inverted screen position */
		v2f32 originScreenPosYUpFromCenter = 
			kmath::normal(originScreenPosYUp - 
			v2f32{windowDimensions.x/2.f, windowDimensions.y/2.f});
		if(originScreenPosYUpFromCenter.isNearlyZero())
			originScreenPosYUpFromCenter = {0,1};
		originScreenPosYUpFromCenter *= static_cast<f32>(
			kmath::max(windowDimensions.x, windowDimensions.y));
		originScreenPosYUp += originScreenPosYUpFromCenter;
	}
	/* clamp the screen position to the border so the origin indicator is 
		always visible */
	if(originScreenPosYUp.x < 0)
		originScreenPosYUp.x = 0;
	if(originScreenPosYUp.y < 0)
		originScreenPosYUp.y = 0;
	if(originScreenPosYUp.x > windowDimensions.x)
		originScreenPosYUp.x = static_cast<f32>(windowDimensions.x);
	if(originScreenPosYUp.y > windowDimensions.y)
		originScreenPosYUp.y = static_cast<f32>(windowDimensions.y);
	/* set ortho with y+ pointing UP */
	g_krb->setProjectionOrtho(windowDimensions.x, windowDimensions.y, 1);
	/* adjust the view such that the bottom-left corner of the window is the 
		screen-space origin */
	g_krb->viewTranslate({windowDimensions.x/-2.f, windowDimensions.y/-2.f});
	g_krb->setModelXform2d(originScreenPosYUp, q32::IDENTITY, {1,1});
	g_krb->drawCircle(10, 0, krb::TRANSPARENT, krb::WHITE, 32);
}
internal void 
	kgtDrawOrigin(
		const v2u32& windowDimensions, const v3f32& camForward, 
		const v2f32& camPosition2d)
{
	kgtDrawOrigin(
		windowDimensions, camForward, 
		v3f32{camPosition2d.x, camPosition2d.y, 0});
}
internal void 
	kgtDrawCompass(u32 squareSize, const v3f32& camForward)
{
	g_krb->setProjectionOrtho(
		squareSize, squareSize, static_cast<f32>(squareSize));
	g_krb->lookAt(
		v3f32::ZERO.elements, camForward.elements, v3f32::Z.elements);
	kgtDrawAxes({squareSize/2.f, squareSize/2.f, squareSize/2.f});
}
internal void 
	kgtDrawBoxLines2d(
		const v2f32& cornerA, const v2f32& cornerB, const Color4f32& color)
{
	const KgtVertex mesh[] = 
		{ {{cornerA.x, cornerA.y, 0}, {}, color}
		, {{cornerA.x, cornerB.y, 0}, {}, color}
		, {{cornerA.x, cornerA.y, 0}, {}, color}
		, {{cornerB.x, cornerA.y, 0}, {}, color}
		, {{cornerB.x, cornerB.y, 0}, {}, color}
		, {{cornerB.x, cornerA.y, 0}, {}, color}
		, {{cornerB.x, cornerB.y, 0}, {}, color}
		, {{cornerA.x, cornerB.y, 0}, {}, color} };
	g_krb->setModelXform2d(v2f32::ZERO, q32::IDENTITY, {1.f, 1.f});
	g_krb->drawLines(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), 
	                 KGT_VERTEX_ATTRIBS_NO_TEXTURE);
}
internal void 
	kgtDrawBox2d(
		const v2f32& cornerMinusXPlusY, const v2f32& size, 
		const Color4f32& color)
{
	const KgtVertex mesh[] = 
		// upper-left triangle //
		{ {{cornerMinusXPlusY.x,cornerMinusXPlusY.y,0}, {}, color} 
		, {{cornerMinusXPlusY.x,cornerMinusXPlusY.y - size.y,0}, {}, color} 
		, {{cornerMinusXPlusY.x + size.x,cornerMinusXPlusY.y,0}, {}, color} 
		// lower-right triangle //
		, {{cornerMinusXPlusY.x + size.x,cornerMinusXPlusY.y - size.y,0}, 
			{}, color} 
		, {{cornerMinusXPlusY.x + size.x,cornerMinusXPlusY.y,0}, {}, color} 
		, {{cornerMinusXPlusY.x,cornerMinusXPlusY.y - size.y,0}, {}, color} 
		};
	g_krb->drawTris(mesh, CARRAY_SIZE(mesh), sizeof(mesh[0]), 
	                KGT_VERTEX_ATTRIBS_NO_TEXTURE);
}