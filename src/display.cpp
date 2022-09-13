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


#include "dirent.h"

#include "surface.h"

#include <queue>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <linux/fb.h>
#include <sys/ioctl.h>
#include <drm/drm_fourcc.h>
#include <linux/dma-buf.h>
#include <linux/kd.h>
#include <semaphore.h>
#include <pthread.h>

#include "ge2d.h"
#include "ge2d_cmd.h"

#include "surface.h"


#define FBIOGET_OSD_DMABUF               0x46fc
struct fb_dmabuf_export {
	__u32 buffer_idx;
	__u32 fd;
	__u32 flags;
};

typedef struct gou_display
{
    int fd;
    int width;
    int height;
    std::queue<int>* freeFrameBuffers;
    std::queue<int>* usedFrameBuffers;
    pthread_mutex_t queueMutex;
    sem_t freeSem;
    sem_t usedSem;
    pthread_t renderThread; 
    bool terminating;
    uint32_t backgroundColor;
} gou_display_t;


static int ge2d_fd = -1;


static uint32_t GE2DFormat(uint32_t drm_fourcc)
{
    switch (drm_fourcc)
    {
        // 32bit
        case DRM_FORMAT_RGBA8888:
        case DRM_FORMAT_RGBX8888:
            return GE2D_FORMAT_S32_RGBA;
    
        case DRM_FORMAT_BGRX8888:
            return GE2D_FORMAT_S32_BGRA;
   
        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_XRGB8888:
            return GE2D_FORMAT_S32_ARGB;
    
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_XBGR8888:
            return GE2D_FORMAT_S32_ABGR;


        // 24bit
        case DRM_FORMAT_RGB888:
            return GE2D_FORMAT_S24_RGB;
 
        case DRM_FORMAT_BGR888:
            return GE2D_FORMAT_S24_BGR;


        // 16bit
        case DRM_FORMAT_RGB565:
            return GE2D_FORMAT_S16_RGB_565;

        case DRM_FORMAT_RGBA5551:
            return GE2D_FORMAT_S16_ARGB_1555;
    
        case DRM_FORMAT_RGBA4444:
            return GE2D_FORMAT_S16_RGBA_4444;

    
        default:
            printf("GE2D not supported. ");
            printf("drm_fourcc=%c%c%c%c\n", drm_fourcc & 0xff, drm_fourcc >> 8 & 0xff, drm_fourcc >> 16 & 0xff, drm_fourcc >> 24);
            abort();
    }
}

static void ClearScreen(uint32_t color, int width, int height, int fullWidth, int fullHeight, int voffset)
{
    int io;


    config_ge2d_para_ex_s ex_mem = { 0 };

    config_para_ex_ion_s& fill_config = ex_mem.para_config_memtype._ge2d_config_ex;

    fill_config.alu_const_color = 0xffffffff;

    fill_config.src_para.mem_type = CANVAS_TYPE_INVALID;

    fill_config.src2_para.mem_type = CANVAS_TYPE_INVALID;

    fill_config.dst_para.mem_type = CANVAS_OSD0;
    fill_config.dst_para.format = GE2D_FORMAT_S32_ARGB;
    fill_config.dst_para.left = 0;
    fill_config.dst_para.top = 0;
    fill_config.dst_para.width = fullWidth;
    fill_config.dst_para.height = fullHeight;
    fill_config.dst_para.x_rev = 0;
    fill_config.dst_para.y_rev = 0;

    fill_config.dst_planes[0].w = fullWidth;
    fill_config.dst_planes[0].h = fullHeight;


    ex_mem.para_config_memtype.src1_mem_alloc_type = AML_GE2D_MEM_INVALID;
    ex_mem.para_config_memtype.src2_mem_alloc_type = AML_GE2D_MEM_INVALID;
    ex_mem.para_config_memtype.dst_mem_alloc_type = AML_GE2D_MEM_INVALID;


    io = ioctl(ge2d_fd, GE2D_CONFIG_EX_MEM, &ex_mem);
    if (io < 0)
    {
        printf("GE2D_CONFIG failed\n");
        abort();
    }


    // Convert ABGR -> RGBA
    const uint8_t a = ((color & 0xff000000) >> 24);
    const uint8_t b = ((color & 0x00ff0000) >> 16);
    const uint8_t g = ((color & 0x0000ff00) >> 8);
    const uint8_t r = (color & 0x000000ff);
    const uint32_t rgba = (r << 24) | (g << 16) | (b << 8) | a;

    // Perform the fill operation
    ge2d_para_s fillRect = { 0 };

    fillRect.src1_rect.x = 0;
    fillRect.src1_rect.y = voffset;
    fillRect.src1_rect.w = width;
    fillRect.src1_rect.h = height;
    fillRect.color = rgba;

    io = ioctl(ge2d_fd, GE2D_FILLRECTANGLE, &fillRect);
    if (io < 0)
    {
        printf("GE2D_FILLRECTANGLE failed.\n");
        abort();
    }
}

