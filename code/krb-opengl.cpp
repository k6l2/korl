/** This code should all be a completely platform-independent implementation of 
 * the Kyle's Renderer Backend interface.  
*/
#include "krb-interface.h"
/* @TODO: maybe figure out a more platform-independent way of getting OpenGL
	function definitions. */
#ifdef _WIN32
	#include "win32-main.h"
	#include <GL/GL.h>
#endif
#include "krb-opengl-extensions.h"
bool 
	KrbVertexAttributeOffsets::operator==(
		const KrbVertexAttributeOffsets& other) const
{
	return position_3f32 == other.position_3f32 
		&& color_4f32    == other.color_4f32 
		&& texCoord_2f32 == other.texCoord_2f32;
}
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
#define KRB_COMPILE_SHADER(shader, src, logBuffer) \
	glShaderSource(shader, 1, &src, nullptr);\
	glCompileShader(shader);\
	{\
		GLint successCompile = GL_FALSE;\
		glGetShaderiv(shader, GL_COMPILE_STATUS, &successCompile);\
		if(successCompile != GL_TRUE)\
		{\
			glGetShaderInfoLog(\
				shader, CARRAY_SIZE(logBuffer), nullptr, logBuffer);\
			KLOG(ERROR, "shader compile failure! '%s'", logBuffer);\
		}\
	}
#define KRB_LINK_PROGRAM(program, logBuffer) \
	glLinkProgram(program);\
	{\
		GLint successLink = GL_FALSE;\
		glGetProgramiv(program, GL_LINK_STATUS, &successLink);\
		if(successLink != GL_TRUE)\
		{\
			glGetProgramInfoLog(\
				program, CARRAY_SIZE(logBuffer), nullptr, logBuffer);\
			KLOG(ERROR, "link program failed! '%s'", logBuffer);\
		}\
	}
/** This must be called whenever: 
 *   - the internal immediate mode VBO gets expanded to hold more data, since we 
 *     will create a new VBO with a larger size and copy the data from the old 
 *     VBO to it!
 *   - the structure of the vertex data being sent to the render backend 
 *     changes */
