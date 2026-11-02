#pragma once
#include "Settings.h"
#include <memory>
namespace HUD
{
    void Run();
    void Publish(std::shared_ptr<const UI::Settings> settings);
}
