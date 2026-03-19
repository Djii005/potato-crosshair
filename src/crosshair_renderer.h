#pragma once

#include "app_types.h"

#include <objidl.h>
#include <gdiplus.h>

namespace potato
{
COLORREF GetCrosshairColor(const Settings& settings);
int CalculateOverlayWindowSize(const Settings& settings);
void DrawCrosshair(Gdiplus::Graphics& graphics, const Settings& settings, const Gdiplus::RectF& bounds);
} // namespace potato
