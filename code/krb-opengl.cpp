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
internal KRB_BEGIN_FRAME(krbBeginFrame)
{
	glClearColor(clamped0_1_red, clamped0_1_green, clamped0_1_blue, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	///TODO: check GL errors
}
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho)
{
	glOrtho(-windowSizeX/2, windowSizeX/2, -windowSizeY/2, windowSizeY/2, 
	        halfDepth, -halfDepth);
	///TODO: check GL errors
}
internal KRB_DRAW_LINE(krbDrawLine)
{
	glDisable(GL_TEXTURE_2D);
	glColor4f(color.r, color.g, color.b, color.a);
	glBegin(GL_LINES);
	glVertex2f(p0.x, p0.y);
	glVertex2f(p1.x, p1.y);
	glEnd();
	///TODO: check GL errors
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
	///TODO: check GL errors
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
	///TODO: check GL errors
}
internal KRB_VIEW_TRANSLATE(krbViewTranslate)
{
	glMatrixMode(GL_MODELVIEW);
	glTranslatef(offset.x, offset.y, 0.f);
	///TODO: check GL errors
}
internal KRB_LOAD_IMAGE_Z85(krbLoadImageZ85)
{
	const i32 imageFileDataSize = kmath::safeTruncateI32(
		z85::decodedFileSizeBytes(z85ImageNumBytes - 1));
	if(!z85::decode(reinterpret_cast<const i8*>(z85ImageData), 
	                tempImageDataBuffer))
	{
		kassert(!"z85::decode failure!");
		///TODO: handle error
	}
	int imgW, imgH, imgNumByteChannels;
	u8*const img = 
		stbi_load_from_memory(reinterpret_cast<u8*>(tempImageDataBuffer), 
		                      imageFileDataSize, 
		                      &imgW, &imgH, &imgNumByteChannels, 4);
	kassert(img);
	if(!img)
	{
		///TODO: handle error
	}
	GLuint texName;
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imgW, imgH, 0, 
	             GL_RGBA, GL_UNSIGNED_BYTE, img);
	///TODO: check GL errors
	stbi_image_free(img);
	return texName;
}
internal KRB_USE_TEXTURE(krbUseTexture)
{
	glBindTexture(GL_TEXTURE_2D, kth);
	///TODO: check GL errors
}