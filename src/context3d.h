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

#pragma once

#include "surface.h"


typedef struct gou_context3d gou_context3d_t;

typedef struct gou_context3d_attributes
{
    int major;
    int minor;
    int red_bits;
    int green_bits;
    int blue_bits;
    int alpha_bits;
    int depth_bits;
    int stencil_bits;
} gou_context3d_attributes_t;


#ifdef __cplusplus
extern "C" {
#endif

gou_context3d_t* gou_context3d_create(gou_display_t* display, int width, int height, const gou_context3d_attributes_t* attributes);
void gou_context3d_destroy(gou_context3d_t* context);
void* gou_context3d_egldisplay_get(gou_context3d_t* context);
void* gou_context3d_eglcontext_get(gou_context3d_t* context);
int gou_context3d_fbo_get(gou_context3d_t* context);
void gou_context3d_make_current(gou_context3d_t* context);
void gou_context3d_swap_buffers(gou_context3d_t* context);
gou_surface_t* gou_context3d_surface_lock(gou_context3d_t* context);
void gou_context3d_surface_unlock(gou_context3d_t* context, gou_surface_t* surface);


#ifdef __cplusplus
}
#endif
