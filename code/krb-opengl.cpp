/** This code should all be a completely platform-independent implementation of 
 * the Kyle's Renderer Backend interface.  
*/
#include "krb-interface.h"
///@TODO: maybe figure out a more platform-independent way of getting OpenGL
///       function definitions.
#ifdef _WIN32
       #include "win32-main.h"
       #include <GL/GL.h>
#endif
#include "krb-opengl-extensions.h"
internal GLenum krbOglCheckErrors(const char* file, int line)
{
	GLenum errorCode;
	do
	{
		errorCode = glGetError();
		if(errorCode != GL_NO_ERROR)
		{
			KLOG(ERROR, "[%s|%i] OpenGL ERROR=%i", kutil::fileName(file), line, 
			     errorCode);
		}
	} while (errorCode != GL_NO_ERROR);
	return errorCode;
}
#define GL_CHECK_ERROR() krbOglCheckErrors(__FILE__, __LINE__)
internal KRB_BEGIN_FRAME(krbBeginFrame)
{
	glClearColor(clamped0_1_red, clamped0_1_green, clamped0_1_blue, 1.f);
	/* I use right-handed homogeneous clip space, so depth buffer values 
		farthest away from the camera are -1, instead of the default OpenGL 
		value of 1 */
	glClearDepth(-1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	// empty the modelview stack //
	{
		GLint modelViewStackDepth;
		glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
		for(; modelViewStackDepth > 1; modelViewStackDepth--)
		{
			glPopMatrix();
		}
	}
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_CHECK_ERROR();
	*(krb::g_context) = {};
}
internal KRB_SET_DEPTH_TESTING(krbSetDepthTesting)
{
	if(enable)
	{
		glEnable(GL_DEPTH_TEST);
		/* default OpenGL uses a left-handed homogeneous clip space, whereas I 
			use right-handed, so I have to invert the depth function from the 
			default of GL_LESS */
		glDepthFunc(GL_GREATER);
	}
	else
		glDisable(GL_DEPTH_TEST);
	GL_CHECK_ERROR();
}
internal KRB_SET_BACKFACE_CULLING(krbSetBackfaceCulling)
{
	if(enable)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	GL_CHECK_ERROR();
}
internal KRB_SET_WIREFRAME(krbSetWireframe)
{
	if(enable)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho)
{
	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, windowSizeX, windowSizeY);
	const f32 left  = -static_cast<f32>(windowSizeX)/2;
	const f32 right =  static_cast<f32>(windowSizeX)/2;
	const f32 bottom = -static_cast<f32>(windowSizeY)/2;
	const f32 top    =  static_cast<f32>(windowSizeY)/2;
	const f32 zNear =  halfDepth;
	const f32 zFar  = -halfDepth;
	/* http://www.songho.ca/opengl/gl_projectionmatrix.html */
	m4x4f32 projectionMatrix = m4x4f32::IDENTITY;
	projectionMatrix.r0c0 = 2 / (right - left);
	projectionMatrix.r0c3 = -(right + left) / (right - left);
	projectionMatrix.r1c1 = 2 / (top - bottom);
	projectionMatrix.r1c3 = -(top + bottom) / (top - bottom);
	projectionMatrix.r2c2 = -2 / (zFar - zNear);
	projectionMatrix.r2c3 = -(zFar + zNear) / (zFar - zNear);
	glLoadTransposeMatrixf(projectionMatrix.elements);
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(krbSetProjectionOrthoFixedHeight)
{
	/*
		w / fixedHeight == windowAspectRatio
	*/
	const f32 windowAspectRatio = windowSizeY == 0
		? 1.f : static_cast<f32>(windowSizeX) / windowSizeY;
	const GLsizei viewportWidth = 
		static_cast<GLsizei>(windowAspectRatio * fixedHeight);
	const f32 left  = -static_cast<f32>(viewportWidth)/2;
	const f32 right =  static_cast<f32>(viewportWidth)/2;
	const f32 bottom = -static_cast<f32>(fixedHeight)/2;
	const f32 top    =  static_cast<f32>(fixedHeight)/2;
	const f32 zNear =  halfDepth;
	const f32 zFar  = -halfDepth;
	glViewport(0, 0, windowSizeX, windowSizeY);
	glMatrixMode(GL_PROJECTION);
	/* http://www.songho.ca/opengl/gl_projectionmatrix.html */
	m4x4f32 projectionMatrix = m4x4f32::IDENTITY;
	projectionMatrix.r0c0 = 2 / (right - left);
	projectionMatrix.r0c3 = -(right + left) / (right - left);
	projectionMatrix.r1c1 = 2 / (top - bottom);
	projectionMatrix.r1c3 = -(top + bottom) / (top - bottom);
	projectionMatrix.r2c2 = -2 / (zFar - zNear);
	projectionMatrix.r2c3 = -(zFar + zNear) / (zFar - zNear);
	glLoadTransposeMatrixf(projectionMatrix.elements);
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_FOV(krbSetProjectionFov)
{
	const v2u32* windowDimensions = reinterpret_cast<const v2u32*>(windowSize);
	glViewport(0, 0, windowDimensions->x, windowDimensions->y);
	const f32 aspectRatio = 
		static_cast<f32>(windowDimensions->x)/windowDimensions->y;
	kassert(!kmath::isNearlyZero(aspectRatio));
	kassert(!kmath::isNearlyEqual(clipNear, clipFar) && clipFar > clipNear);
	const f32 horizonFovRadians = horizonFovDegrees*PI32/180;
	const f32 tanHalfFov = tanf(horizonFovRadians / 2);
	m4x4f32 projectionMatrix = {};
	// http://ogldev.org/www/tutorial12/tutorial12.html
	// http://www.songho.ca/opengl/gl_projectionmatrix.html
	projectionMatrix.r0c0 = 1 / (aspectRatio * tanHalfFov);
	projectionMatrix.r1c1 = 1 / tanHalfFov;
	/* I negate the Z row calculation to maintain RIGHT-HANDED coordinates! */
	projectionMatrix.r2c2 = (clipFar + clipNear) / (clipFar - clipNear);
	projectionMatrix.r2c3 = 2*clipFar*clipNear   / (clipFar - clipNear);
	projectionMatrix.r3c2 = -1;
	glMatrixMode(GL_PROJECTION);
	glLoadTransposeMatrixf(projectionMatrix.elements);
	GL_CHECK_ERROR();
}
internal KRB_LOOK_AT(krbLookAt)
{
	const v3f32*const eye = reinterpret_cast<const v3f32*>(v3f32_eye);
	// https://www.3dgep.com/understanding-the-view-matrix/
	/* camera Z-axis points AWAY from the target to maintain RIGHT-HANDED 
		coordinates! */
	const v3f32 camAxisZ = kmath::normal(
		*eye - *reinterpret_cast<const v3f32*>(v3f32_target));
	const v3f32 camAxisX = kmath::normal(
		reinterpret_cast<const v3f32*>(v3f32_worldUp)->cross(camAxisZ));
	const v3f32 camAxisY = camAxisZ.cross(camAxisX);
	m4x4f32 mView = m4x4f32::IDENTITY;
	mView.r0c0 = camAxisX.x; mView.r0c1 = camAxisX.y; mView.r0c2 = camAxisX.z;
	mView.r1c0 = camAxisY.x; mView.r1c1 = camAxisY.y; mView.r1c2 = camAxisY.z;
	mView.r2c0 = camAxisZ.x; mView.r2c1 = camAxisZ.y; mView.r2c2 = camAxisZ.z;
	mView.r0c3 = -camAxisX.dot(*eye);
	mView.r1c3 = -camAxisY.dot(*eye);
	mView.r2c3 = -camAxisZ.dot(*eye);
	glMatrixMode(GL_MODELVIEW);
	glLoadTransposeMatrixf(mView.elements);
	glPushMatrix();
	GL_CHECK_ERROR();
}
internal KRB_DRAW_LINES(krbDrawLines)
{
	kassert(vertexCount % 2 == 0);
	kassert(vertexAttribOffsets.position_3f32 < vertexStride);
	if(vertexAttribOffsets.texCoord_2f32 >= vertexStride)
		glDisable(GL_TEXTURE_2D);
	else
		glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(vertexAttribOffsets.color_4f32 >= vertexStride)
		glColor4f(krb::g_context->defaultColor.r, 
		          krb::g_context->defaultColor.g, 
		          krb::g_context->defaultColor.b, 
		          krb::g_context->defaultColor.a);
	glBegin(GL_LINES);
	for(size_t v = 0; v < vertexCount; v++)
	{
		const void* vertex = 
			reinterpret_cast<const u8*>(vertices) + (v*vertexStride);
		const f32* v3f32_position = reinterpret_cast<const f32*>(
			reinterpret_cast<const u8*>(vertex) + 
				vertexAttribOffsets.position_3f32);
		const f32* v4f32_color = reinterpret_cast<const f32*>(
			reinterpret_cast<const u8*>(vertex) + 
				vertexAttribOffsets.color_4f32);
		if(vertexAttribOffsets.color_4f32 < vertexStride)
			glColor4fv(v4f32_color);
		glVertex3fv(v3f32_position);
	}
	glEnd();
	GL_CHECK_ERROR();
}
internal KRB_DRAW_TRIS(krbDrawTris)
{
	kassert(vertexCount % 3 == 0);
	kassert(vertexAttribOffsets.position_3f32 < vertexStride);
	if(vertexAttribOffsets.texCoord_2f32 >= vertexStride)
		glDisable(GL_TEXTURE_2D);
	else
		glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(vertexAttribOffsets.color_4f32 >= vertexStride)
		glColor4f(krb::g_context->defaultColor.r, 
		          krb::g_context->defaultColor.g, 
		          krb::g_context->defaultColor.b, 
		          krb::g_context->defaultColor.a);
	glBegin(GL_TRIANGLES);
	for(size_t v = 0; v < vertexCount; v++)
	{
		const void* vertex = 
			reinterpret_cast<const u8*>(vertices) + (v*vertexStride);
		const f32* v3f32_position = reinterpret_cast<const f32*>(
			reinterpret_cast<const u8*>(vertex) + 
				vertexAttribOffsets.position_3f32);
		if(vertexAttribOffsets.color_4f32 < vertexStride)
		{
			const f32* v4f32_color = reinterpret_cast<const f32*>(
				reinterpret_cast<const u8*>(vertex) + 
					vertexAttribOffsets.color_4f32);
			glColor4fv(v4f32_color);
		}
		if(vertexAttribOffsets.texCoord_2f32 < vertexStride)
		{
			const f32* v2f32_texCoord = reinterpret_cast<const f32*>(
				reinterpret_cast<const u8*>(vertex) + 
					vertexAttribOffsets.texCoord_2f32);
			glTexCoord2fv(v2f32_texCoord);
		}
		glVertex3fv(v3f32_position);
	}
	glEnd();
	GL_CHECK_ERROR();
}
internal KRB_DRAW_QUAD_TEXTURED(krbDrawQuadTextured)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_TRIANGLES);
	const v2f32 quadMeshOffset = {-ratioAnchor.x*size.x, ratioAnchor.y*size.y};
	// draw the bottom-left triangle //
	// up-left vertex
	glTexCoord2f(texCoords[0].x, texCoords[0].y);
	glColor4f(colors[0].r, colors[0].g, colors[0].b, colors[0].a);
	glVertex2f(quadMeshOffset.x + 0, quadMeshOffset.y + 0);
	// down-left vertex
	glTexCoord2f(texCoords[1].x, texCoords[1].y); 
	glColor4f(colors[1].r, colors[1].g, colors[1].b, colors[1].a);
	glVertex2f(quadMeshOffset.x + 0, quadMeshOffset.y + -size.y);
	// down-right vertex
	glTexCoord2f(texCoords[2].x, texCoords[2].y); 
	glColor4f(colors[2].r, colors[2].g, colors[2].b, colors[2].a);
	glVertex2f(quadMeshOffset.x + size.x, quadMeshOffset.y + -size.y);
	// draw the upper-right triangle //
	// up-left vertex
	glTexCoord2f(texCoords[0].x, texCoords[0].y);
	glColor4f(colors[0].r, colors[0].g, colors[0].b, colors[0].a);
	glVertex2f(quadMeshOffset.x + 0, quadMeshOffset.y + 0);
	// down-right vertex
	glTexCoord2f(texCoords[2].x, texCoords[2].y); 
	glColor4f(colors[2].r, colors[2].g, colors[2].b, colors[2].a);
	glVertex2f(quadMeshOffset.x + size.x, quadMeshOffset.y + -size.y);
	// up-right vertex
	glTexCoord2f(texCoords[3].x, texCoords[3].y); 
	glColor4f(colors[3].r, colors[3].g, colors[3].b, colors[3].a);
	glVertex2f(quadMeshOffset.x + size.x, quadMeshOffset.y + 0);
	glEnd();
	GL_CHECK_ERROR();
}
internal KRB_DRAW_CIRCLE(krbDrawCircle)
{
	if(vertexCount < 3)
	{
		KLOG(WARNING, "Attempting to draw a degenerate circle!  Ignoring...");
		return;
	}
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
	glDisable(GL_TEXTURE_2D);
	const f32 deltaRadians = 2*PI32 / vertexCount;
	if(!kmath::isNearlyZero(colorFill.a))
	// Draw the fill region //
	{
		glColor4f(colorFill.r, colorFill.g, colorFill.b, colorFill.a);
		glBegin(GL_TRIANGLE_FAN);
		glVertex2f(0,0);
		for(u16 v = 0; v <= vertexCount; v++)
		{
			const f32 radians = v*deltaRadians;
			glVertex2f(radius*cosf(radians), radius*sinf(radians));
		}
		glEnd();
	}
	if(!kmath::isNearlyZero(colorOutline.a))
	// Draw the outline region //
	{
		glColor4f(colorOutline.r, colorOutline.g, colorOutline.b, 
		          colorOutline.a);
		if(kmath::isNearlyZero(outlineThickness))
		// Draw the outline as a LINE_LOOP primitive //
		{
			glBegin(GL_LINE_LOOP);
			for(u16 v = 0; v < vertexCount; v++)
			{
				const f32 radians = v*deltaRadians;
				glVertex2f(radius*cosf(radians), radius*sinf(radians));
			}
			glEnd();
		}
		else
		// Draw the outline as a TRIANGLE_STRIP primitive //
		{
			glBegin(GL_TRIANGLE_STRIP);
			for(u16 v = 0; v <= vertexCount; v++)
			{
				const f32 radians = v*deltaRadians;
				glVertex2f(radius*cosf(radians), radius*sinf(radians));
				glVertex2f((radius+outlineThickness)*cosf(radians), 
				           (radius+outlineThickness)*sinf(radians));
			}
			glEnd();
		}
	}
	GL_CHECK_ERROR();
}
internal KRB_VIEW_TRANSLATE(krbViewTranslate)
{
	glMatrixMode(GL_MODELVIEW);
	glTranslatef(offset.x, offset.y, 0.f);
	glPushMatrix();
	GL_CHECK_ERROR();
}
internal KRB_SET_MODEL_XFORM(krbSetModelXform)
{
	glMatrixMode(GL_MODELVIEW);
	GLint modelViewStackDepth;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
	if(modelViewStackDepth > 1)
	{
		glPopMatrix();
	}
	glPushMatrix();
	glTranslatef(translation.x, translation.y, translation.z);
	{
		const f32 orientationRadians = 2 * acosf(orientation.qw);
		const f32 axisDivisor = 
			kmath::isNearlyZero(1 - orientation.qw*orientation.qw)
				? 1
				: sqrtf(1 - orientation.qw*orientation.qw);
		const f32 orientationAxisX = orientation.qx / axisDivisor;
		const f32 orientationAxisY = orientation.qy / axisDivisor;
		const f32 orientationAxisZ = orientation.qz / axisDivisor;
		glRotatef(orientationRadians*180/PI32, 
		          orientationAxisX, orientationAxisY, orientationAxisZ);
	}
	glScalef(scale.x, scale.y, scale.z);
	GL_CHECK_ERROR();
}
internal KRB_SET_MODEL_XFORM_2D(krbSetModelXform2d)
{
	glMatrixMode(GL_MODELVIEW);
	GLint modelViewStackDepth;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
	if(modelViewStackDepth > 1)
	{
		glPopMatrix();
	}
	glPushMatrix();
	glTranslatef(translation.x, translation.y, 0.f);
	{
		const f32 orientationRadians = 2 * acosf(orientation.qw);
		const f32 axisDivisor = 
			kmath::isNearlyZero(1 - orientation.qw*orientation.qw)
				? 1
				: sqrtf(1 - orientation.qw*orientation.qw);
		const f32 orientationAxisX = orientation.qx / axisDivisor;
		const f32 orientationAxisY = orientation.qy / axisDivisor;
		const f32 orientationAxisZ = orientation.qz / axisDivisor;
		glRotatef(orientationRadians*180/PI32, 
		          orientationAxisX, orientationAxisY, orientationAxisZ);
	}
	glScalef(scale.x, scale.y, 1.f);
	GL_CHECK_ERROR();
}
internal KRB_SET_MODEL_MATRIX(krbSetModelMatrix)
{
	glMatrixMode(GL_MODELVIEW);
	GLint modelViewStackDepth;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
	if(modelViewStackDepth > 1)
	{
		glPopMatrix();
	}
	glPushMatrix();
	glMultTransposeMatrixf(rowMajorMatrix4x4);
	GL_CHECK_ERROR();
}
internal KRB_SET_MODEL_XFORM_BILLBOARD(krbSetModelXformBillboard)
{
	m4x4f32 modelView;
	glGetFloatv(GL_TRANSPOSE_MODELVIEW_MATRIX, modelView.elements);
	if(lockX)
	{
		modelView.r0c0 = 1;
		modelView.r1c0 = 0;
		modelView.r2c0 = 0;
	}
	if(lockY)
	{
		modelView.r0c1 = 0;
		modelView.r1c1 = 1;
		modelView.r2c1 = 0;
	}
	if(lockZ)
	{
		modelView.r0c2 = 0;
		modelView.r1c2 = 0;
		modelView.r2c2 = 1;
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadTransposeMatrixf(modelView.elements);
	GL_CHECK_ERROR();
}
internal KRB_LOAD_IMAGE(krbLoadImage)
{
	GLuint texName;
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageSizeX, imageSizeY, 0, 
	             GL_RGBA, GL_UNSIGNED_BYTE, imageDataRGBA);
	GL_CHECK_ERROR();
	return texName;
}
internal KRB_DELETE_TEXTURE(krbDeleteTexture)
{
	glDeleteTextures(1, &krbTextureHandle);
	GL_CHECK_ERROR();
}
internal KRB_USE_TEXTURE(krbUseTexture)
{
	glBindTexture(GL_TEXTURE_2D, kth);
	GL_CHECK_ERROR();
}
internal KRB_WORLD_TO_SCREEN(krbWorldToScreen)
{
	kassert(worldPositionDimension < 4);
	/* obtain VP matrix from opengl driver */
	glMatrixMode(GL_MODELVIEW);
	GLint modelViewStackDepth;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
	while(modelViewStackDepth > 1)
	{
		glPopMatrix();
		modelViewStackDepth--;
	}
	m4x4f32 mView;
	m4x4f32 mProjection;
	glGetFloatv(GL_TRANSPOSE_MODELVIEW_MATRIX, mView.elements);
	glMatrixMode(GL_PROJECTION);
	glGetFloatv(GL_TRANSPOSE_PROJECTION_MATRIX, mProjection.elements);
	GL_CHECK_ERROR();
	/* obtain viewport size & viewport offset from driver */
	GLint viewportValues[4];// [x,y, width,height]
	glGetIntegerv(GL_VIEWPORT, viewportValues);
	GL_CHECK_ERROR();
	/* calculate clip-space */
	v4f32 worldPoint = {};
	worldPoint.elements[3] = 1.f;
	for(u8 d = 0; d < worldPositionDimension; d++)
	{
		worldPoint.elements[d] = pWorldPosition[d];
	}
	const v4f32 cameraSpacePoint = mView * worldPoint;
	const v4f32 clipSpacePoint   = mProjection * cameraSpacePoint;
	if(kmath::isNearlyZero(clipSpacePoint.elements[3]))
	{
		local_persist const v2f32 INVALID_RESULT = {nanf(""), nanf("")};
		return INVALID_RESULT;
	}
	/* calculate normalized-device-coordinate-space 
		y is inverted here because screen-space y axis is flipped! */
	const v3f32 ndcSpacePoint = 
		{ clipSpacePoint.elements[0] / clipSpacePoint.elements[3]
		, clipSpacePoint.elements[1] / clipSpacePoint.elements[3] * -1
		, clipSpacePoint.elements[2] / clipSpacePoint.elements[3]
	};
	/* calculate screen-space.  glsl formula = 
	   ((ndcSpacePoint.xy + 1.0) / 2.0) * viewSize + viewOffset */
	const v2f32 result = 
		{ ((ndcSpacePoint.x + 1.f) / 2.f) * viewportValues[2] + 
			viewportValues[0]
		, ((ndcSpacePoint.y + 1.f) / 2.f) * viewportValues[3] + 
			viewportValues[1]
	};
	return result;
}
internal KRB_SCREEN_TO_WORLD(krbScreenToWorld)
{
	/* obtain VP matrix from opengl driver */
	glMatrixMode(GL_MODELVIEW);
	GLint modelViewStackDepth;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
	while(modelViewStackDepth > 1)
	{
		glPopMatrix();
		modelViewStackDepth--;
	}
	m4x4f32 mView;
	m4x4f32 mProjection;
	glGetFloatv(GL_TRANSPOSE_MODELVIEW_MATRIX, mView.elements);
	glMatrixMode(GL_PROJECTION);
	glGetFloatv(GL_TRANSPOSE_PROJECTION_MATRIX, mProjection.elements);
	GL_CHECK_ERROR();
	/* at this point, we have everything we need from the GL to calculate the 
		eye ray: */
	m4x4f32 mInverseView;
	m4x4f32 mInverseProjection;
	if(!m4x4f32::invert(mView.elements, mInverseView.elements))
	{
		KLOG(WARNING, "Failed to invert the view matrix!");
		return false;
	}
	if(!m4x4f32::invert(mProjection.elements, mInverseProjection.elements))
	{
		KLOG(WARNING, "Failed to invert the projection matrix!");
		return false;
	}
	const v2i32& v2i32WindowPos = 
		*reinterpret_cast<const v2i32*>(windowPosition);
	const v2f32 v2f32WindowPos = 
		{ static_cast<f32>(v2i32WindowPos.x)
		, static_cast<f32>(v2i32WindowPos.y) };
	const v2u32& v2u32WindowSize = 
		*reinterpret_cast<const v2u32*>(windowSize);
	/* We can determine if a projection matrix is orthographic or frustum based 
		on the last element of the W row.  See: 
		http://www.songho.ca/opengl/gl_projectionmatrix.html */
	kassert(mProjection.r3c3 == 1 || mProjection.r3c3 == 0);
	const bool isOrthographic = mProjection.r3c3 == 1;
	/* viewport-space          => normalized-device-space */
	const v2f32 eyeRayNds = 
		{  2*v2f32WindowPos.x / v2u32WindowSize.x - 1
		, -2*v2f32WindowPos.y / v2u32WindowSize.y + 1 };
	/* normalized-device-space => homogeneous-clip-space */
	/* arbitrarily set the eye ray direction vector as far to the "back" of the 
		homogeneous clip space box, which means setting the Z coordinate to a 
		value of -1, since coordinates are right-handed */
	const v4f32 eyeRayDirectionHcs = {eyeRayNds.x, eyeRayNds.y, -1, 1};
	/* homogeneous-clip-space  => eye-space */
	v4f32 eyeRayDirectionEs = mInverseProjection*eyeRayDirectionHcs;
	if(!isOrthographic)
		eyeRayDirectionEs.vw = 0;
	/* eye-space               => world-space */
	const v4f32 eyeRayDirectionWs = mInverseView*eyeRayDirectionEs;
	v3f32& resultPosition  = *reinterpret_cast<v3f32*>(o_worldEyeRayPosition);
	v3f32& resultDirection = *reinterpret_cast<v3f32*>(o_worldEyeRayDirection);
	if(isOrthographic)
	{
		/* the orthographic eye position should be as far to the "front" of the 
			homogenous clip space box, which means setting the Z coordinate to a 
			value of 1 */
		const v4f32 eyeRayPositionHcs = {eyeRayNds.x, eyeRayNds.y, 1, 1};
		const v4f32 eyeRayPositionEs  = mInverseProjection*eyeRayPositionHcs;
		const v4f32 eyeRayPositionWs  = mInverseView*eyeRayPositionEs;
		resultPosition  = 
			{eyeRayPositionWs.vx, eyeRayPositionWs.vy, eyeRayPositionWs.vz};
		resultDirection = 
			kmath::normal( v3f32{eyeRayDirectionWs.vx, 
			                     eyeRayDirectionWs.vy, 
			                     eyeRayDirectionWs.vz} - resultPosition );
	}
	else
	{
		/* camera's eye position can be easily derived from the inverse view 
			matrix: https://stackoverflow.com/a/39281556/4526664 */
		resultPosition = 
			{mInverseView.r0c3, mInverseView.r1c3, mInverseView.r2c3};
		resultDirection = 
			kmath::normal( v3f32{eyeRayDirectionWs.vx, 
			                     eyeRayDirectionWs.vy, 
			                     eyeRayDirectionWs.vz} );
	}
	return true;
}
internal KRB_SET_CURRENT_CONTEXT(krbSetCurrentContext)
{
	krb::g_context = context;
}
internal KRB_SET_DEFAULT_COLOR(krbSetDefaultColor)
{
	krb::g_context->defaultColor = color;
}