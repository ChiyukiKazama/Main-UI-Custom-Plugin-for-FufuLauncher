#pragma once
#include "Settings.h"
#include <memory>
namespace Native { struct Profile; }
namespace HUD
{
    void Run(const Native::Profile& profile);
    void Publish(std::shared_ptr<const UI::Settings> settings);
}
