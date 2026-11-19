#pragma once
#include <Windows.h>
#include <Xinput.h>

namespace HUD
{
    inline constexpr bool IsGamepadKey(int key)
    {
        return key >= VK_GAMEPAD_A && key <= VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT;
    }

    inline bool GamepadKeyDown(int key, const XINPUT_GAMEPAD& pad)
    {
        constexpr int stickThreshold = 16384;
        switch (key)
        {
        case VK_GAMEPAD_A: return (pad.wButtons & XINPUT_GAMEPAD_A) != 0;
        case VK_GAMEPAD_B: return (pad.wButtons & XINPUT_GAMEPAD_B) != 0;
        case VK_GAMEPAD_X: return (pad.wButtons & XINPUT_GAMEPAD_X) != 0;
        case VK_GAMEPAD_Y: return (pad.wButtons & XINPUT_GAMEPAD_Y) != 0;
        case VK_GAMEPAD_RIGHT_SHOULDER: return (pad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
        case VK_GAMEPAD_LEFT_SHOULDER: return (pad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
        case VK_GAMEPAD_LEFT_TRIGGER: return pad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        case VK_GAMEPAD_RIGHT_TRIGGER: return pad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
        case VK_GAMEPAD_DPAD_UP: return (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
        case VK_GAMEPAD_DPAD_DOWN: return (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
        case VK_GAMEPAD_DPAD_LEFT: return (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
        case VK_GAMEPAD_DPAD_RIGHT: return (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
        case VK_GAMEPAD_MENU: return (pad.wButtons & XINPUT_GAMEPAD_START) != 0;
        case VK_GAMEPAD_VIEW: return (pad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
        case VK_GAMEPAD_LEFT_THUMBSTICK_BUTTON: return (pad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
        case VK_GAMEPAD_RIGHT_THUMBSTICK_BUTTON: return (pad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
        case VK_GAMEPAD_LEFT_THUMBSTICK_UP: return pad.sThumbLY > stickThreshold;
        case VK_GAMEPAD_LEFT_THUMBSTICK_DOWN: return pad.sThumbLY < -stickThreshold;
        case VK_GAMEPAD_LEFT_THUMBSTICK_RIGHT: return pad.sThumbLX > stickThreshold;
        case VK_GAMEPAD_LEFT_THUMBSTICK_LEFT: return pad.sThumbLX < -stickThreshold;
        case VK_GAMEPAD_RIGHT_THUMBSTICK_UP: return pad.sThumbRY > stickThreshold;
        case VK_GAMEPAD_RIGHT_THUMBSTICK_DOWN: return pad.sThumbRY < -stickThreshold;
        case VK_GAMEPAD_RIGHT_THUMBSTICK_RIGHT: return pad.sThumbRX > stickThreshold;
        case VK_GAMEPAD_RIGHT_THUMBSTICK_LEFT: return pad.sThumbRX < -stickThreshold;
        default: return false;
        }
    }
}
