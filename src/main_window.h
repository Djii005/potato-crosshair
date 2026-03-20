#pragma once

#include "app_types.h"

#include <array>
#include <vector>

namespace potato
{
class AppController;

class MainWindow
{
public:
    explicit MainWindow(AppController* app);
    ~MainWindow();

    bool Create(HINSTANCE instance);
    void Destroy();
    void Show(int showCommand);
    void ShowAndFocus();
    void HideToTray();
    void ApplySettings(const Settings& settings);

    HWND hwnd() const;

private:
    struct SectionControl
    {
        HWND handle = nullptr;
        AppSection section = AppSection::General;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK PreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateFonts();
    void CreateControls();
    void LayoutChildren(int width, int height);
    void RegisterSectionControl(HWND handle, AppSection section);
    void SetActiveSection(AppSection section);
    void UpdateSectionVisibility();
    void UpdateSliderLabels(const Settings& settings);
    void UpdateImageStatus(const Settings& settings);
    void UpdateOwnerDrawControls();
    void PaintWindow();
    void PaintPreview(HWND preview) const;
    void DrawOwnerButton(const DRAWITEMSTRUCT& drawItem) const;
    void HandleCommand(WPARAM wParam, LPARAM lParam);
    void HandleScroll(WPARAM wParam, LPARAM lParam);
    void HandleTrayMessage(LPARAM lParam);
    void OpenColorDialog();
    void OpenImportDialog();
    std::wstring OpenPngFileDialog() const;
    Settings CopySettings() const;
    void ApplyWindowFrame();
    void UpdateWindowRegion(int width, int height);
    void CenterWindow();
    bool IsHeaderPoint(POINT point) const;
    bool IsPointInControl(HWND control, POINT point) const;
    void HideInsteadOfClose();

    AppController* app_ = nullptr;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    AppSection activeSection_ = AppSection::General;
    bool hasInitialPlacement_ = false;
    bool updatingControls_ = false;

    HFONT titleFont_ = nullptr;
    HFONT bodyFont_ = nullptr;
    HFONT smallFont_ = nullptr;
    HFONT monoFont_ = nullptr;

    HBRUSH windowBrush_ = nullptr;
    HBRUSH panelBrush_ = nullptr;

    std::vector<SectionControl> sectionControls_;
    std::array<HWND, 5> sectionButtons_{};
    std::array<HWND, kColorPresets.size()> swatchButtons_{};

    HWND headerMinimizeButton_ = nullptr;
    HWND headerCloseButton_ = nullptr;

    HWND generalVisibleCheck_ = nullptr;
    HWND generalSummaryLabel_ = nullptr;
    HWND generalStorageLabel_ = nullptr;

    HWND previewControl_ = nullptr;
    HWND styleCombo_ = nullptr;
    HWND lengthSlider_ = nullptr;
    HWND lengthValue_ = nullptr;
    HWND gapSlider_ = nullptr;
    HWND gapValue_ = nullptr;
    HWND thicknessSlider_ = nullptr;
    HWND thicknessValue_ = nullptr;
    HWND opacitySlider_ = nullptr;
    HWND opacityValue_ = nullptr;
    HWND useCustomColorCheck_ = nullptr;
    HWND pickColorButton_ = nullptr;
    HWND customColorSwatch_ = nullptr;

    HWND enableCustomImageCheck_ = nullptr;
    HWND importImageButton_ = nullptr;
    HWND clearImageButton_ = nullptr;
    HWND imageStatusLabel_ = nullptr;
    HWND imageSizeSlider_ = nullptr;
    HWND imageSizeValue_ = nullptr;

    HWND hotkeysText_ = nullptr;
    HWND aboutText_ = nullptr;
};
} // namespace potato
