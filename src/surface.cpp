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

#include "surface.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include <drm/drm_fourcc.h>

#include "ion.h"


#define ALIGN(val, align)	(((val) + (align) - 1) & ~((align) - 1))


typedef struct gou_surface
{
    gou_display_t* display;
    int width;
    int height;
    uint32_t format;
    int stride;
    int size;
    uint32_t ion_handle;;
    int share_fd;
    void* map;
} go2_surface_t;


static int ion_fd = -1;


gou_surface_t* gou_surface_create(gou_display_t* display, int width, int height, uint32_t format)
{
    if (ion_fd < 0)
    {
        ion_fd = open("/dev/ion", O_RDWR);
        if (ion_fd < 0)
        {
            printf("open /dev/ion failed.\n");
            abort();
        }
    }

    // Allocate a buffer
    int stride = ALIGN(width * (gou_drm_format_get_bpp(format) / 8), 64);
    int size = height * stride;

    ion_allocation_data allocation_data = { 0 };
    allocation_data.len = size;
    allocation_data.heap_id_mask = (1 << ION_HEAP_TYPE_DMA);
    allocation_data.flags = 0;

    int io = ioctl(ion_fd, ION_IOC_ALLOC, &allocation_data);
    if (io != 0)
    {
        printf("ION_IOC_ALLOC failed.\n");
        abort();
    }
    
    go2_surface_t* result = (go2_surface_t*)malloc(sizeof(go2_surface_t));
    if (!result)
    {
        printf("malloc failed.\n");
        abort();
    }

    memset(result, 0, sizeof(*result));

    result->display = display;
    result->width = width;
    result->height = height;
    result->format = format;
    result->size = size;
    result->stride = stride;
    result->ion_handle = allocation_data.handle;
    result->share_fd = -1;
    result->map = MAP_FAILED;

    return result;
}

void gou_surface_destroy(gou_surface_t* surface)
{
    if (surface->share_fd >= 0) close(surface->share_fd);

    if (surface->map) munmap(surface->map, surface->size);

    ion_handle_data ionHandleData = { 0 };
    ionHandleData.handle = surface->ion_handle;

    int io = ioctl(ion_fd, ION_IOC_FREE, &ionHandleData);
    if (io != 0)
    {
        printf("ION_IOC_FREE failed.\n");
        abort();
    }

    free(surface);
}

int gou_surface_width_get(gou_surface_t* surface)
{
    return surface->width;
}

int gou_surface_height_get(gou_surface_t* surface)
{
    return surface->height;
}

uint32_t gou_surface_format_get(gou_surface_t* surface)
{
    return surface->format;
}

int gou_surface_stride_get(gou_surface_t* surface)
{
    return surface->stride;
}

gou_display_t* gou_surface_display_get(gou_surface_t* surface)
{
    return surface->display;
}

int gou_surface_share_fd(gou_surface_t* surface)
{
    if (surface->share_fd <= 0)
    {
        ion_fd_data ionData = { 0 };
        ionData.handle = surface->ion_handle;

        int io = ioctl(ion_fd, ION_IOC_SHARE, &ionData);
        if (io != 0)
        {
            printf("ION_IOC_SHARE failed.\n");
            abort();
        }

        surface->share_fd = ionData.fd;
    }

    return surface->share_fd;
}

void* gou_surface_map(gou_surface_t* surface)
{
   if (surface->map == MAP_FAILED)
   {
        int share_fd = gou_surface_share_fd(surface);
        surface->map = mmap(NULL, surface->size, PROT_READ | PROT_WRITE, MAP_SHARED, share_fd, 0);
        if (surface->map == MAP_FAILED)
        {
            printf("mmap failed.\n");
            abort();
        }
   }

   return surface->map;
}

void gou_surface_unmap(gou_surface_t* surface)
{
    if (surface->map != MAP_FAILED)
    {
        munmap(surface->map, surface->size);
        surface->map = MAP_FAILED;
    }
}



int gou_drm_format_get_bpp(uint32_t format)
{
    int result;

    switch(format)
    {
        case DRM_FORMAT_XRGB4444:
        case DRM_FORMAT_XBGR4444:
        case DRM_FORMAT_RGBX4444:
        case DRM_FORMAT_BGRX4444:

        case DRM_FORMAT_ARGB4444:
        case DRM_FORMAT_ABGR4444:
        case DRM_FORMAT_RGBA4444:
        case DRM_FORMAT_BGRA4444:

        case DRM_FORMAT_XRGB1555:
        case DRM_FORMAT_XBGR1555:
        case DRM_FORMAT_RGBX5551:
        case DRM_FORMAT_BGRX5551:

        case DRM_FORMAT_ARGB1555:
        case DRM_FORMAT_ABGR1555:
        case DRM_FORMAT_RGBA5551:
        case DRM_FORMAT_BGRA5551:

        case DRM_FORMAT_RGB565:
        case DRM_FORMAT_BGR565:
            result = 16;
            break;
            

        case DRM_FORMAT_RGB888:
        case DRM_FORMAT_BGR888:
            result = 24;
            break;


        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_XBGR8888:
        case DRM_FORMAT_RGBX8888:
        case DRM_FORMAT_BGRX8888:

        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_RGBA8888:
        case DRM_FORMAT_BGRA8888:

        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_XBGR2101010:
        case DRM_FORMAT_RGBX1010102:
        case DRM_FORMAT_BGRX1010102:

        case DRM_FORMAT_ARGB2101010:
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_RGBA1010102:
        case DRM_FORMAT_BGRA1010102:
            result = 32;
            break;


        default:
            printf("unhandled DRM FORMAT.\n");
            result = 0;
    }

    return result;
}
