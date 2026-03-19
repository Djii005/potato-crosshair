#include "crosshair_renderer.h"

#include <algorithm>
#include <filesystem>

using Gdiplus::Color;
using Gdiplus::Graphics;
using Gdiplus::Image;
using Gdiplus::ImageAttributes;
using Gdiplus::LineCapRound;
using Gdiplus::Pen;
using Gdiplus::RectF;
using Gdiplus::SolidBrush;

namespace potato
{
namespace
{
Color MakeColor(const COLORREF color, const BYTE alpha)
{
    return Color(alpha, GetRValue(color), GetGValue(color), GetBValue(color));
}

bool TryDrawCustomImage(Graphics& graphics, const Settings& settings, const RectF& bounds)
{
    if (settings.renderMode != RenderMode::CustomImage || settings.customImagePath.empty())
    {
        return false;
    }

    std::error_code errorCode;
    if (!std::filesystem::exists(settings.customImagePath, errorCode))
    {
        return false;
    }

    Image image(settings.customImagePath.c_str());
    if (image.GetLastStatus() != Gdiplus::Ok || image.GetWidth() == 0 || image.GetHeight() == 0)
    {
        return false;
    }

    const float imageSize = static_cast<float>(ClampInt(settings.imageSize, 16, 512));
    const float left = bounds.X + (bounds.Width - imageSize) / 2.0F;
    const float top = bounds.Y + (bounds.Height - imageSize) / 2.0F;

    ImageAttributes imageAttributes;
    Gdiplus::ColorMatrix matrix = {{
        {1.0F, 0.0F, 0.0F, 0.0F, 0.0F},
        {0.0F, 1.0F, 0.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 1.0F, 0.0F, 0.0F},
        {0.0F, 0.0F, 0.0F, static_cast<float>(settings.opacity) / 255.0F, 0.0F},
        {0.0F, 0.0F, 0.0F, 0.0F, 1.0F},
    }};
    imageAttributes.SetColorMatrix(&matrix);

    graphics.DrawImage(
        &image,
        Gdiplus::RectF(left, top, imageSize, imageSize),
        0.0F,
        0.0F,
        static_cast<float>(image.GetWidth()),
        static_cast<float>(image.GetHeight()),
        Gdiplus::UnitPixel,
        &imageAttributes);

    return true;
}

void DrawBuiltInCrosshair(Graphics& graphics, const Settings& settings, const RectF& bounds)
{
    const COLORREF effectiveColor = GetCrosshairColor(settings);
    const BYTE alpha = static_cast<BYTE>(ClampInt(settings.opacity, 40, 255));
    const float centerX = bounds.X + bounds.Width / 2.0F;
    const float centerY = bounds.Y + bounds.Height / 2.0F;
    const float thickness = static_cast<float>(ClampInt(settings.thickness, 1, 20));
    const float armLength = static_cast<float>(ClampInt(settings.length, 4, 120));
    const float gap = static_cast<float>(ClampInt(settings.gap, 0, 80));
    const float dotRadius = std::max(2.0F, thickness * 1.15F);
    const float circleRadius = armLength;

    Pen outlinePen(Color(alpha, 0, 0, 0), thickness + 2.0F);
    outlinePen.SetStartCap(LineCapRound);
    outlinePen.SetEndCap(LineCapRound);

    Pen mainPen(MakeColor(effectiveColor, alpha), thickness);
    mainPen.SetStartCap(LineCapRound);
    mainPen.SetEndCap(LineCapRound);

    auto drawLine = [&](const float x1, const float y1, const float x2, const float y2)
    {
        graphics.DrawLine(&outlinePen, x1, y1, x2, y2);
        graphics.DrawLine(&mainPen, x1, y1, x2, y2);
    };

    switch (settings.style)
    {
    case CrosshairStyle::Cross:
        drawLine(centerX - gap, centerY, centerX - gap - armLength, centerY);
        drawLine(centerX + gap, centerY, centerX + gap + armLength, centerY);
        drawLine(centerX, centerY - gap, centerX, centerY - gap - armLength);
        drawLine(centerX, centerY + gap, centerX, centerY + gap + armLength);
        break;

    case CrosshairStyle::Dot:
    {
        SolidBrush outlineBrush(Color(alpha, 0, 0, 0));
        SolidBrush fillBrush(MakeColor(effectiveColor, alpha));
        const RectF outlineRect(centerX - dotRadius - 1.5F, centerY - dotRadius - 1.5F, (dotRadius + 1.5F) * 2.0F, (dotRadius + 1.5F) * 2.0F);
        const RectF fillRect(centerX - dotRadius, centerY - dotRadius, dotRadius * 2.0F, dotRadius * 2.0F);
        graphics.FillEllipse(&outlineBrush, outlineRect);
        graphics.FillEllipse(&fillBrush, fillRect);
        break;
    }

    case CrosshairStyle::Circle:
        graphics.DrawEllipse(&outlinePen, centerX - circleRadius, centerY - circleRadius, circleRadius * 2.0F, circleRadius * 2.0F);
        graphics.DrawEllipse(&mainPen, centerX - circleRadius, centerY - circleRadius, circleRadius * 2.0F, circleRadius * 2.0F);
        break;

    case CrosshairStyle::TShape:
        drawLine(centerX - gap, centerY, centerX - gap - armLength, centerY);
        drawLine(centerX + gap, centerY, centerX + gap + armLength, centerY);
        drawLine(centerX, centerY + gap, centerX, centerY + gap + armLength);
        break;
    }
}
} // namespace

COLORREF GetCrosshairColor(const Settings& settings)
{
    if (settings.useCustomColor)
    {
        return settings.customColor;
    }

    return GetPresetColor(settings.colorIndex);
}

int CalculateOverlayWindowSize(const Settings& settings)
{
    if (settings.renderMode == RenderMode::CustomImage && !settings.customImagePath.empty())
    {
        return std::max(96, ClampInt(settings.imageSize, 16, 512) + 24);
    }

    const int armExtent = settings.gap + settings.length + settings.thickness;
    const int circleExtent = settings.length + settings.thickness + 10;
    const int dotExtent = (settings.thickness * 3) + 8;
    const int maxExtent = std::max({armExtent, circleExtent, dotExtent});
    return std::max(96, (maxExtent * 2) + 24);
}

void DrawCrosshair(Graphics& graphics, const Settings& settings, const RectF& bounds)
{
    if (TryDrawCustomImage(graphics, settings, bounds))
    {
        return;
    }

    DrawBuiltInCrosshair(graphics, settings, bounds);
}
} // namespace potato