static void Blit(gou_surface_t* src, int srcX, int srcY, int srcWidth, int srcHeight, bool hMirror, bool yMirror,
          int dstX, int dstY, int dstWidth, int dstHeight, int fullWidth, int fullHeight, int voffset, gou_rotation_t rotation)
{
    int io;

    config_ge2d_para_ex_s ex_mem = { 0 };

    config_para_ex_ion_s& blit_config = ex_mem.para_config_memtype._ge2d_config_ex;

    blit_config.alu_const_color = 0xffffffff;

    blit_config.src_para.mem_type = CANVAS_ALLOC;
    blit_config.src_para.format = GE2DFormat(gou_surface_format_get(src));

    blit_config.src_para.left = 0;
    blit_config.src_para.top = 0;
    blit_config.src_para.width = gou_surface_width_get(src);
    blit_config.src_para.height = gou_surface_height_get(src);
    blit_config.src_para.x_rev = hMirror ? 1 : 0;
    blit_config.src_para.y_rev = yMirror ? 1 : 0;

    blit_config.src_planes[0].shared_fd = gou_surface_share_fd(src);
    blit_config.src_planes[0].w = gou_surface_stride_get(src) / (gou_drm_format_get_bpp(gou_surface_format_get(src)) / 8);
    blit_config.src_planes[0].h = gou_surface_height_get(src);
  

    ex_mem.para_config_memtype.src1_mem_alloc_type = AML_GE2D_MEM_ION;
    ex_mem.para_config_memtype.src2_mem_alloc_type = AML_GE2D_MEM_INVALID;
    
    

    blit_config.dst_para.mem_type = CANVAS_OSD0;
    blit_config.dst_para.format = GE2D_FORMAT_S32_ARGB;
    blit_config.dst_para.left = 0;
    blit_config.dst_para.top = 0;
    blit_config.dst_para.width = fullWidth;
    blit_config.dst_para.height = fullHeight;
    blit_config.dst_para.x_rev = 0;
    blit_config.dst_para.y_rev = 0;

    blit_config.dst_planes[0].addr = 0;
    blit_config.dst_planes[0].w = fullWidth;
    blit_config.dst_planes[0].h = fullHeight;

    ex_mem.para_config_memtype.dst_mem_alloc_type = AML_GE2D_MEM_INVALID;

 
    int tmp;
    switch (rotation)
    {
        case GOU_ROTATION_DEGREES_0:
            break;

        case GOU_ROTATION_DEGREES_90:
            blit_config.dst_xy_swap = 1;
            blit_config.dst_para.x_rev = 1;

            tmp = dstWidth;
            dstWidth = dstHeight;
            dstHeight = tmp;
            break;

        case GOU_ROTATION_DEGREES_180:
            blit_config.dst_para.x_rev = 1;
            blit_config.dst_para.y_rev = 1;
            break;

        case GOU_ROTATION_DEGREES_270:
            blit_config.dst_xy_swap = 1;
            blit_config.dst_para.y_rev = 1;

            tmp = dstWidth;
            dstWidth = dstHeight;
            dstHeight = tmp;
            break;
            
        default:
            break;
    }


    io = ioctl(ge2d_fd, GE2D_CONFIG_EX_MEM, &ex_mem);
    if (io < 0)
    {
        printf("GE2D_CONFIG failed\n");
        abort();
    }


    ge2d_para_s blitRect = { 0 };

    blitRect.src1_rect.x = srcX;
    blitRect.src1_rect.y = srcY;
    blitRect.src1_rect.w = srcWidth;
    blitRect.src1_rect.h = srcHeight;

    blitRect.dst_rect.x = dstX;
    blitRect.dst_rect.y = dstY + voffset;
    blitRect.dst_rect.w = dstWidth;
    blitRect.dst_rect.h = dstHeight;


    io = ioctl(ge2d_fd, GE2D_STRETCHBLIT, &blitRect);
    if (io < 0)
    {
        printf("GE2D_STRETCHBLIT failed.\n");
        abort();
    }
}


