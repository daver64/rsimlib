#pragma once

#ifdef _WIN32
#include <GL/glew.h>
#elif defined(SIMLIB_RENDERER_GLES)
#include <GLES3/gl31.h>
#include <GLES3/gl3ext.h>
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif
#ifndef GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif
#else
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#endif
