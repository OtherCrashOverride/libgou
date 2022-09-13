/*
libgou - Support library for the ODROID-GO Ultra
Copyright (C) 2022 OtherCrashOverride

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "context3d.h"

#include "surface.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <drm/drm_fourcc.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


typedef struct gou_context3d
{
    EGLDisplay eglDisplay;
    EGLSurface eglSurface;
    EGLContext eglContext;
    
    gou_surface_t* surface;
    EGLImageKHR image;
    GLuint fbo;
    GLuint depthBuffer;
    GLuint texture2D;
} gou_context3d_t;





static void GLCheckError()
{
    int error = glGetError();
    if (error != GL_NO_ERROR)
    {
        printf("OpenGL error=0x%x\n", error);
        abort();
    }
}

static void EglCheckError()
{
	EGLint error = eglGetError();
	if (error != EGL_SUCCESS)
	{
		printf("EGL error=0x%x\n", error);
        abort();
	}
}


static void CreateContext(gou_context3d_t* context, EGLint glMajor, EGLint glMinor)
{
    EGLBoolean success;


    context->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (context->eglDisplay == EGL_NO_DISPLAY)
    {
        printf("eglGetDisplay failed.\n");
        abort();
    }


    // Initialize EGL
    EGLint major;
    EGLint minor;
    success = eglInitialize(context->eglDisplay, &major, &minor);
    if (success != EGL_TRUE)
    {
        printf("eglInitialize failed.\n");
        abort();
    }

    printf("EGL: major=%d, minor=%d\n", major, minor);
    printf("EGL: Vendor=%s\n", eglQueryString(context->eglDisplay, EGL_VENDOR));
    printf("EGL: Version=%s\n", eglQueryString(context->eglDisplay, EGL_VERSION));
    printf("EGL: ClientAPIs=%s\n", eglQueryString(context->eglDisplay, EGL_CLIENT_APIS));
    printf("EGL: Extensions=%s\n", eglQueryString(context->eglDisplay, EGL_EXTENSIONS));
    printf("EGL: ClientExtensions=%s\n", eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    printf("\n");


    // Create a context
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint contextAttributes[] = {
        EGL_CONTEXT_MAJOR_VERSION_KHR, glMajor,
        EGL_CONTEXT_MINOR_VERSION_KHR, glMinor,
        EGL_NONE };


    //   EGL_KHR_no_config_context
    context->eglContext = eglCreateContext(context->eglDisplay, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, contextAttributes);
    if (context->eglContext == EGL_NO_CONTEXT)
    {
        printf("eglCreateContext failed\n");
        abort();
    }



    //   EGL_KHR_surfaceless_context
    success = eglMakeCurrent(context->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context->eglContext);
    if (success != EGL_TRUE)
    {
        printf("eglMakeCurrent failed\n");
        abort();
    }
}

static EGLImageKHR CreateEglImage(gou_context3d_t* context, gou_surface_t* surface)
{
    EGLint img_attrs[] = {
        EGL_WIDTH, gou_surface_width_get(surface),
        EGL_HEIGHT, gou_surface_height_get(surface),
        EGL_LINUX_DRM_FOURCC_EXT, gou_surface_format_get(surface),
        EGL_DMA_BUF_PLANE0_FD_EXT, gou_surface_share_fd(surface),
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, gou_surface_stride_get(surface),
        EGL_NONE
    };

    static PFNEGLCREATEIMAGEKHRPROC p_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
    if (!p_eglCreateImageKHR) abort();

    EGLImageKHR eglImage = p_eglCreateImageKHR(context->eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, 0, img_attrs);
    if (eglImage == EGL_NO_IMAGE_KHR)
    {
        printf("eglCreateImageKHR failed.\n");
        abort();
    }

    return eglImage;
}

static void DestroyEglImage(gou_context3d_t* context, EGLImageKHR eglImage)
{
    static PFNEGLDESTROYIMAGEKHRPROC p_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
    if (!p_eglDestroyImageKHR) abort();

    p_eglDestroyImageKHR(context->eglDisplay, eglImage);
}



gou_context3d_t* gou_context3d_create(gou_display_t* display, int width, int height, const gou_context3d_attributes_t* attributes)
{
    gou_context3d_t* result = (gou_context3d_t*)malloc(sizeof(gou_context3d_t));
    if (!result)
    {
        printf("malloc failed.\n");
        abort();
    }

    memset(result, 0, sizeof(*result));


    result->depthBuffer = 0;


    uint32_t format;
    if (attributes->red_bits == 5 && attributes->green_bits == 6 && attributes->blue_bits == 5)
    {
        format = DRM_FORMAT_RGB565;
    }
    else
    {
        format = DRM_FORMAT_ABGR8888;
    }
    
    
    CreateContext(result, attributes->major, attributes->minor);


    result->surface = gou_surface_create(display, width, height, format);
    result->image = CreateEglImage(result, result->surface);


    glGenFramebuffers(1, &result->fbo);
    GLCheckError();

    glBindFramebuffer(GL_FRAMEBUFFER, result->fbo);
    GLCheckError();

    glGenTextures(1, &result->texture2D);
    GLCheckError();

    glActiveTexture(GL_TEXTURE0);
    GLCheckError();

    glBindTexture(GL_TEXTURE_2D, result->texture2D);
    GLCheckError();

    // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // GLCheckError();

    // glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // GLCheckError();

    static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC p_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if (!p_glEGLImageTargetTexture2DOES) abort();

    p_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, result->image);
    GLCheckError();

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,	result->texture2D, 0);
    GLCheckError();


    if (attributes->depth_bits)
    {
        glGenRenderbuffers(1, &result->depthBuffer);
        GLCheckError();

        glBindRenderbuffer(GL_RENDERBUFFER, result->depthBuffer);
        GLCheckError();

        glRenderbufferStorage(GL_RENDERBUFFER, attributes->depth_bits == 24 ? GL_DEPTH_COMPONENT24_OES : GL_DEPTH_COMPONENT16_OES, width, height);
        GLCheckError();

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, result->depthBuffer);
        GLCheckError();
    }


    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("FBO: Not Complete (status=0x%x).\n", fboStatus);
        abort();
    }

    return result;
}

void gou_context3d_destroy(gou_context3d_t* context)
{
    // TODO - destroy context
    abort();
}


    
void* gou_context3d_egldisplay_get(gou_context3d_t* context)
{
    return context->eglDisplay;
}

void* gou_context3d_eglcontext_get(gou_context3d_t* context)
{
    return context->eglContext;
}

int gou_context3d_fbo_get(gou_context3d_t* context)
{
    return context->fbo;
}

void gou_context3d_make_current(gou_context3d_t* context)
{
    EGLBoolean success = eglMakeCurrent(context->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context->eglContext);
    if (success != EGL_TRUE)
    {
        printf("eglMakeCurrent failed\n");
        abort();
    } 
}

void gou_context3d_swap_buffers(gou_context3d_t* context)
{
    // Noop
}

gou_surface_t* gou_context3d_surface_lock(gou_context3d_t* context)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFinish();

    return context->surface;
}

void gou_context3d_surface_unlock(gou_context3d_t* context, gou_surface_t* surface)
{
    glBindFramebuffer(GL_FRAMEBUFFER, context->fbo);
}