static void* RenderThread(void* arg)
{
    gou_display_t* obj = (gou_display_t*)arg;

    int prevFrameBuffer = -1;


    fb_var_screeninfo var_info;
    if (ioctl(obj->fd, FBIOGET_VSCREENINFO, &var_info) < 0)
    {
        printf("FBIOGET_VSCREENINFO failed.\n");
        abort();
    }

    const int buffer_count = var_info.yres_virtual / var_info.yres;
    printf("RenderThread: buffer_count=%d\n", buffer_count);

    int current_buffer = 0;

    obj->terminating = false;
    while(true)
    {
        sem_wait(&obj->usedSem);
        if(obj->terminating) break;


        pthread_mutex_lock(&obj->queueMutex);

        if (obj->usedFrameBuffers->size() < 1)
        {
            pthread_mutex_unlock(&obj->queueMutex);

            printf("no framebuffer available.\n");
            abort();
        }

        int framebuffer = obj->usedFrameBuffers->front();
        obj->usedFrameBuffers->pop();

        pthread_mutex_unlock(&obj->queueMutex);


 

        // Swap buffers
        var_info.yoffset = framebuffer;
#if 0
        if (ioctl(obj->fd, FBIO_WAITFORVSYNC, 0) < 0)
        {
            printf("FBIO_WAITFORVSYNC failed.\n");
            abort();
        }

        if (ioctl(obj->fd, FBIOPAN_DISPLAY, &var_info) < 0)
        {
            printf("FBIOPAN_DISPLAY failed.\n");
            abort();
        }
#else
        if (ioctl(obj->fd, FBIOPUT_VSCREENINFO, &var_info) < 0) 
        {
            printf("FBIOPUT_VSCREENINFO failed.\n");
            abort();
        }
#endif

        ++current_buffer;
        current_buffer %= buffer_count;

        //printf("FbDevDisplay: frame - current_buffer=%d\n", current_buffer);
        
        if (prevFrameBuffer >= 0)
        {
            pthread_mutex_lock(&obj->queueMutex);
            obj->freeFrameBuffers->push(prevFrameBuffer);
            pthread_mutex_unlock(&obj->queueMutex);

            sem_post(&obj->freeSem);
        }

        prevFrameBuffer = framebuffer;            
    }


    return NULL;
}


gou_display_t* gou_display_create()
{
    if (ge2d_fd < 0)
    {
        ge2d_fd = open("/dev/ge2d", O_RDWR);
        if (ge2d_fd < 0)
        {
            printf("open /dev/ge2d failed.\n");
            abort();
        }
    }

    gou_display_t* result = (gou_display_t*)malloc(sizeof(gou_display_t));
    if (!result)
    {
        printf("malloc failed.\n");
        abort();
    }

    memset(result, 0, sizeof(*result));

    
    result->backgroundColor = (0xff000000);
    result->freeFrameBuffers = new std::queue<int>;
    result->usedFrameBuffers = new std::queue<int>;


    // Open device
    result->fd = open("/dev/fb0", O_RDWR);
    if (result->fd < 0)
    {
        printf("open /dev/fb0 failed.\n");
        abort();
    }

 
    // Properties
    fb_var_screeninfo var_info;
    if (ioctl(result->fd, FBIOGET_VSCREENINFO, &var_info) < 0)
    {
        printf("FBIOGET_VSCREENINFO failed.\n");
        abort();
    }

    result->width = var_info.xres;
    result->height = var_info.yres;

    const int BUFFER_COUNT = var_info.yres_virtual / var_info.yres;

    printf("gou_display_create: w=%d, h=%d, vx=%d, vy=%d (buffers=%d)\n",
        var_info.xres, var_info.yres, var_info.xres_virtual, var_info.yres_virtual, BUFFER_COUNT);


    // buffers
    for (int i = 0; i < BUFFER_COUNT; ++i)
    {
        //c4_surface_t* surface = c4_surface_create(result, result->width, result->height, DRM_FORMAT_XRGB8888);
        int framebuffer = i * var_info.yres;

        result->freeFrameBuffers->push(framebuffer);
    }


    sem_init(&result->usedSem, 0, 0);
    sem_init(&result->freeSem, 0, BUFFER_COUNT);

    pthread_mutex_init(&result->queueMutex, NULL);

    pthread_create(&result->renderThread, NULL, RenderThread, result);

    return result;
}

