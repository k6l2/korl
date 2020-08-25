/** This code should all be a completely platform-independent implementation of 
 * the Kyle's Renderer Backend interface.  
*/
#include "krb-interface.h"
///TODO: maybe figure out a more platform-independent way of getting OpenGL 
///      function definitions.
#ifdef _WIN32
	#include "win32-main.h"
	#include <GL/GL.h>
#endif
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
	glClear(GL_COLOR_BUFFER_BIT);
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
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho)
{
	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, windowSizeX, windowSizeY);
	glOrtho(-static_cast<f32>(windowSizeX)/2, static_cast<f32>(windowSizeX)/2, 
	        -static_cast<f32>(windowSizeY)/2, static_cast<f32>(windowSizeY)/2, 
	        halfDepth, -halfDepth);
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
	glMatrixMode(GL_PROJECTION);
	glViewport(0, 0, windowSizeX, windowSizeY);
	glOrtho(-static_cast<f32>(viewportWidth)/2, 
	         static_cast<f32>(viewportWidth)/2, 
	        -static_cast<f32>(fixedHeight)/2, static_cast<f32>(fixedHeight)/2, 
	        halfDepth, -halfDepth);
	GL_CHECK_ERROR();
}
internal KRB_DRAW_LINES(krbDrawLines)
{
	kassert(vertexCount % 2 == 0);
	kassert(vertexAttribOffsets.position_3f32 < vertexStride);
	kassert(vertexAttribOffsets.texCoord_2f32 >= vertexStride);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);
	if(vertexAttribOffsets.color_4f32 >= vertexStride)
		glColor4b(0x7F, 0x7F, 0x7F, 0x7F);
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
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_TEXTURE_2D);
	if(vertexAttribOffsets.color_4f32 >= vertexStride)
		glColor4b(0x7F, 0x7F, 0x7F, 0x7F);
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
		const f32 orientationRadians = 2 * acosf(orientation.w);
		const f32 axisDivisor = 
			kmath::isNearlyZero(1 - orientation.w*orientation.w)
				? 1
				: sqrtf(1 - orientation.w*orientation.w);
		const f32 orientationAxisX = orientation.x / axisDivisor;
		const f32 orientationAxisY = orientation.y / axisDivisor;
		const f32 orientationAxisZ = orientation.z / axisDivisor;
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
		const f32 orientationRadians = 2 * acosf(orientation.w);
		const f32 axisDivisor = 
			kmath::isNearlyZero(1 - orientation.w*orientation.w)
				? 1
				: sqrtf(1 - orientation.w*orientation.w);
		const f32 orientationAxisX = orientation.x / axisDivisor;
		const f32 orientationAxisY = orientation.y / axisDivisor;
		const f32 orientationAxisZ = orientation.z / axisDivisor;
		glRotatef(orientationRadians*180/PI32, 
		          orientationAxisX, orientationAxisY, orientationAxisZ);
	}
	glScalef(scale.x, scale.y, 1.f);
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
	GLfloat gl_matrixView[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, gl_matrixView);
	glMatrixMode(GL_PROJECTION);
	GLfloat gl_matrixProjection[16];
	glGetFloatv(GL_PROJECTION_MATRIX, gl_matrixProjection);
	/* @optimization: apparently you can just pass GL_TRANSPOSE_MODELVIEW_MATRIX 
		to glGet* functions, so I don't need to transpose my matrices? */
	const m4x4f32 mView       = m4x4f32::transpose(gl_matrixView);
	const m4x4f32 mProjection = m4x4f32::transpose(gl_matrixProjection);
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
		{ .x = clipSpacePoint.elements[0] / clipSpacePoint.elements[3]
		, .y = clipSpacePoint.elements[1] / clipSpacePoint.elements[3] * -1
		, .z = clipSpacePoint.elements[2] / clipSpacePoint.elements[3]
	};
	/* calculate screen-space.  glsl formula = 
	   ((ndcSpacePoint.xy + 1.0) / 2.0) * viewSize + viewOffset */
	const v2f32 result = 
		{ .x = ((ndcSpacePoint.x + 1.f) / 2.f) * viewportValues[2] + 
		       viewportValues[0]
		, .y = ((ndcSpacePoint.y + 1.f) / 2.f) * viewportValues[3] + 
		       viewportValues[1]
	};
	return result;
}