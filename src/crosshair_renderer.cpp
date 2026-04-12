#include "crosshair_renderer.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

using Gdiplus::Color;
using Gdiplus::Graphics;
using Gdiplus::GraphicsState;
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

void DrawCenterDot(Graphics& graphics, const float radius, const Color& color, const BYTE alpha, const bool outlineEnabled)
{
    if (outlineEnabled)
    {
        SolidBrush outlineBrush(Color(alpha, 0, 0, 0));
        const RectF outlineRect(-radius - 1.5F, -radius - 1.5F, (radius + 1.5F) * 2.0F, (radius + 1.5F) * 2.0F);
        graphics.FillEllipse(&outlineBrush, outlineRect);
    }

    SolidBrush fillBrush(color);
    const RectF fillRect(-radius, -radius, radius * 2.0F, radius * 2.0F);
    graphics.FillEllipse(&fillBrush, fillRect);
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
    const bool outlineEnabled = settings.outlineEnabled;

    Pen outlinePen(Color(alpha, 0, 0, 0), thickness + 2.0F);
    outlinePen.SetStartCap(LineCapRound);
    outlinePen.SetEndCap(LineCapRound);

    Pen mainPen(MakeColor(effectiveColor, alpha), thickness);
    mainPen.SetStartCap(LineCapRound);
    mainPen.SetEndCap(LineCapRound);

    const GraphicsState state = graphics.Save();
    graphics.TranslateTransform(centerX, centerY);
    graphics.RotateTransform(static_cast<Gdiplus::REAL>(ClampInt(settings.rotation, 0, 359)));

    auto drawLine = [&](const float x1, const float y1, const float x2, const float y2)
    {
        if (outlineEnabled)
        {
            graphics.DrawLine(&outlinePen, x1, y1, x2, y2);
        }
        graphics.DrawLine(&mainPen, x1, y1, x2, y2);
    };

    switch (settings.style)
    {
    case CrosshairStyle::Cross:
        drawLine(-gap, 0.0F, -gap - armLength, 0.0F);
        drawLine(gap, 0.0F, gap + armLength, 0.0F);
        drawLine(0.0F, -gap, 0.0F, -gap - armLength);
        drawLine(0.0F, gap, 0.0F, gap + armLength);
        break;

    case CrosshairStyle::Dot:
        DrawCenterDot(graphics, dotRadius, MakeColor(effectiveColor, alpha), alpha, outlineEnabled);
        break;

    case CrosshairStyle::Circle:
        if (outlineEnabled)
        {
            graphics.DrawEllipse(&outlinePen, -circleRadius, -circleRadius, circleRadius * 2.0F, circleRadius * 2.0F);
        }
        graphics.DrawEllipse(&mainPen, -circleRadius, -circleRadius, circleRadius * 2.0F, circleRadius * 2.0F);
        break;

    case CrosshairStyle::TShape:
        drawLine(-gap, 0.0F, -gap - armLength, 0.0F);
        drawLine(gap, 0.0F, gap + armLength, 0.0F);
        drawLine(0.0F, gap, 0.0F, gap + armLength);
        break;
    }

    if (settings.middleDot && settings.style != CrosshairStyle::Dot)
    {
        DrawCenterDot(graphics, dotRadius, MakeColor(effectiveColor, alpha), alpha, outlineEnabled);
    }

    graphics.Restore(state);
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

    const float outlinePadding = settings.outlineEnabled ? 2.0F : 0.0F;
    const float armExtent = static_cast<float>(settings.gap + settings.length) + static_cast<float>(settings.thickness) + outlinePadding;
    const float circleExtent = static_cast<float>(settings.length) + static_cast<float>(settings.thickness) + outlinePadding + 10.0F;
    const float dotExtent = static_cast<float>(settings.thickness * 3) + outlinePadding + 8.0F;
    const float centerDotExtent = settings.middleDot ? dotExtent : 0.0F;
    const float maxExtent = std::max({armExtent, circleExtent, dotExtent, centerDotExtent});

    float rotationScale = 1.0F;
    if (settings.style == CrosshairStyle::Cross || settings.style == CrosshairStyle::TShape)
    {
        const double radians = static_cast<double>(ClampInt(settings.rotation, 0, 359)) * 3.14159265358979323846 / 180.0;
        rotationScale = static_cast<float>(std::abs(std::cos(radians)) + std::abs(std::sin(radians)));
    }

    return std::max(96, static_cast<int>(std::ceil((maxExtent * 2.0F * rotationScale) + 24.0F)));
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
