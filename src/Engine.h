#pragma once
#include "Settings.h"
#include <memory>
namespace Engine
{
    bool Initialize();
    void PublishSettings(std::shared_ptr<const UI::Settings> settings);
}