internal void korl_rb_ogl_resetImmediateModeVertexAttributes()
{
	/* bind to the internal immediate-mode VAO & VBO used by it */
	glBindVertexArray(krb::g_context->vaoImmediate);
	glBindBuffer(GL_ARRAY_BUFFER, krb::g_context->vboImmediate);
	/* allow general vertex layouts/strides */
	// position is REQUIRED to be a valid attribute within vertex data! //
	korlAssert(
		krb::g_context->immediateVertexAttributeOffsets.position_3f32 < 
		krb::g_context->immediateVertexStride);
	/* @robustness: ensure that all present vertex attributes do not overlap 
		each other, or go out-of-bounds of the vertex stride, as these 
		situations would prove to be VERY annoying to debug... */
	glVertexAttribPointer(
		// 0 == hard-coded internal position attribute location
		0, 3, GL_FLOAT, GL_FALSE, krb::g_context->immediateVertexStride, 
		// dumb looking cast due to bad OGL API & dumb 64-bit conversion
		reinterpret_cast<void*>(static_cast<size_t>(
			krb::g_context->immediateVertexAttributeOffsets.position_3f32)));
	// optional color vertex attribute //
	if(krb::g_context->immediateVertexAttributeOffsets.color_4f32 < 
			krb::g_context->immediateVertexStride)
	{
		glVertexAttribPointer(
			// 1 == hard-coded internal color attribute location
			1, 4, GL_FLOAT, GL_FALSE, krb::g_context->immediateVertexStride, 
			// dumb looking cast due to bad OGL API & dumb 64-bit conversion
			reinterpret_cast<void*>(static_cast<size_t>(
				krb::g_context->immediateVertexAttributeOffsets.color_4f32)));
		glEnableVertexAttribArray(1);// 1 == attribute_color
	}
	else
		glDisableVertexAttribArray(1);// 1 == attribute_color
	// optional texture normal attribute //
	if(krb::g_context->immediateVertexAttributeOffsets.texCoord_2f32 < 
			krb::g_context->immediateVertexStride)
	{
		glVertexAttribPointer(
			// 2 == hard-coded internal texture normal attribute location
			2, 2, GL_FLOAT, GL_FALSE, krb::g_context->immediateVertexStride, 
			// dumb looking cast due to bad OGL API & dumb 64-bit conversion
			reinterpret_cast<void*>(static_cast<size_t>(
				krb::g_context->immediateVertexAttributeOffsets
					.texCoord_2f32)));
		glEnableVertexAttribArray(2);// 2 == attribute_texNormal
	}
	else
	{
		/* un-bind tex2DUnit 0 since we're getting ready to draw something that 
			doesn't even use a texCoord vertex attribute */
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisableVertexAttribArray(2);// 2 == attribute_texNormal
	}
	/* unbind our internal immediate-mode VAO/VBO; we're done! */
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	GL_CHECK_ERROR();
}
internal void korl_rb_ogl_reserveImmediateModeVboBytes(u32 additionalBytes)
{
	const u32 vboImmediateUsedBytes = 
		krb::g_context->vboImmediateVertexCount * 
		krb::g_context->immediateVertexStride;
	const u32 vboImmediateRemainingBytes = 
		krb::g_context->vboImmediateCapacity - vboImmediateUsedBytes;
	if(additionalBytes > vboImmediateRemainingBytes)
	{
		/* if not, allocate the next highest power of 2 */
		const u32 bytesRequired = vboImmediateUsedBytes + additionalBytes;
		u32 newCap = krb::g_context->vboImmediateCapacity;
		while(additionalBytes > newCap - vboImmediateUsedBytes)
		{
			/* gradually double capacity while making sure that we don't 
				overflow */
			const u32 oldCap = newCap;
			newCap *= 2;
			korlAssert(newCap > oldCap);
		}
		/* create a new buffer with newCap size bytes */
		GLuint vboNew;
		glGenBuffers(1, &vboNew);
		glBindBuffer(GL_ARRAY_BUFFER, vboNew);
		glBufferData(GL_ARRAY_BUFFER, newCap, nullptr, GL_DYNAMIC_DRAW);
		/* copy the old buffer data to the new buffer */
		glBindBuffer(GL_COPY_READ_BUFFER, krb::g_context->vboImmediate);
		glCopyBufferSubData(
			GL_COPY_READ_BUFFER, GL_ARRAY_BUFFER, 0, 0, 
			vboImmediateUsedBytes);
		glBindBuffer(GL_ARRAY_BUFFER    , 0);
		glBindBuffer(GL_COPY_READ_BUFFER, 0);
		/* delete the old buffer, replace reference with the new buffer */
		glDeleteBuffers(1, &krb::g_context->vboImmediate);
		krb::g_context->vboImmediate         = vboNew;
		krb::g_context->vboImmediateCapacity = newCap;
		/* update the immediate-mode VAO to use the new buffer */
		korl_rb_ogl_resetImmediateModeVertexAttributes();
	}
	GL_CHECK_ERROR();
}
internal void korl_rb_ogl_lazyInitializeContext()
{
	if(krb::g_context->initialized)
		return;
	/* prepare immediate-mode draw API shaders */
	GLchar bufferShaderInfoLog[512];
	const GLchar*const sourceShaderImmediateVertex = 
		"#version 330 core\n"
		"uniform vec4 uniform_defaultColor;\n"
		"layout(location = 0) in vec3 attribute_position;\n"
		"out vec4 vertexColor;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = vec4(attribute_position, 1.0);\n"
		"	vertexColor = uniform_defaultColor;\n"
		"}\0";
	const GLchar*const sourceShaderImmediateVertexColor = 
		"#version 330 core\n"
		"layout(location = 0) in vec3 attribute_position;\n"
		"layout(location = 1) in vec4 attribute_color;\n"
		"out vec4 vertexColor;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = vec4(attribute_position, 1.0);\n"
		"	vertexColor = attribute_color;\n"
		"}\0";
	const GLchar*const sourceShaderImmediateVertexColorTexNormal = 
		"#version 330 core\n"
		"layout(location = 0) in vec3 attribute_position;\n"
		"layout(location = 1) in vec4 attribute_color;\n"
		"layout(location = 2) in vec2 attribute_texNormal;\n"
		"out vec4 vertexColor;\n"
		"out vec2 vertexTexNormal;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = vec4(attribute_position, 1.0);\n"
		"	vertexColor = attribute_color;\n"
		"	vertexTexNormal = attribute_texNormal;\n"
		"}\0";
	const GLchar*const sourceShaderImmediateFragment = 
		"#version 330 core\n"
		"in vec4 vertexColor;\n"
		"out vec4 fragmentColor;\n"
		"void main()\n"
		"{\n"
		"	fragmentColor = vertexColor;\n"
		"}\0";
	const GLchar*const sourceShaderImmediateFragmentTexture = 
		"#version 330 core\n"
		"in vec4 vertexColor;\n"
		"in vec2 vertexTexNormal;\n"
		"out vec4 fragmentColor;\n"
		"uniform sampler2D textureUnit0;\n"
		"void main()\n"
		"{\n"
		"	fragmentColor = \n"
		"		vertexColor * texture(textureUnit0, vertexTexNormal);\n"
		"}\0";
	/* compile internal immediate-mode shaders */
	krb::g_context->shaderImmediateVertex = glCreateShader(GL_VERTEX_SHADER);
	krb::g_context->shaderImmediateVertexColor = 
		glCreateShader(GL_VERTEX_SHADER);
	krb::g_context->shaderImmediateVertexColorTexNormal = 
		glCreateShader(GL_VERTEX_SHADER);
	krb::g_context->shaderImmediateFragment = 
		glCreateShader(GL_FRAGMENT_SHADER);
	krb::g_context->shaderImmediateFragmentTexture = 
		glCreateShader(GL_FRAGMENT_SHADER);
	KRB_COMPILE_SHADER(
		krb::g_context->shaderImmediateVertex, 
		sourceShaderImmediateVertex, bufferShaderInfoLog);
	KRB_COMPILE_SHADER(
		krb::g_context->shaderImmediateVertexColor, 
		sourceShaderImmediateVertexColor, bufferShaderInfoLog);
	KRB_COMPILE_SHADER(
		krb::g_context->shaderImmediateVertexColorTexNormal, 
		sourceShaderImmediateVertexColorTexNormal, bufferShaderInfoLog);
	KRB_COMPILE_SHADER(
		krb::g_context->shaderImmediateFragment, 
		sourceShaderImmediateFragment, bufferShaderInfoLog);
	KRB_COMPILE_SHADER(
		krb::g_context->shaderImmediateFragmentTexture, 
		sourceShaderImmediateFragmentTexture, bufferShaderInfoLog);
	/* link internal immediate-mode programs */
	krb::g_context->programImmediate = glCreateProgram();
	glAttachShader(
		krb::g_context->programImmediate, 
		krb::g_context->shaderImmediateVertex);
	glAttachShader(
		krb::g_context->programImmediate, 
		krb::g_context->shaderImmediateFragment);
	KRB_LINK_PROGRAM(krb::g_context->programImmediate, bufferShaderInfoLog);
	krb::g_context->programImmediateColor = glCreateProgram();
	glAttachShader(
		krb::g_context->programImmediateColor, 
		krb::g_context->shaderImmediateVertexColor);
	glAttachShader(
		krb::g_context->programImmediateColor, 
		krb::g_context->shaderImmediateFragment);
	KRB_LINK_PROGRAM(
		krb::g_context->programImmediateColor, bufferShaderInfoLog);
	krb::g_context->programImmediateColorTexture = glCreateProgram();
	glAttachShader(
		krb::g_context->programImmediateColorTexture, 
		krb::g_context->shaderImmediateVertexColorTexNormal);
	glAttachShader(
		krb::g_context->programImmediateColorTexture, 
		krb::g_context->shaderImmediateFragmentTexture);
	KRB_LINK_PROGRAM(
		krb::g_context->programImmediateColorTexture, bufferShaderInfoLog);
	// explicitly tell OpenGL that the texture sampler(s) should use unit 0 //
	{
		const GLint uniformLocTexSampler = 
			glGetUniformLocation(
				krb::g_context->programImmediateColorTexture, "textureUnit0");
		korlAssert(uniformLocTexSampler > -1);
		glUseProgram(krb::g_context->programImmediateColorTexture);
		glUniform1i(uniformLocTexSampler, 0);
		glUseProgram(0);
	}
	/* prepare immediate-mode draw API VBOs */
	glGenBuffers(1, &krb::g_context->vboImmediate);
	glBindBuffer(GL_ARRAY_BUFFER, krb::g_context->vboImmediate);
	krb::g_context->vboImmediateCapacity = 
#if 1 /* testing dynamic VBO resizing */
		32;
#else /* set the VBO capacity to something more sensible to minimize reallocs */
		kmath::safeTruncateU32(kmath::kilobytes(16));
#endif
	glBufferData(
		GL_ARRAY_BUFFER, krb::g_context->vboImmediateCapacity, 
		nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	krb::g_context->vboImmediateVertexCount = 0;
	/* prepare immediate-mode VAO */
	glGenVertexArrays(1, &krb::g_context->vaoImmediate);
	glBindVertexArray(krb::g_context->vaoImmediate);
	glEnableVertexAttribArray(0);// 0 == attribute_position; always on!
	glBindVertexArray(0);
	/* I use right-handed homogeneous clip space, so depth buffer values 
		farthest away from the camera are -1, instead of the default OpenGL 
		value of 1 */
	glClearDepth(-1);
	/* default OpenGL uses a left-handed homogeneous clip space, whereas I 
		use right-handed, so I have to invert the depth function from the 
		default of GL_LESS */
	glDepthFunc(GL_GREATER);
	GL_CHECK_ERROR();
#if 0/* we don't actually need to update the VAO here, since this will happen 
		when we attempt to add vertex data for the first time anyway */
	korl_rb_ogl_resetImmediateModeVertexAttributes();
#endif//0
	krb::g_context->initialized = true;
}
internal void korl_rb_ogl_flushImmediateBuffer()
{
	/* flush all buffers which have yet to be drawn */
	if(krb::g_context->vboImmediateVertexCount)
	{
		/* @todo: make blend function programmable render state */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		/* we need to choose the appropriate program to use based on the vertex 
			attribute layout & other render states for immediate-mode */
		GLuint program = krb::g_context->programImmediate;
		if(krb::g_context->immediateVertexAttributeOffsets.color_4f32 < 
				krb::g_context->immediateVertexStride 
			&& krb::g_context->immediateVertexAttributeOffsets.texCoord_2f32 < 
				krb::g_context->immediateVertexStride)
			program = krb::g_context->programImmediateColorTexture;
		else if(krb::g_context->immediateVertexAttributeOffsets.color_4f32 < 
				krb::g_context->immediateVertexStride 
			&& krb::g_context->immediateVertexAttributeOffsets.texCoord_2f32 >= 
				krb::g_context->immediateVertexStride)
			program = krb::g_context->programImmediateColor;
		else if(krb::g_context->immediateVertexAttributeOffsets.color_4f32 >= 
				krb::g_context->immediateVertexStride 
			&& krb::g_context->immediateVertexAttributeOffsets.texCoord_2f32 < 
				krb::g_context->immediateVertexStride)
			korlAssert(!"texNormal without color attrib not yet implemented!");
		else {/* only position data in vertices, just use default program */}
		/* if we're using a 'texCoord', ensure that texture unit 0 contains a 2D 
			texture */
		if(krb::g_context->immediateVertexAttributeOffsets.texCoord_2f32 < 
			krb::g_context->immediateVertexStride)
		{
			GLint nameTex2dUnit0;
			glActiveTexture(GL_TEXTURE0);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &nameTex2dUnit0);
			korlAssert(nameTex2dUnit0);
		}
#if 0/* we can't actually do this because this flush occurs AFTER we tell the 
		KORL RB to begin using a texture */
		else
		/* likewise, if we're drawing without a 'texCoord', then make sure we 
			haven't bound anything there to help prevent draw errors */
		{
			GLint nameTex2dUnit0;
			glActiveTexture(GL_TEXTURE0);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &nameTex2dUnit0);
			korlAssert(!nameTex2dUnit0);
		}
#endif//0
		/* actually draw the immediate-mode buffer w/ correct render states */
		glUseProgram(program);
		/* if we aren't using a 'color' vertex attribute, we must be using a 
			program which takes a default color */
		/* @speed: instead of setting this each time we draw, we could 
			potentially only set this uniform for all relevant programs ONLY 
			when the default color is changed */
		if(krb::g_context->immediateVertexAttributeOffsets.color_4f32 >= 
				krb::g_context->immediateVertexStride)
		{
			/* @speed: instead of searching for the uniform location every time, 
				we could just locate it once when the program is linked & then 
				just use the cached value(s) here depending on the value of 
				`program` */
			const GLint uniformLocDefaultColor = 
				glGetUniformLocation(program, "uniform_defaultColor");
			korlAssert(uniformLocDefaultColor > -1);
			glUniform4fv(
				uniformLocDefaultColor, 1, 
				krb::g_context->defaultColor.elements);
		}
		glBindVertexArray(krb::g_context->vaoImmediate);
		glDrawArrays(
			krb::g_context->immediatePrimitiveType, 0, 
			krb::g_context->vboImmediateVertexCount);
		glBindVertexArray(0);
		glUseProgram(0);
		krb::g_context->vboImmediateVertexCount = 0;
	}
	GL_CHECK_ERROR();
}
internal KRB_BEGIN_FRAME(krbBeginFrame)
{
	korlAssert(!krb::g_context->frameInProgress);
	korl_rb_ogl_lazyInitializeContext();
	/* Disable the scissor test BEFORE clearing the color buffer so we can 
		guarantee the ENTIRE window gets cleared! */
	glDisable(GL_SCISSOR_TEST);
	glClearColor(
		clamped_0_1_colorRgb[0], clamped_0_1_colorRgb[1], 
		clamped_0_1_colorRgb[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glViewport(0, 0, windowSize[0], windowSize[1]);
	GL_CHECK_ERROR();
	/* clear the krb context */
	krb::g_context->windowSizeX = windowSize[0];
	krb::g_context->windowSizeY = windowSize[1];
	krb::g_context->defaultColor = krb::WHITE;
	krb::g_context->frameInProgress = true;
}
internal KRB_END_FRAME(krbEndFrame)
{
	korlAssert(krb::g_context->frameInProgress);
	korl_rb_ogl_flushImmediateBuffer();
	krb::g_context->frameInProgress = false;
}
internal KRB_SET_DEPTH_TESTING(krbSetDepthTesting)
{
	if(enable)
		glEnable(GL_DEPTH_TEST);
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
	glViewport(0, 0, krb::g_context->windowSizeX, krb::g_context->windowSizeY);
	glDisable(GL_SCISSOR_TEST);
	const f32 left  = -static_cast<f32>(krb::g_context->windowSizeX)/2;
	const f32 right =  static_cast<f32>(krb::g_context->windowSizeX)/2;
	const f32 bottom = -static_cast<f32>(krb::g_context->windowSizeY)/2;
	const f32 top    =  static_cast<f32>(krb::g_context->windowSizeY)/2;
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
	// empty the modelview stack //
	glMatrixMode(GL_MODELVIEW);
	{
		GLint modelViewStackDepth;
		glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
		for(; modelViewStackDepth > 1; modelViewStackDepth--)
		{
			glPopMatrix();
		}
	}
	glLoadIdentity();
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(krbSetProjectionOrthoFixedHeight)
{
	/*
		w / fixedHeight == windowAspectRatio
	*/
	const f32 windowAspectRatio = krb::g_context->windowSizeY == 0
		? 1.f 
		: static_cast<f32>(krb::g_context->windowSizeX) / 
			krb::g_context->windowSizeY;
	const GLsizei viewportWidth = 
		static_cast<GLsizei>(windowAspectRatio * fixedHeight);
	const f32 left  = -static_cast<f32>(viewportWidth)/2;
	const f32 right =  static_cast<f32>(viewportWidth)/2;
	const f32 bottom = -static_cast<f32>(fixedHeight)/2;
	const f32 top    =  static_cast<f32>(fixedHeight)/2;
	const f32 zNear =  halfDepth;
	const f32 zFar  = -halfDepth;
	glViewport(0, 0, krb::g_context->windowSizeX, krb::g_context->windowSizeY);
	glDisable(GL_SCISSOR_TEST);
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
	// empty the modelview stack //
	glMatrixMode(GL_MODELVIEW);
	{
		GLint modelViewStackDepth;
		glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
		for(; modelViewStackDepth > 1; modelViewStackDepth--)
		{
			glPopMatrix();
		}
	}
	glLoadIdentity();
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_FOV(krbSetProjectionFov)
{
	glViewport(0, 0, krb::g_context->windowSizeX, krb::g_context->windowSizeY);
	glDisable(GL_SCISSOR_TEST);
	const f32 aspectRatio = 
		static_cast<f32>(krb::g_context->windowSizeX) / 
		krb::g_context->windowSizeY;
	korlAssert(!kmath::isNearlyZero(aspectRatio));
	korlAssert(!kmath::isNearlyEqual(clipNear, clipFar) && clipFar > clipNear);
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
	// empty the modelview stack //
	glMatrixMode(GL_MODELVIEW);
	{
		GLint modelViewStackDepth;
		glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &modelViewStackDepth);
		for(; modelViewStackDepth > 1; modelViewStackDepth--)
		{
			glPopMatrix();
		}
	}
	glLoadIdentity();
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
internal KRB_DRAW_POINTS(krbDrawPoints)
{
	korlAssert(vertexAttribOffsets.position_3f32 < vertexStride);
	korlAssert(vertexAttribOffsets.texCoord_2f32 >= vertexStride);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if(vertexAttribOffsets.color_4f32 >= vertexStride)
		glColor4f(krb::g_context->defaultColor.r, 
		          krb::g_context->defaultColor.g, 
		          krb::g_context->defaultColor.b, 
		          krb::g_context->defaultColor.a);
	glBegin(GL_POINTS);
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
internal KRB_DRAW_LINES(krbDrawLines)
{
	korlAssert(vertexCount % 2 == 0);
	korlAssert(vertexAttribOffsets.position_3f32 < vertexStride);
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
	korlAssert(vertexCount % 3 == 0);
	korlAssert(vertexAttribOffsets.position_3f32 < vertexStride);
	if(    krb::g_context->immediatePrimitiveType != GL_TRIANGLES 
		|| krb::g_context->immediateVertexStride != vertexStride 
		|| krb::g_context->immediateVertexAttributeOffsets != 
			vertexAttribOffsets)
	{
		korl_rb_ogl_flushImmediateBuffer();
		krb::g_context->immediatePrimitiveType = GL_TRIANGLES;
		krb::g_context->immediateVertexStride = vertexStride;
		krb::g_context->immediateVertexAttributeOffsets = vertexAttribOffsets;
		korl_rb_ogl_resetImmediateModeVertexAttributes();
	}
	/* check to see if the vertex VBO has enough bytes for vertices; ensure the 
		vertex VBO has enough capacity for the vertices! */
	korl_rb_ogl_reserveImmediateModeVboBytes(vertexCount*vertexStride);
	/* append vertices to the end of the VBO */
	glBindBuffer(GL_ARRAY_BUFFER, krb::g_context->vboImmediate);
	glBufferSubData(
		GL_ARRAY_BUFFER, krb::g_context->vboImmediateVertexCount*vertexStride, 
		vertexCount*vertexStride, vertices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	krb::g_context->vboImmediateVertexCount += vertexCount;
	GL_CHECK_ERROR();
}
internal KRB_DRAW_QUAD(krbDrawQuad)
{
	struct QuadVertex
	{
		v3f32 position;
		ColorRgbaF32 color;
	} quadVertices[6];
	local_const u32 VERTEX_STRIDE = sizeof(QuadVertex);
	local_const KrbVertexAttributeOffsets VERTEX_ATTRIB_OFFSETS = 
		{ .position_3f32 = offsetof(QuadVertex, position)
		, .color_4f32    = offsetof(QuadVertex, color)
		, .texCoord_2f32 = sizeof(QuadVertex) };
	local_const u32 VERTEX_COUNT = CARRAY_SIZE(quadVertices);
	if(    krb::g_context->immediatePrimitiveType != GL_TRIANGLES 
		|| krb::g_context->immediateVertexStride != VERTEX_STRIDE 
		|| krb::g_context->immediateVertexAttributeOffsets != 
			VERTEX_ATTRIB_OFFSETS)
	{
		korl_rb_ogl_flushImmediateBuffer();
		krb::g_context->immediatePrimitiveType = GL_TRIANGLES;
		krb::g_context->immediateVertexStride = VERTEX_STRIDE;
		krb::g_context->immediateVertexAttributeOffsets = VERTEX_ATTRIB_OFFSETS;
		korl_rb_ogl_resetImmediateModeVertexAttributes();
	}
	/* check to see if the vertex VBO has enough bytes for vertices; ensure the 
		vertex VBO has enough capacity for the vertices! */
	korl_rb_ogl_reserveImmediateModeVboBytes(VERTEX_COUNT*VERTEX_STRIDE);
	/* build the quad vertices */
	const v2f32 quadMeshOffset = 
		{-ratioAnchor[0]*size[0], ratioAnchor[1]*size[1]};
	// bottom-left triangle //
	// top-left vertex //
	quadVertices[0].position = {quadMeshOffset.x, quadMeshOffset.y, 0};
	quadVertices[0].color = colors[0];
	// bottom-left vertex //
	quadVertices[1].position = 
		{quadMeshOffset.x, quadMeshOffset.y - size[1], 0};
	quadVertices[1].color = colors[1];
	// bottom-right vertex //
	quadVertices[2].position = 
		{quadMeshOffset.x + size[0], quadMeshOffset.y - size[1], 0};
	quadVertices[2].color = colors[2];
	// top-right triangle //
	// top-left vertex //
	quadVertices[3].position = {quadMeshOffset.x, quadMeshOffset.y, 0};
	quadVertices[3].color = colors[0];
	// bottom-right vertex //
	quadVertices[4].position = 
		{quadMeshOffset.x + size[0], quadMeshOffset.y - size[1], 0};
	quadVertices[4].color = colors[2];
	// top-right vertex //
	quadVertices[5].position = 
		{quadMeshOffset.x + size[0], quadMeshOffset.y, 0};
	quadVertices[5].color = colors[3];
	/* append vertices to the end of the VBO */
	glBindBuffer(GL_ARRAY_BUFFER, krb::g_context->vboImmediate);
	glBufferSubData(
		GL_ARRAY_BUFFER, krb::g_context->vboImmediateVertexCount*VERTEX_STRIDE, 
		VERTEX_COUNT*VERTEX_STRIDE, quadVertices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	krb::g_context->vboImmediateVertexCount += VERTEX_COUNT;
	GL_CHECK_ERROR();
}
internal KRB_DRAW_QUAD_TEXTURED(krbDrawQuadTextured)
{
	struct QuadVertex
	{
		v3f32 position;
		ColorRgbaF32 color;
		v2f32 textureNormal;
	} quadVertices[6];
	local_const u32 VERTEX_STRIDE = sizeof(QuadVertex);
	local_const KrbVertexAttributeOffsets VERTEX_ATTRIB_OFFSETS = 
		{ .position_3f32 = offsetof(QuadVertex, position)
		, .color_4f32    = offsetof(QuadVertex, color)
		, .texCoord_2f32 = offsetof(QuadVertex, textureNormal) };
	local_const u32 VERTEX_COUNT = CARRAY_SIZE(quadVertices);
	if(    krb::g_context->immediatePrimitiveType != GL_TRIANGLES 
		|| krb::g_context->immediateVertexStride != VERTEX_STRIDE 
		|| krb::g_context->immediateVertexAttributeOffsets != 
			VERTEX_ATTRIB_OFFSETS)
	{
		korl_rb_ogl_flushImmediateBuffer();
		krb::g_context->immediatePrimitiveType = GL_TRIANGLES;
		krb::g_context->immediateVertexStride = VERTEX_STRIDE;
		krb::g_context->immediateVertexAttributeOffsets = VERTEX_ATTRIB_OFFSETS;
		korl_rb_ogl_resetImmediateModeVertexAttributes();
	}
	/* check to see if the vertex VBO has enough bytes for vertices; ensure the 
		vertex VBO has enough capacity for the vertices! */
	korl_rb_ogl_reserveImmediateModeVboBytes(VERTEX_COUNT*VERTEX_STRIDE);
	/* build the quad vertices */
	const v2f32 quadMeshOffset = 
		{-ratioAnchor[0]*size[0], ratioAnchor[1]*size[1]};
	// bottom-left triangle //
	// top-left vertex //
	quadVertices[0].position = {quadMeshOffset.x, quadMeshOffset.y, 0};
	quadVertices[0].color = colors[0];
	quadVertices[0].textureNormal = {texNormalMin[0], texNormalMin[1]};
	// bottom-left vertex //
	quadVertices[1].position = 
		{quadMeshOffset.x, quadMeshOffset.y - size[1], 0};
	quadVertices[1].color = colors[1];
	quadVertices[1].textureNormal = {texNormalMin[0], texNormalMax[1]};
	// bottom-right vertex //
	quadVertices[2].position = 
		{quadMeshOffset.x + size[0], quadMeshOffset.y - size[1], 0};
	quadVertices[2].color = colors[2];
	quadVertices[2].textureNormal = {texNormalMax[0], texNormalMax[1]};
	// top-right triangle //
	// top-left vertex //
	quadVertices[3].position = {quadMeshOffset.x, quadMeshOffset.y, 0};
	quadVertices[3].color = colors[0];
	quadVertices[3].textureNormal = {texNormalMin[0], texNormalMin[1]};
	// bottom-right vertex //
	quadVertices[4].position = 
		{quadMeshOffset.x + size[0], quadMeshOffset.y - size[1], 0};
	quadVertices[4].color = colors[2];
	quadVertices[4].textureNormal = {texNormalMax[0], texNormalMax[1]};
	// top-right vertex //
	quadVertices[5].position = 
		{quadMeshOffset.x + size[0], quadMeshOffset.y, 0};
	quadVertices[5].color = colors[3];
	quadVertices[5].textureNormal = {texNormalMax[0], texNormalMin[1]};
	/* append vertices to the end of the VBO */
	glBindBuffer(GL_ARRAY_BUFFER, krb::g_context->vboImmediate);
	glBufferSubData(
		GL_ARRAY_BUFFER, krb::g_context->vboImmediateVertexCount*VERTEX_STRIDE, 
		VERTEX_COUNT*VERTEX_STRIDE, quadVertices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	krb::g_context->vboImmediateVertexCount += VERTEX_COUNT;
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
internal GLint 
	korl_rb_ogl_decodeImageInternalFormat(KorlPixelDataFormat pdf)
{
	switch(pdf)
	{
		case KorlPixelDataFormat::RGBA:
			break;
		case KorlPixelDataFormat::BGR:
			return GL_RGB8;
	}
	return GL_RGBA8;
}
internal GLenum 
	korl_rb_ogl_decodeImageFormat(KorlPixelDataFormat pdf)
{
	switch(pdf)
	{
		case KorlPixelDataFormat::RGBA:
			break;
		case KorlPixelDataFormat::BGR:
			return GL_BGR;
	}
	return GL_RGBA;
}
internal KRB_LOAD_IMAGE(krbLoadImage)
{
	GLuint texName;
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	const GLint internalFormat = 
		korl_rb_ogl_decodeImageInternalFormat(pixelDataFormat);
	const GLenum format = 
		korl_rb_ogl_decodeImageFormat(pixelDataFormat);
	glTexImage2D(
		GL_TEXTURE_2D, 0, internalFormat, imageSizeX, imageSizeY, 
		0, format, GL_UNSIGNED_BYTE, pixelData);
	GL_CHECK_ERROR();
	return texName;
}
internal KRB_DELETE_TEXTURE(krbDeleteTexture)
{
	glDeleteTextures(1, &krbTextureHandle);
	GL_CHECK_ERROR();
}
internal GLint korlRenderBackendDecodeTextureWrapMode(KorlTextureWrapMode m)
{
	switch(m)
	{
		case KorlTextureWrapMode::CLAMP:
			return GL_CLAMP_TO_EDGE;
		case KorlTextureWrapMode::REPEAT:
			return GL_REPEAT;
		case KorlTextureWrapMode::REPEAT_MIRRORED:
			return GL_MIRRORED_REPEAT;
		default:
			KLOG(ERROR, "Invalid texture meta data wrap mode (%i)!", 
			     static_cast<i32>(m));
	}
	return GL_CLAMP_TO_EDGE;
}
internal GLint korlRenderBackendDecodeTextureFilterMode(KorlTextureFilterMode m)
{
	switch(m)
	{
		case KorlTextureFilterMode::NEAREST:
			return GL_NEAREST;
		case KorlTextureFilterMode::LINEAR:
			return GL_LINEAR;
		default:
			KLOG(ERROR, "Invalid texture meta data filter mode (%i)!", 
			     static_cast<i32>(m));
	}
	return GL_NEAREST_MIPMAP_LINEAR;
}
internal KRB_USE_TEXTURE(krbUseTexture)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, kth);
	const GLint paramWrapS = 
		korlRenderBackendDecodeTextureWrapMode(texMeta.wrapX);
	const GLint paramWrapT = 
		korlRenderBackendDecodeTextureWrapMode(texMeta.wrapY);
	const GLint paramFilterMin = 
		korlRenderBackendDecodeTextureFilterMode(texMeta.filterMinify);
	const GLint paramFilterMag = 
		korlRenderBackendDecodeTextureFilterMode(texMeta.filterMagnify);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, paramWrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, paramWrapT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, paramFilterMin);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, paramFilterMag);
	GL_CHECK_ERROR();
}
internal KRB_WORLD_TO_SCREEN(krbWorldToScreen)
{
	korlAssert(worldPositionDimension < 4);
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
	/* We can determine if a projection matrix is orthographic or frustum based 
		on the last element of the W row.  See: 
		http://www.songho.ca/opengl/gl_projectionmatrix.html */
	korlAssert(mProjection.r3c3 == 1 || mProjection.r3c3 == 0);
	const bool isOrthographic = mProjection.r3c3 == 1;
	/* viewport-space          => normalized-device-space */
	const v2f32 eyeRayNds = 
		{  2*v2f32WindowPos.x / krb::g_context->windowSizeX - 1
		, -2*v2f32WindowPos.y / krb::g_context->windowSizeY + 1 };
	/* normalized-device-space => homogeneous-clip-space */
	/* arbitrarily set the eye ray direction vector as far to the "back" of the 
		homogeneous clip space box, which means setting the Z coordinate to a 
		value of -1, since coordinates are right-handed */
	const v4f32 eyeRayDirectionHcs = {eyeRayNds.x, eyeRayNds.y, -1, 1};
	/* homogeneous-clip-space  => eye-space */
	v4f32 eyeRayDirectionEs = mInverseProjection*eyeRayDirectionHcs;
	if(!isOrthographic)
		eyeRayDirectionEs.w = 0;
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
			{eyeRayPositionWs.x, eyeRayPositionWs.y, eyeRayPositionWs.z};
		resultDirection = 
			kmath::normal( v3f32{eyeRayDirectionWs.x, 
			                     eyeRayDirectionWs.y, 
			                     eyeRayDirectionWs.z} - resultPosition );
	}
	else
	{
		/* camera's eye position can be easily derived from the inverse view 
			matrix: https://stackoverflow.com/a/39281556/4526664 */
		resultPosition = 
			{mInverseView.r0c3, mInverseView.r1c3, mInverseView.r2c3};
		resultDirection = 
			kmath::normal( v3f32{eyeRayDirectionWs.x, 
			                     eyeRayDirectionWs.y, 
			                     eyeRayDirectionWs.z} );
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
internal KRB_SET_CLIP_BOX(krbSetClipBox)
{
	glEnable(GL_SCISSOR_TEST);
	glScissor(left, bottom, width, height);
	GL_CHECK_ERROR();
}
internal KRB_DISABLE_CLIP_BOX(krbDisableClipBox)
{
	glDisable(GL_SCISSOR_TEST);
	GL_CHECK_ERROR();
}