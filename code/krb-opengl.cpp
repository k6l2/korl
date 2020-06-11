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
			KLOG(ERROR, "[%s|%i] OpenGL ERROR=%i", file, line, errorCode);
		}
	} while (errorCode != GL_NO_ERROR);
	return errorCode;
}
#define GL_CHECK_ERROR() krbOglCheckErrors(__FILENAME__, __LINE__)
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
	glViewport(0, 0, 
	           static_cast<GLsizei>(windowSizeX),
	           static_cast<GLsizei>(windowSizeY));
	glOrtho(-windowSizeX/2, windowSizeX/2, -windowSizeY/2, windowSizeY/2, 
	        halfDepth, -halfDepth);
	GL_CHECK_ERROR();
}
internal KRB_DRAW_LINE(krbDrawLine)
{
	glDisable(GL_TEXTURE_2D);
	glColor4f(color.r, color.g, color.b, color.a);
	glBegin(GL_LINES);
	glVertex2f(p0.x, p0.y);
	glVertex2f(p1.x, p1.y);
	glEnd();
	GL_CHECK_ERROR();
}
internal KRB_DRAW_TRI(krbDrawTri)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
	glDisable(GL_TEXTURE_2D);
	glColor4b(0x7F, 0x7F, 0x7F, 0x7F);
	glBegin(GL_TRIANGLES);
	glVertex2f(p0.x, p0.y);
	glVertex2f(p1.x, p1.y);
	glVertex2f(p2.x, p2.y);
	glEnd();
	GL_CHECK_ERROR();
}
internal KRB_DRAW_TRI_TEXTURED(krbDrawTriTextured)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
	glEnable(GL_TEXTURE_2D);
	glColor4b(0x7F, 0x7F, 0x7F, 0x7F);
	glBegin(GL_TRIANGLES);
	glTexCoord2f(t0.x, t0.y); glVertex2f(p0.x, p0.y);
	glTexCoord2f(t1.x, t1.y); glVertex2f(p1.x, p1.y);
	glTexCoord2f(t2.x, t2.y); glVertex2f(p2.x, p2.y);
	glEnd();
	GL_CHECK_ERROR();
}
internal KRB_DRAW_QUAD_TEXTURED(krbDrawQuadTextured)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
	glEnable(GL_TEXTURE_2D);
	glColor4b(0x7F, 0x7F, 0x7F, 0x7F);
	glBegin(GL_TRIANGLES);
	const v2f32 quadMeshOffset = {-ratioAnchor.x*size.x, ratioAnchor.y*size.y};
	// draw the bottom-left triangle //
	glTexCoord2f(texCoordUL.x, texCoordUL.y); 
	glVertex2f(quadMeshOffset.x + 0, quadMeshOffset.y + 0);
	glTexCoord2f(texCoordDL.x, texCoordDL.y); 
	glVertex2f(quadMeshOffset.x + 0, quadMeshOffset.y + -size.y);
	glTexCoord2f(texCoordDR.x, texCoordDR.y); 
	glVertex2f(quadMeshOffset.x + size.x, quadMeshOffset.y + -size.y);
	// draw the upper-right triangle //
	glTexCoord2f(texCoordUL.x, texCoordUL.y); 
	glVertex2f(quadMeshOffset.x + 0, quadMeshOffset.y + 0);
	glTexCoord2f(texCoordDR.x, texCoordDR.y); 
	glVertex2f(quadMeshOffset.x + size.x, quadMeshOffset.y + -size.y);
	glTexCoord2f(texCoordUR.x, texCoordUR.y); 
	glVertex2f(quadMeshOffset.x + size.x, quadMeshOffset.y + 0);
	glEnd();
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
		glPushMatrix();
	}
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
		glPushMatrix();
	}
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