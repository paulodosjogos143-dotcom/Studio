// ROXBOX - headless GL stub (verification builds ONLY)
// Provides the small subset of the OpenGL / OpenGL ES 2.0 API used by
// gl_backend.cpp so the renderer compiles and links without a GPU driver.
#pragma once
#include <cstdint>
#include <cstddef>

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef char GLchar;
typedef void GLvoid;
typedef std::ptrdiff_t GLsizeiptr;
typedef std::ptrdiff_t GLintptr;

#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_STREAM_DRAW 0x88E0
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_ZERO 0
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TRUE 1
#define GL_FALSE 0

inline GLuint glGenBuffers(GLsizei, GLuint*) { return 0; }
inline void glBindBuffer(GLenum, GLuint) {}
inline void glBufferData(GLenum, GLsizeiptr, const void*, GLenum) {}
inline void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {}
inline void glEnableVertexAttribArray(GLuint) {}
inline void glDrawElements(GLenum, GLsizei, GLenum, const void*) {}
inline void glDrawArrays(GLenum, GLint, GLsizei) {}
#define GL_POINTS 0x0000
inline GLuint glGenTextures(GLsizei, GLuint*) { return 0; }
inline void glBindTexture(GLenum, GLuint) {}
inline void glActiveTexture(GLenum) {}
inline void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {}
inline void glTexParameteri(GLenum, GLenum, GLint) {}
inline GLuint glGenShaders(GLsizei, GLuint*) { return 0; }
inline GLuint glCreateShader(GLenum) { return 0; }
inline void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) {}
inline void glCompileShader(GLuint) {}
inline void glGetShaderiv(GLuint, GLenum param, GLint* v) { if (v) *v = GL_TRUE; }
inline void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline GLuint glCreateProgram() { return 1; }
inline void glAttachShader(GLuint, GLuint) {}
inline void glLinkProgram(GLuint) {}
inline void glGetProgramiv(GLuint, GLenum param, GLint* v) { if (v) *v = GL_TRUE; }
inline void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
inline void glUseProgram(GLuint) {}
inline GLint glGetUniformLocation(GLuint, const GLchar*) { return 0; }
inline void glUniform1f(GLint, GLfloat) {}
inline void glUniform1i(GLint, GLint) {}
inline void glUniform2f(GLint, GLfloat, GLfloat) {}
inline void glUniform3f(GLint, GLfloat, GLfloat, GLfloat) {}
inline void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat) {}
inline void glUniformMatrix4fv(GLint, GLsizei, GLboolean, const GLfloat*) {}
inline GLenum glGetError() { return 0; }
inline void glClear(GLenum) {}
inline void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat) {}
inline void glViewport(GLint, GLint, GLsizei, GLsizei) {}
inline void glEnable(GLenum) {}
inline void glDisable(GLenum) {}
inline void glBlendFunc(GLenum, GLenum) {}
inline void glDepthMask(GLboolean) {}
inline void glDeleteBuffers(GLsizei, const GLuint*) {}
inline void glDeleteTextures(GLsizei, const GLuint*) {}
inline void glDeleteShader(GLuint) {}
inline void glDeleteProgram(GLuint) {}
inline void glFinish() {}