void gou_display_destroy(gou_display_t* display)
{
    display->terminating = true;
    sem_post(&display->usedSem);

    pthread_join(display->renderThread, NULL);

    close(display->fd);

    delete display->freeFrameBuffers;
    delete display->usedFrameBuffers;

    free(display);
}

int gou_display_width_get(gou_display_t* display)
{
    // Display is rotated
    return display->height;
}

int gou_display_height_get(gou_display_t* display)
{
    // Display is rotated
    return display->width;
}

void gou_display_present(gou_display_t* display, gou_surface_t* surface,
            int srcX, int srcY, int srcWidth, int srcHeight, bool mirrorX, bool mirrorY,
            int dstX, int dstY, int dstWidth, int dstHeight)
{
    sem_wait(&display->freeSem);


    pthread_mutex_lock(&display->queueMutex);

    if (display->freeFrameBuffers->size() < 1)
    {
        printf("no framebuffer available.\n");
        abort();
    }

    int dstFrameBuffer = display->freeFrameBuffers->front();
    display->freeFrameBuffers->pop();

    pthread_mutex_unlock(&display->queueMutex);


    fb_var_screeninfo var_info;
    if (ioctl(display->fd, FBIOGET_VSCREENINFO, &var_info) < 0)
    {
        printf("FBIOGET_VSCREENINFO failed.\n");
        abort();
    }

    //printf("yres_virtual=%d, offset=%d\n", var_info.yres_virtual, dstFrameBuffer.offset);

    if (dstX != 0 || dstY != 0 ||
        dstWidth != display->width || dstHeight != display->height)
    {
        ClearScreen(display->backgroundColor, var_info.xres, var_info.yres, var_info.xres_virtual, var_info.yres_virtual, dstFrameBuffer);
    }

    // Blit(surface, srcX, srcY, srcWidth, srcHeight, mirrorX, mirrorY,
    //     dstX, dstY, dstWidth, dstHeight,
    //     var_info.xres_virtual, var_info.yres_virtual, dstFrameBuffer, GOU_ROTATION_DEGREES_270);

    // Blit(surface, srcX, srcY, srcWidth, srcHeight, mirrorX, mirrorY,
    //     dstY, dstX, dstHeight, dstWidth,
    //     var_info.xres_virtual, var_info.yres_virtual, dstFrameBuffer, GOU_ROTATION_DEGREES_270);

    Blit(surface, srcX, srcY, srcWidth, srcHeight, mirrorX, mirrorY,
        dstY, display->height - (dstX + dstWidth), dstWidth, dstHeight,
        var_info.xres_virtual, var_info.yres_virtual, dstFrameBuffer, GOU_ROTATION_DEGREES_270);



    pthread_mutex_lock(&display->queueMutex);
    display->usedFrameBuffers->push(dstFrameBuffer);
    pthread_mutex_unlock(&display->queueMutex);

    sem_post(&display->usedSem);       
}

uint32_t gou_display_background_color_get(gou_display_t* display)
{
    return display->backgroundColor;
}

void gou_display_background_color_set(gou_display_t* display, uint32_t value)
{
    display->backgroundColor = value;
}

