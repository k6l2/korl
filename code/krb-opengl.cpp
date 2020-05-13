/** This code should all be a completely platform-independent implementation of 
 * the Kyle's Renderer Backend interface.  
*/
#include "krb-interface.h"
///TODO: maybe figure out a more platform-independent way of getting OpenGL 
///      function definitions.
#ifdef _WIN32
	#include <windows.h>
	#include <GL/GL.h>
#endif
#include "z85.h"
#include "stb/stb_image.h"
internal GLenum krbOglCheckErrors(const char* file, int line)
{
	GLenum errorCode;
	do
	{
		errorCode = glGetError();
	} while (errorCode != GL_NO_ERROR);
	if(errorCode != GL_NO_ERROR)
	{
		KLOG_ERROR("[%s|%i] OpenGL ERROR=%i", file, line, errorCode);
	}
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
		for(; modelViewStackDepth; modelViewStackDepth--)
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
	if(modelViewStackDepth > 0)
	{
		glPopMatrix();
		glPushMatrix();
	}
	glTranslatef(translation.x, translation.y, 0.f);
	GL_CHECK_ERROR();
}
internal KRB_LOAD_IMAGE_Z85(krbLoadImageZ85)
{
	const i32 imageFileDataSize = kmath::safeTruncateI32(
		z85::decodedFileSizeBytes(z85ImageNumBytes));
	if(!z85::decode(reinterpret_cast<const i8*>(z85ImageData), 
	                tempImageDataBuffer))
	{
		KLOG_ERROR("z85::decode failure!");
		return 0;
	}
	int imgW, imgH, imgNumByteChannels;
	u8*const img = 
		stbi_load_from_memory(reinterpret_cast<u8*>(tempImageDataBuffer), 
		                      imageFileDataSize, 
		                      &imgW, &imgH, &imgNumByteChannels, 4);
	kassert(img);
	if(!img)
	{
		KLOG_ERROR("stbi_load_from_memory failure!");
		return 0;
	}
	defer(stbi_image_free(img));
	GLuint texName;
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imgW, imgH, 0, 
	             GL_RGBA, GL_UNSIGNED_BYTE, img);
	GL_CHECK_ERROR();
	return texName;
}
internal KRB_USE_TEXTURE(krbUseTexture)
{
	glBindTexture(GL_TEXTURE_2D, kth);
	GL_CHECK_ERROR();
}