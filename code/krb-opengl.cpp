/** This code should all be a completely platform-independent implementation of 
 * the Kyle's Renderer Backend interface.  
*/
#include "krb-interface.h"
#include "korl-ogl-loader.h"
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
		"uniform mat4 uniform_xformMvp;\n"
		"uniform vec4 uniform_defaultColor;\n"
		"layout(location = 0) in vec3 attribute_position;\n"
		"out vec4 vertexColor;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = uniform_xformMvp * vec4(attribute_position, 1.0);\n"
		"	vertexColor = uniform_defaultColor;\n"
		"}\0";
	const GLchar*const sourceShaderImmediateVertexColor = 
		"#version 330 core\n"
		"uniform mat4 uniform_xformMvp;\n"
		"layout(location = 0) in vec3 attribute_position;\n"
		"layout(location = 1) in vec4 attribute_color;\n"
		"out vec4 vertexColor;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = uniform_xformMvp * vec4(attribute_position, 1.0);\n"
		"	vertexColor = attribute_color;\n"
		"}\0";
	const GLchar*const sourceShaderImmediateVertexColorTexNormal = 
		"#version 330 core\n"
		"uniform mat4 uniform_xformMvp;\n"
		"layout(location = 0) in vec3 attribute_position;\n"
		"layout(location = 1) in vec4 attribute_color;\n"
		"layout(location = 2) in vec2 attribute_texNormal;\n"
		"out vec4 vertexColor;\n"
		"out vec2 vertexTexNormal;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = uniform_xformMvp * vec4(attribute_position, 1.0);\n"
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
		"uniform sampler2D textureUnit0;\n"
		"in vec4 vertexColor;\n"
		"in vec2 vertexTexNormal;\n"
		"out vec4 fragmentColor;\n"
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
		else
		/* likewise, if we're drawing without a 'texCoord', then make sure we 
			haven't bound anything there to help prevent draw errors */
		{
			GLint nameTex2dUnit0;
			glActiveTexture(GL_TEXTURE0);
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &nameTex2dUnit0);
			korlAssert(!nameTex2dUnit0);
		}
		/* actually draw the immediate-mode buffer w/ correct render states */
		glUseProgram(program);
		/* set context xform state to the shader uniforms */
		/* @speed: instead of setting this every time we draw, we could upload 
			xform state to a UBO & use binding points for programs */
		{
			const GLint uniformLocXformMvp = 
				glGetUniformLocation(program, "uniform_xformMvp");
			korlAssert(uniformLocXformMvp > -1);
			glUniformMatrix4fv(
				uniformLocXformMvp, 1, GL_TRUE, 
				krb::g_context->m4ModelViewProjection.elements);
		}
		/* if we aren't using a 'color' vertex attribute, we must be using a 
			program which takes a default color */
		/* @speed: instead of setting this each time we draw, we could 
			potentially only set this uniform for all relevant programs ONLY 
			when the default color is changed (actually, we should just use UBO 
			binding points) */
		if(krb::g_context->immediateVertexAttributeOffsets.color_4f32 >= 
				krb::g_context->immediateVertexStride)
		{
			/* @speed: instead of searching for the uniform location every time, 
				we could just locate it once when the program is linked & then 
				just use the cached value(s) here depending on the value of 
				`program` (actually, we should just use UBO + binding points) */
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
internal void korl_rb_ogl_bufferImmediateVertices(
	u32 primitiveType, const KrbVertexAttributeOffsets& vertexAttribOffsets, 
	u32 vertexStride, const void* vertices, u32 vertexCount)
{
	korlAssert(vertexAttribOffsets.position_3f32 < vertexStride);
	if(vertexAttribOffsets.texCoord_2f32 >= vertexStride)
		krbUseTexture(krb::INVALID_TEXTURE_HANDLE, {});
	if(    krb::g_context->immediatePrimitiveType != primitiveType 
		|| krb::g_context->immediateVertexStride != vertexStride 
		|| krb::g_context->immediateVertexAttributeOffsets != 
			vertexAttribOffsets)
	{
		korl_rb_ogl_flushImmediateBuffer();
		krb::g_context->immediatePrimitiveType = primitiveType;
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
	krb::g_context->m4Model               = m4f32::IDENTITY;
	krb::g_context->m4View                = m4f32::IDENTITY;
	krb::g_context->m4Projection          = m4f32::IDENTITY;
	krb::g_context->m4ModelViewProjection = m4f32::IDENTITY;
}
internal KRB_END_FRAME(krbEndFrame)
{
	korlAssert(krb::g_context->frameInProgress);
	korl_rb_ogl_flushImmediateBuffer();
	krb::g_context->frameInProgress = false;
}
internal KRB_SET_DEPTH_TESTING(krbSetDepthTesting)
{
	const bool isDepthTestEnabled = (glIsEnabled(GL_DEPTH_TEST) == GL_TRUE);
	if(isDepthTestEnabled == enable)
		return;
	korl_rb_ogl_flushImmediateBuffer();
	if(enable)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
	GL_CHECK_ERROR();
}
internal KRB_SET_BACKFACE_CULLING(krbSetBackfaceCulling)
{
	const bool isFaceCullingEnabled = (glIsEnabled(GL_CULL_FACE) == GL_TRUE);
	if(isFaceCullingEnabled == enable)
		return;
	korl_rb_ogl_flushImmediateBuffer();
	if(enable)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);
	GL_CHECK_ERROR();
}
#if 0
internal KRB_SET_WIREFRAME(krbSetWireframe)
{
	if(enable)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	GL_CHECK_ERROR();
}
#endif//0
internal KRB_SET_PROJECTION_ORTHO(krbSetProjectionOrtho)
{
	korl_rb_ogl_flushImmediateBuffer();
	glViewport(0, 0, krb::g_context->windowSizeX, krb::g_context->windowSizeY);
	glDisable(GL_SCISSOR_TEST);
	const f32 left  = -static_cast<f32>(krb::g_context->windowSizeX)/2;
	const f32 right =  static_cast<f32>(krb::g_context->windowSizeX)/2;
	const f32 bottom = -static_cast<f32>(krb::g_context->windowSizeY)/2;
	const f32 top    =  static_cast<f32>(krb::g_context->windowSizeY)/2;
	const f32 zNear =  halfDepth;
	const f32 zFar  = -halfDepth;
	/* http://www.songho.ca/opengl/gl_projectionmatrix.html */
	krb::g_context->m4Projection = m4f32::IDENTITY;
	krb::g_context->m4Projection.r0c0 = 2 / (right - left);
	krb::g_context->m4Projection.r0c3 = -(right + left) / (right - left);
	krb::g_context->m4Projection.r1c1 = 2 / (top - bottom);
	krb::g_context->m4Projection.r1c3 = -(top + bottom) / (top - bottom);
	krb::g_context->m4Projection.r2c2 = -2 / (zFar - zNear);
	krb::g_context->m4Projection.r2c3 = -(zFar + zNear) / (zFar - zNear);
	/* clear MV matrix */
	krb::g_context->m4Model = m4f32::IDENTITY;
	krb::g_context->m4View  = m4f32::IDENTITY;
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_ORTHO_FIXED_HEIGHT(krbSetProjectionOrthoFixedHeight)
{
	korl_rb_ogl_flushImmediateBuffer();
	glViewport(0, 0, krb::g_context->windowSizeX, krb::g_context->windowSizeY);
	glDisable(GL_SCISSOR_TEST);
	/*
		w / fixedHeight == windowAspectRatio
	*/
	const f32 windowAspectRatio = krb::g_context->windowSizeY == 0
		? 1.f 
		: static_cast<f32>(krb::g_context->windowSizeX) / 
			krb::g_context->windowSizeY;
	const f32 viewportWidth = windowAspectRatio * fixedHeight;
	const f32 left  = -viewportWidth / 2.f;
	const f32 right =  viewportWidth / 2.f;
	const f32 bottom = -fixedHeight / 2.f;
	const f32 top    =  fixedHeight / 2.f;
	const f32 zNear =  halfDepth;
	const f32 zFar  = -halfDepth;
	/* http://www.songho.ca/opengl/gl_projectionmatrix.html */
	krb::g_context->m4Projection = m4f32::IDENTITY;
	krb::g_context->m4Projection.r0c0 = 2 / (right - left);
	krb::g_context->m4Projection.r0c3 = -(right + left) / (right - left);
	krb::g_context->m4Projection.r1c1 = 2 / (top - bottom);
	krb::g_context->m4Projection.r1c3 = -(top + bottom) / (top - bottom);
	krb::g_context->m4Projection.r2c2 = -2 / (zFar - zNear);
	krb::g_context->m4Projection.r2c3 = -(zFar + zNear) / (zFar - zNear);
	/* clear MV matrix */
	krb::g_context->m4Model = m4f32::IDENTITY;
	krb::g_context->m4View  = m4f32::IDENTITY;
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
	GL_CHECK_ERROR();
}
internal KRB_SET_PROJECTION_FOV(krbSetProjectionFov)
{
	korl_rb_ogl_flushImmediateBuffer();
	glViewport(0, 0, krb::g_context->windowSizeX, krb::g_context->windowSizeY);
	glDisable(GL_SCISSOR_TEST);
	const f32 aspectRatio = 
		static_cast<f32>(krb::g_context->windowSizeX) / 
		krb::g_context->windowSizeY;
	korlAssert(!kmath::isNearlyZero(aspectRatio));
	korlAssert(!kmath::isNearlyEqual(clipNear, clipFar) && clipFar > clipNear);
	const f32 horizonFovRadians = horizonFovDegrees*PI32/180;
	const f32 tanHalfFov = tanf(horizonFovRadians / 2);
	krb::g_context->m4Projection = {};
	// http://ogldev.org/www/tutorial12/tutorial12.html
	// http://www.songho.ca/opengl/gl_projectionmatrix.html
	krb::g_context->m4Projection.r0c0 = 1 / (aspectRatio * tanHalfFov);
	krb::g_context->m4Projection.r1c1 = 1 / tanHalfFov;
	/* I negate the Z row calculation to maintain RIGHT-HANDED coordinates! */
	krb::g_context->m4Projection.r2c2 = 
		(clipFar + clipNear) / (clipFar - clipNear);
	krb::g_context->m4Projection.r2c3 = 
		2*clipFar*clipNear   / (clipFar - clipNear);
	krb::g_context->m4Projection.r3c2 = -1;
	/* clear MV matrix */
	krb::g_context->m4Model = m4f32::IDENTITY;
	krb::g_context->m4View  = m4f32::IDENTITY;
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
	GL_CHECK_ERROR();
}
internal KRB_LOOK_AT(krbLookAt)
{
	korl_rb_ogl_flushImmediateBuffer();
	const v3f32*const eye = reinterpret_cast<const v3f32*>(v3f32_eye);
	// https://www.3dgep.com/understanding-the-view-matrix/
	/* camera Z-axis points AWAY from the target to maintain RIGHT-HANDED 
		coordinates! */
	const v3f32 camAxisZ = kmath::normal(
		*eye - *reinterpret_cast<const v3f32*>(v3f32_target));
	const v3f32 camAxisX = kmath::normal(
		reinterpret_cast<const v3f32*>(v3f32_worldUp)->cross(camAxisZ));
	const v3f32 camAxisY = camAxisZ.cross(camAxisX);
	krb::g_context->m4View = m4f32::IDENTITY;
	m4f32& mView = krb::g_context->m4View;
	mView.r0c0 = camAxisX.x; mView.r0c1 = camAxisX.y; mView.r0c2 = camAxisX.z;
	mView.r1c0 = camAxisY.x; mView.r1c1 = camAxisY.y; mView.r1c2 = camAxisY.z;
	mView.r2c0 = camAxisZ.x; mView.r2c1 = camAxisZ.y; mView.r2c2 = camAxisZ.z;
	mView.r0c3 = -camAxisX.dot(*eye);
	mView.r1c3 = -camAxisY.dot(*eye);
	mView.r2c3 = -camAxisZ.dot(*eye);
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
}
internal KRB_DRAW_POINTS(krbDrawPoints)
{
	korlAssert(vertexAttribOffsets.position_3f32 < vertexStride);
	korl_rb_ogl_bufferImmediateVertices(
		GL_POINTS, vertexAttribOffsets, vertexStride, vertices, vertexCount);
}
internal KRB_DRAW_LINES(krbDrawLines)
{
	korlAssert(vertexCount % 2 == 0);
	korl_rb_ogl_bufferImmediateVertices(
		GL_LINES, vertexAttribOffsets, vertexStride, vertices, vertexCount);
}
internal KRB_DRAW_TRIS(krbDrawTris)
{
	korlAssert(vertexCount % 3 == 0);
	korl_rb_ogl_bufferImmediateVertices(
		GL_TRIANGLES, vertexAttribOffsets, vertexStride, vertices, vertexCount);
}
internal KRB_DRAW_QUAD(krbDrawQuad)
{
	struct QuadVertex
	{
		v3f32 position;
		RgbaF32 color;
	} quadVertices[6];
	local_const u32 VERTEX_STRIDE = sizeof(QuadVertex);
	local_const KrbVertexAttributeOffsets VERTEX_ATTRIB_OFFSETS = 
		{ .position_3f32 = offsetof(QuadVertex, position)
		, .color_4f32    = offsetof(QuadVertex, color)
		, .texCoord_2f32 = sizeof(QuadVertex) };
	local_const u32 VERTEX_COUNT = CARRAY_SIZE(quadVertices);
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
	/* submit the quad mesh to the immediate-mode buffer */
	korl_rb_ogl_bufferImmediateVertices(
		GL_TRIANGLES, VERTEX_ATTRIB_OFFSETS, VERTEX_STRIDE, 
		quadVertices, VERTEX_COUNT);
}
internal KRB_DRAW_QUAD_TEXTURED(krbDrawQuadTextured)
{
	struct QuadVertex
	{
		v3f32 position;
		RgbaF32 color;
		v2f32 textureNormal;
	} quadVertices[6];
	local_const u32 VERTEX_STRIDE = sizeof(QuadVertex);
	local_const KrbVertexAttributeOffsets VERTEX_ATTRIB_OFFSETS = 
		{ .position_3f32 = offsetof(QuadVertex, position)
		, .color_4f32    = offsetof(QuadVertex, color)
		, .texCoord_2f32 = offsetof(QuadVertex, textureNormal) };
	local_const u32 VERTEX_COUNT = CARRAY_SIZE(quadVertices);
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
	/* submit the quad mesh to the immediate-mode buffer */
	korl_rb_ogl_bufferImmediateVertices(
		GL_TRIANGLES, VERTEX_ATTRIB_OFFSETS, VERTEX_STRIDE, 
		quadVertices, VERTEX_COUNT);
}
internal KRB_DRAW_CIRCLE(krbDrawCircle)
{
	if(vertexCount < 3)
	{
		KLOG(WARNING, "Attempting to draw a degenerate circle!  Ignoring...");
		return;
	}
	struct CircleVertex
	{
		v3f32 position;
	} vertexPool[128];
	/* in order to draw a circle, we require at minimum a vertex buffer of 
		2*(vertexCount + 1) because that is how many vertices are required to 
		draw an outline for the circle with non-zero thickness 
		(triangle strip) */
	if((2*(vertexCount + 1)) > CARRAY_SIZE(vertexPool))
	{
		KLOG(WARNING, "vertexCount(2 * (%u + 1)) > %u! Unable to draw circle.", 
			vertexCount, CARRAY_SIZE(vertexPool));
		return;
	}
	local_const u32 VERTEX_STRIDE = sizeof(CircleVertex);
	local_const KrbVertexAttributeOffsets VERTEX_ATTRIB_OFFSETS = 
		{ .position_3f32 = offsetof(CircleVertex, position)
		, .color_4f32    = sizeof(CircleVertex)
		, .texCoord_2f32 = sizeof(CircleVertex) };
	const f32 deltaRadians = 2*PI32 / vertexCount;
	u32 poolVertexCount;
	/* draw the fill primitives */
	krbSetDefaultColor(colorFill);
	poolVertexCount = 0;
	vertexPool[poolVertexCount++] = {v3f32::ZERO};
	for(u16 v = 0; v <= vertexCount; v++)
	{
		const f32 radians = v*deltaRadians;
		vertexPool[poolVertexCount++] = 
			{v3f32{radius*cosf(radians), radius*sinf(radians), 0}};
	}
	korl_rb_ogl_bufferImmediateVertices(
		GL_TRIANGLE_FAN, VERTEX_ATTRIB_OFFSETS, VERTEX_STRIDE, 
		vertexPool, poolVertexCount);
	/* draw the outline primitives IFF the outline color is NON-ZERO! */
	if(!kmath::isNearlyZero(colorOutline.a))
	{
		krbSetDefaultColor(colorOutline);
		/* draw the outline as a line if the thickness is set to 0 */
		if(kmath::isNearlyZero(outlineThickness))
		{
			korl_rb_ogl_bufferImmediateVertices(
				GL_LINE_LOOP, VERTEX_ATTRIB_OFFSETS, VERTEX_STRIDE, 
				/* we can simply re-use the fill vertices by excluding the 
					initial vertex placed at the center of the triangle fan */
				vertexPool + 1, poolVertexCount - 1);
		}
		/* otherwise, draw the outline as a triangle strip */
		else
		{
			poolVertexCount = 0;
			for(u16 v = 0; v <= vertexCount; v++)
			{
				const f32 radians = v*deltaRadians;
				vertexPool[poolVertexCount++] = 
					{v3f32{radius*cosf(radians), radius*sinf(radians), 0}};
				vertexPool[poolVertexCount++] = 
					{ v3f32
						{ (radius+outlineThickness)*cosf(radians)
						, (radius+outlineThickness)*sinf(radians), 0 } };
				korl_rb_ogl_bufferImmediateVertices(
					GL_TRIANGLE_STRIP, VERTEX_ATTRIB_OFFSETS, VERTEX_STRIDE, 
					vertexPool, poolVertexCount);
			}
		}
	}
}
internal KRB_SET_VIEW_XFORM_2D(krbSetViewXform2d)
{
	korl_rb_ogl_flushImmediateBuffer();
	krb::g_context->m4View = m4f32::IDENTITY;
	krb::g_context->m4View.r0c3 = -worldPositionCenter.x;
	krb::g_context->m4View.r1c3 = -worldPositionCenter.y;
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
}
internal KRB_SET_MODEL_XFORM(krbSetModelXform)
{
	korl_rb_ogl_flushImmediateBuffer();
	kmath::makeM4f32(
		orientation, translation, scale, &krb::g_context->m4Model);
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
}
internal KRB_SET_MODEL_XFORM_2D(krbSetModelXform2d)
{
	korl_rb_ogl_flushImmediateBuffer();
	kmath::makeM4f32(
		orientation, {translation.x, translation.y, 0}, {scale.x, scale.y, 1}, 
		&krb::g_context->m4Model);
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
}
internal KRB_SET_MODEL_MATRIX(krbSetModelMatrix)
{
	korl_rb_ogl_flushImmediateBuffer();
	for(u32 i = 0; i < 16; i++)
		krb::g_context->m4Model.elements[i] = rowMajorMatrix4x4[i];
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
}
internal KRB_GET_MATRICES_MVP(krbGetMatricesMvp)
{
	if(o_model)
		for(u32 i = 0; i < 16; i++)
			o_model[i] = krb::g_context->m4Model.elements[i];
	if(o_view)
		for(u32 i = 0; i < 16; i++)
			o_view[i] = krb::g_context->m4View.elements[i];
	if(o_projection)
		for(u32 i = 0; i < 16; i++)
			o_projection[i] = krb::g_context->m4Projection.elements[i];
}
internal KRB_SET_MATRICES_MVP(krbSetMatricesMvp)
{
	if(model || view || projection)
		korl_rb_ogl_flushImmediateBuffer();
	else
		return;
	if(model)
		for(u32 i = 0; i < 16; i++)
			krb::g_context->m4Model.elements[i] = model[i];
	if(view)
		for(u32 i = 0; i < 16; i++)
			krb::g_context->m4View.elements[i] = view[i];
	if(projection)
		for(u32 i = 0; i < 16; i++)
			krb::g_context->m4Projection.elements[i] = projection[i];
	/* cache MVP matrix */
	krb::g_context->m4ModelViewProjection = 
		krb::g_context->m4Projection * krb::g_context->m4View * 
		krb::g_context->m4Model;
}
#if 0
internal KRB_SET_MODEL_XFORM_BILLBOARD(krbSetModelXformBillboard)
{
	m4f32 modelView;
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
#endif//0
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
	/* save old texture2dUnit0 since we're going to use that slot for uploading 
		our image */
	glActiveTexture(GL_TEXTURE0);
	GLint nameTex2dUnit0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &nameTex2dUnit0);
	/* generate a new texture object and upload the image to it */
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
	/* restore the old texture2dUnit0 state */
	glBindTexture(GL_TEXTURE_2D, nameTex2dUnit0);
	/* check for errors and return */
	GL_CHECK_ERROR();
	return texName;
}
internal KRB_DELETE_TEXTURE(krbDeleteTexture)
{
	glDeleteTextures(1, &krbTextureHandle);
	GL_CHECK_ERROR();
}
internal GLint korl_rb_ogl_decodeTextureWrapMode(KorlTextureWrapMode m)
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
internal GLint korl_rb_ogl_decodeTextureFilterMode(KorlTextureFilterMode m)
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
	/* if the texture we are binding differs from the one which is currently 
		bound, then we need to flush the immediate buffer */
	{
		GLint nameTex2dUnit0;
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &nameTex2dUnit0);
		static_assert(sizeof(KrbTextureHandle) == sizeof(GLint));
		if(nameTex2dUnit0 != static_cast<GLint>(kth))
			korl_rb_ogl_flushImmediateBuffer();
	}
	glBindTexture(GL_TEXTURE_2D, kth);
	if(kth != krb::INVALID_TEXTURE_HANDLE)
	{
		const GLint paramWrapS = 
			korl_rb_ogl_decodeTextureWrapMode(texMeta.wrapX);
		const GLint paramWrapT = 
			korl_rb_ogl_decodeTextureWrapMode(texMeta.wrapY);
		const GLint paramFilterMin = 
			korl_rb_ogl_decodeTextureFilterMode(texMeta.filterMinify);
		const GLint paramFilterMag = 
			korl_rb_ogl_decodeTextureFilterMode(texMeta.filterMagnify);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, paramWrapS);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, paramWrapT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, paramFilterMin);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, paramFilterMag);
	}
	GL_CHECK_ERROR();
}
internal KRB_WORLD_TO_SCREEN(krbWorldToScreen)
{
	local_const v2f32 INVALID_RESULT = {nanf(""), nanf("")};
	korlAssert(worldPositionDimension < 4);
	/* obtain viewport size & viewport offset from driver */
	GLint viewportValues[4];// [x,y, width,height]
	glGetIntegerv(GL_VIEWPORT, viewportValues);
	GL_CHECK_ERROR();
	/* calculate clip-space */
	v4f32 worldPoint = {0,0,0,1};
	for(u8 d = 0; d < worldPositionDimension; d++)
		worldPoint.elements[d] = pWorldPosition[d];
	const v4f32 cameraSpacePoint = krb::g_context->m4View * worldPoint;
	const v4f32 clipSpacePoint = 
		krb::g_context->m4Projection * cameraSpacePoint;
	if(kmath::isNearlyZero(clipSpacePoint.elements[3]))
		return INVALID_RESULT;
	/* calculate normalized-device-coordinate-space 
		y is inverted here because screen-space y axis is flipped! */
	const v3f32 ndcSpacePoint = 
		{ clipSpacePoint.elements[0] / clipSpacePoint.elements[3]
		, clipSpacePoint.elements[1] / clipSpacePoint.elements[3] * -1
		, clipSpacePoint.elements[2] / clipSpacePoint.elements[3] };
	/* Calculate screen-space.  GLSL formula: 
		((ndcSpacePoint.xy + 1.0) / 2.0) * viewSize + viewOffset */
	const v2f32 result = 
		{ ((ndcSpacePoint.x + 1.f) / 2.f) * viewportValues[2] + 
			viewportValues[0]
		, ((ndcSpacePoint.y + 1.f) / 2.f) * viewportValues[3] + 
			viewportValues[1] };
	return result;
}
internal KRB_SCREEN_TO_WORLD(krbScreenToWorld)
{
	/* at this point, we have everything we need from the GL to calculate the 
		eye ray: */
	m4f32 mInverseView;
	m4f32 mInverseProjection;
	if(!m4f32::invert(krb::g_context->m4View.elements, mInverseView.elements))
	{
		KLOG(WARNING, "Failed to invert the view matrix!");
		return false;
	}
	if(!m4f32::invert(
		krb::g_context->m4Projection.elements, mInverseProjection.elements))
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
	korlAssert(
		   krb::g_context->m4Projection.r3c3 == 1 
		|| krb::g_context->m4Projection.r3c3 == 0);
	const bool isOrthographic = krb::g_context->m4Projection.r3c3 == 1;
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
			kmath::normal(v3f32
				{ eyeRayDirectionWs.x
				, eyeRayDirectionWs.y
				, eyeRayDirectionWs.z} - resultPosition );
	}
	else
	{
		/* camera's eye position can be easily derived from the inverse view 
			matrix: https://stackoverflow.com/a/39281556/4526664 */
		resultPosition = 
			{mInverseView.r0c3, mInverseView.r1c3, mInverseView.r2c3};
		resultDirection = 
			kmath::normal(v3f32
				{ eyeRayDirectionWs.x
				, eyeRayDirectionWs.y
				, eyeRayDirectionWs.z });
	}
	return true;
}
internal KRB_SET_CURRENT_CONTEXT(krbSetCurrentContext)
{
	krb::g_context = context;
}
internal KRB_SET_DEFAULT_COLOR(krbSetDefaultColor)
{
	if(color == krb::g_context->defaultColor)
		return;
	korl_rb_ogl_flushImmediateBuffer();
	krb::g_context->defaultColor = color;
}
internal KRB_SET_CLIP_BOX(krbSetClipBox)
{
	const GLboolean isScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
	GLint scissorBox[4];
	glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
	if(!isScissorEnabled 
			|| scissorBox[0] != left  || scissorBox[1] != bottom 
			|| scissorBox[2] != kmath::safeTruncateI32(width) 
			|| scissorBox[3] != kmath::safeTruncateI32(height))
		korl_rb_ogl_flushImmediateBuffer();
	glEnable(GL_SCISSOR_TEST);
	glScissor(left, bottom, width, height);
	GL_CHECK_ERROR();
}
internal KRB_DISABLE_CLIP_BOX(krbDisableClipBox)
{
	const GLboolean isScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
	if(isScissorEnabled)
		korl_rb_ogl_flushImmediateBuffer();
	glDisable(GL_SCISSOR_TEST);
	GL_CHECK_ERROR();
}