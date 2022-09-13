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


#include <stdint.h>


typedef struct gou_display gou_display_t;
typedef struct gou_surface gou_surface_t;


typedef enum gou_rotation
{
    GOU_ROTATION_DEGREES_0 = 0,
    GOU_ROTATION_DEGREES_90,
    GOU_ROTATION_DEGREES_180,
    GOU_ROTATION_DEGREES_270
} gou_rotation_t;


#ifdef __cplusplus
extern "C" {
#endif

gou_display_t* gou_display_create();
void gou_display_destroy(gou_display_t* display);
int gou_display_width_get(gou_display_t* display);
int gou_display_height_get(gou_display_t* display);
void gou_display_present(gou_display_t* display, gou_surface_t* surface,
            int srcX, int srcY, int srcWidth, int srcHeight, bool mirrorX, bool mirrorY,
            int dstX, int dstY, int dstWidth, int dstHeight);
uint32_t gou_display_background_color_get(gou_display_t* display);
void gou_display_background_color_set(gou_display_t* display, uint32_t value);


#ifdef __cplusplus
}
#endif
