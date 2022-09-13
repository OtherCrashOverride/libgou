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

#include <stdint.h>


typedef struct 
{
    float x;
    float y;
} gou_thumb_t;

typedef enum 
{
    ButtonState_Released = 0,
    ButtonState_Pressed
} gou_button_state_t;


typedef struct 
{
    gou_button_state_t a;
    gou_button_state_t b;
    gou_button_state_t x;
    gou_button_state_t y;

    gou_button_state_t top_left;
    gou_button_state_t top_right;

    gou_button_state_t f1;
    gou_button_state_t f2;
    gou_button_state_t f3;
    gou_button_state_t f4;
    gou_button_state_t f5;
    gou_button_state_t f6;

} gou_gamepad_buttons_t;

typedef struct 
{
    gou_button_state_t up;
    gou_button_state_t down;
    gou_button_state_t left;
    gou_button_state_t right;
} gou_dpad_t;

typedef struct 
{
    gou_thumb_t thumb;
    gou_dpad_t dpad;
    gou_gamepad_buttons_t buttons;
} gou_gamepad_state_t;

typedef struct gou_input gou_input_t;


typedef enum 
{
    Battery_Status_Unknown = 0,
    Battery_Status_Discharging,
    Battery_Status_Charging,
    Battery_Status_Full,

    Battery_Status_MAX = 0x7fffffff
} gou_battery_status_t;

typedef struct
{
    uint32_t level;
    gou_battery_status_t status;
} gou_battery_state_t;



// v1.1 API
typedef enum
{
    Go2InputFeatureFlags_None = (1 << 0),
    Go2InputFeatureFlags_Triggers = (1 << 1),
    Go2InputFeatureFlags_RightAnalog = (1 << 2),
} gou_input_feature_flags_t;

typedef enum
{
    Go2InputThumbstick_Left = 0,
    Go2InputThumbstick_Right
} gou_input_thumbstick_t;

typedef enum
{
    Go2InputButton_DPadUp = 0,
    Go2InputButton_DPadDown,
    Go2InputButton_DPadLeft,
    Go2InputButton_DPadRight,

    Go2InputButton_A,
    Go2InputButton_B,
    Go2InputButton_X,
    Go2InputButton_Y,

    Go2InputButton_F1,
    Go2InputButton_F2,
    Go2InputButton_F3,
    Go2InputButton_F4,
    Go2InputButton_F5,
    Go2InputButton_F6,

    Go2InputButton_TopLeft,
    Go2InputButton_TopRight,

    Go2InputButton_TriggerLeft,
    Go2InputButton_TriggerRight
} gou_input_button_t;

typedef struct gou_input_state gou_input_state_t;


#ifdef __cplusplus
extern "C" {
#endif

gou_input_t* gou_input_create();
void gou_input_destroy(gou_input_t* input);
void gou_input_gamepad_read(gou_input_t* input, gou_gamepad_state_t* outGamepadState);
void gou_input_battery_read(gou_input_t* input, gou_battery_state_t* outBatteryState);


// v1.1 API
void gou_input_state_read(gou_input_t* input, gou_input_state_t* outState);


gou_input_state_t* gou_input_state_create();
void gou_input_state_destroy(gou_input_state_t* state);
gou_button_state_t gou_input_state_button_get(gou_input_state_t* state, gou_input_button_t button);
void gou_input_state_button_set(gou_input_state_t* state, gou_input_button_t button, gou_button_state_t value);
gou_thumb_t gou_input_state_thumbstick_get(gou_input_state_t* state, gou_input_thumbstick_t thumbstick);

#ifdef __cplusplus
}
#endif
