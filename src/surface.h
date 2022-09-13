#pragma once

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

#include "display.h"

#include <stdint.h>


typedef struct gou_surface gou_surface_t;


#ifdef __cplusplus
extern "C" {
#endif

gou_surface_t* gou_surface_create(gou_display_t* display, int width, int height, uint32_t format);
void gou_surface_destroy(gou_surface_t* surface);
int gou_surface_width_get(gou_surface_t* surface);
int gou_surface_height_get(gou_surface_t* surface);
uint32_t gou_surface_format_get(gou_surface_t* surface);
int gou_surface_stride_get(gou_surface_t* surface);
gou_display_t* gou_surface_display_get(gou_surface_t* surface);
int gou_surface_share_fd(gou_surface_t* surface);
void* gou_surface_map(gou_surface_t* surface);
void gou_surface_unmap(gou_surface_t* surface);
// void gou_surface_blit(gou_surface_t* srcSurface, int srcX, int srcY, int srcWidth, int srcHeight,
//                       gou_surface_t* dstSurface, int dstX, int dstY, int dstWidth, int dstHeight,
//                       gou_rotation_t rotation);
// int gou_surface_save_as_png(gou_surface_t* surface, const char* filename);

int gou_drm_format_get_bpp(uint32_t format);


#ifdef __cplusplus
}
#endif
