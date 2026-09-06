#include "UI/GGJPauseMenuWidget.h"

#include "UI/GGJPauseMenuManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Styling/SlateTypes.h"

namespace GGJPauseMenuStyle
{
    const FLinearColor Text(0.90f, 0.92f, 0.95f, 0.92f);
    const FLinearColor HoveredText(1.f, 1.f, 1.f, 1.f);
    const FLinearColor PressedText(0.60f, 0.84f, 1.f, 1.f);
    const FLinearColor MutedText(0.62f, 0.68f, 0.76f, 0.90f);
    const FLinearColor Accent(0.62f, 0.84f, 1.f, 1.f);
}

void UGGJPauseMenuWidget::InitializePauseMenu(AGGJPauseMenuManager* InManager)
{
    Manager = InManager;
    PopulateSettings();
}

TSharedRef<SWidget> UGGJPauseMenuWidget::RebuildWidget()
{
    if (!WidgetTree)
    {
        return Super::RebuildWidget();
    }

    // 蓝图子类若已在 UMG Designer 中搭建界面，完整保留其 WidgetTree。
    // 只有原生类没有布局时，才生成下面的默认简约菜单。
    if (WidgetTree->RootWidget)
    {
        return Super::RebuildWidget();
    }

    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),
        TEXT("PauseMenuRoot"));
    WidgetTree->RootWidget = Root;

    UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>(
        UBackgroundBlur::StaticClass(), TEXT("WorldBlur"));
    Blur->SetBlurStrength(18.f);
    Blur->SetBlurRadius(32);
    Blur->SetApplyAlphaToBlur(true);
    UOverlaySlot* BlurSlot = Root->AddChildToOverlay(Blur);
    BlurSlot->SetHorizontalAlignment(HAlign_Fill);
    BlurSlot->SetVerticalAlignment(VAlign_Fill);

    UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
        TEXT("BackgroundShade"));
    Shade->SetBrushColor(FLinearColor(0.f, 0.005f, 0.015f, 0.24f));
    UOverlaySlot* ShadeSlot = Root->AddChildToOverlay(Shade);
    ShadeSlot->SetHorizontalAlignment(HAlign_Fill);
    ShadeSlot->SetVerticalAlignment(VAlign_Fill);

    USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
        TEXT("PanelSize"));
    PanelSize->SetWidthOverride(520.f);
    UOverlaySlot* PanelSizeSlot = Root->AddChildToOverlay(PanelSize);
    PanelSizeSlot->SetHorizontalAlignment(HAlign_Left);
    PanelSizeSlot->SetVerticalAlignment(VAlign_Bottom);
    PanelSizeSlot->SetPadding(FMargin(72.f, 0.f, 0.f, 64.f));

    UBorder* PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
        TEXT("PanelBorder"));
    // 透明容器只负责布局，不绘制任何卡片或边框。
    PanelBorder->SetBrushColor(FLinearColor::Transparent);
    PanelBorder->SetPadding(FMargin(0.f));
    PanelSize->AddChild(PanelBorder);

    UOverlay* PanelSwitcher = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),
        TEXT("PanelSwitcher"));
    PanelBorder->AddChild(PanelSwitcher);

    MainPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
        TEXT("MainPanel"));
    PanelSwitcher->AddChildToOverlay(MainPanel);

    MakeLabel(MainPanel, FText::FromString(TEXT("暂停")), 46);
    UTextBlock* Subtitle = MakeLabel(MainPanel,
        FText::FromString(TEXT("PAUSED  /  ESC TO RESUME")), 12);
    Subtitle->SetColorAndOpacity(GGJPauseMenuStyle::Accent);

    UButton* ResumeButton = MakeMenuButton(MainPanel,
        FText::FromString(TEXT("回到游戏")), TEXT("ResumeButton"));
    UButton* SettingsButton = MakeMenuButton(MainPanel,
        FText::FromString(TEXT("设置")), TEXT("SettingsButton"));
    UButton* DeveloperLevelButton = MakeMenuButton(MainPanel,
        FText::FromString(TEXT("开发者关卡")), TEXT("DeveloperLevelButton"));
    UButton* MainMenuButton = MakeMenuButton(MainPanel,
        FText::FromString(TEXT("退出到主菜单")), TEXT("MainMenuButton"));
    UButton* QuitButton = MakeMenuButton(MainPanel,
        FText::FromString(TEXT("退出游戏")), TEXT("QuitButton"));

    ResumeButton->OnClicked.AddDynamic(this, &UGGJPauseMenuWidget::HandleResumeClicked);
    SettingsButton->OnClicked.AddDynamic(this, &UGGJPauseMenuWidget::HandleSettingsClicked);
    DeveloperLevelButton->OnClicked.AddDynamic(this,
        &UGGJPauseMenuWidget::HandleDeveloperLevelClicked);
    MainMenuButton->OnClicked.AddDynamic(this, &UGGJPauseMenuWidget::HandleMainMenuClicked);
    QuitButton->OnClicked.AddDynamic(this, &UGGJPauseMenuWidget::HandleQuitClicked);

    SettingsPanel = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),
        TEXT("SettingsPanel"));
    PanelSwitcher->AddChildToOverlay(SettingsPanel);
    SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);

    MakeLabel(SettingsPanel, FText::FromString(TEXT("设置")), 42);

    UHorizontalBox* VolumeRow = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass(), TEXT("VolumeRow"));
    UVerticalBoxSlot* VolumeRowSlot = SettingsPanel->AddChildToVerticalBox(VolumeRow);
    VolumeRowSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 8.f));

    UTextBlock* VolumeLabel = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("VolumeLabel"));
    VolumeLabel->SetText(FText::FromString(TEXT("主音量")));
    VolumeLabel->SetColorAndOpacity(GGJPauseMenuStyle::Text);
    UHorizontalBoxSlot* VolumeLabelSlot = VolumeRow->AddChildToHorizontalBox(VolumeLabel);
    VolumeLabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    VolumeLabelSlot->SetVerticalAlignment(VAlign_Center);

    VolumeValueText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("VolumeValue"));
    VolumeValueText->SetColorAndOpacity(GGJPauseMenuStyle::MutedText);
    VolumeRow->AddChildToHorizontalBox(VolumeValueText);

    VolumeSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(),
        TEXT("VolumeSlider"));
    VolumeSlider->SetMinValue(0.f);
    VolumeSlider->SetMaxValue(1.f);
    VolumeSlider->SetStepSize(0.01f);
    VolumeSlider->SetSliderBarColor(GGJPauseMenuStyle::MutedText);
    VolumeSlider->SetSliderHandleColor(GGJPauseMenuStyle::Accent);
    VolumeSlider->OnValueChanged.AddDynamic(this, &UGGJPauseMenuWidget::HandleVolumeChanged);
    UVerticalBoxSlot* SliderSlot = SettingsPanel->AddChildToVerticalBox(VolumeSlider);
    SliderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    MakeLabel(SettingsPanel, FText::FromString(TEXT("分辨率")), 14);
    ResolutionCombo = WidgetTree->ConstructWidget<UComboBoxString>(
        UComboBoxString::StaticClass(), TEXT("ResolutionCombo"));
    UVerticalBoxSlot* ResolutionSlot = SettingsPanel->AddChildToVerticalBox(ResolutionCombo);
    ResolutionSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 16.f));

    MakeLabel(SettingsPanel, FText::FromString(TEXT("显示模式")), 14);
    WindowModeCombo = WidgetTree->ConstructWidget<UComboBoxString>(
        UComboBoxString::StaticClass(), TEXT("WindowModeCombo"));
    WindowModeCombo->AddOption(TEXT("全屏"));
    WindowModeCombo->AddOption(TEXT("无边框全屏"));
    WindowModeCombo->AddOption(TEXT("窗口"));
    UVerticalBoxSlot* WindowModeSlot = SettingsPanel->AddChildToVerticalBox(WindowModeCombo);
    WindowModeSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 18.f));

    UButton* ApplyButton = MakeMenuButton(SettingsPanel,
        FText::FromString(TEXT("应用")), TEXT("ApplyButton"));
    UButton* BackButton = MakeMenuButton(SettingsPanel,
        FText::FromString(TEXT("返回")), TEXT("BackButton"));
    ApplyButton->OnClicked.AddDynamic(this, &UGGJPauseMenuWidget::HandleApplySettingsClicked);
    BackButton->OnClicked.AddDynamic(this, &UGGJPauseMenuWidget::HandleSettingsBackClicked);

    PopulateSettings();
    return Super::RebuildWidget();
}

void UGGJPauseMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    PopulateSettings();
}

FReply UGGJPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape && Manager)
    {
        Manager->ResumeGame();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

UTextBlock* UGGJPauseMenuWidget::MakeLabel(UVerticalBox* Parent,
    const FText& Label, int32 FontSize)
{
    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Text->SetText(Label);
    Text->SetColorAndOpacity(GGJPauseMenuStyle::Text);
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = FontSize;
    Text->SetFont(Font);
    UVerticalBoxSlot* LabelSlot = Parent->AddChildToVerticalBox(Text);
    LabelSlot->SetPadding(FMargin(0.f, 2.f));
    return Text;
}

UButton* UGGJPauseMenuWidget::MakeMenuButton(UVerticalBox* Parent,
    const FText& Label, FName Name)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);

    // 三种状态都不绘制按钮底图，只让文字颜色和按下位移产生反馈。
    FSlateBrush InvisibleBrush;
    InvisibleBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
    FButtonStyle ButtonStyle = Button->GetStyle();
    ButtonStyle
        .SetNormal(InvisibleBrush)
        .SetHovered(InvisibleBrush)
        .SetPressed(InvisibleBrush)
        .SetDisabled(InvisibleBrush)
        .SetNormalForeground(FSlateColor(GGJPauseMenuStyle::Text))
        .SetHoveredForeground(FSlateColor(GGJPauseMenuStyle::HoveredText))
        .SetPressedForeground(FSlateColor(GGJPauseMenuStyle::PressedText))
        .SetNormalPadding(FMargin(0.f, 3.f))
        .SetPressedPadding(FMargin(7.f, 3.f, 0.f, 3.f));
    Button->SetStyle(ButtonStyle);

    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Text->SetText(Label);
    Text->SetJustification(ETextJustify::Left);
    Text->SetColorAndOpacity(FSlateColor::UseForeground());
    Text->SetShadowOffset(FVector2D(0.f, 1.f));
    Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
    FSlateFontInfo Font = Text->GetFont();
    Font.Size = 27;
    Text->SetFont(Font);
    Button->AddChild(Text);
    if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Text->Slot))
    {
        ContentSlot->SetHorizontalAlignment(HAlign_Left);
        ContentSlot->SetVerticalAlignment(VAlign_Center);
    }

    UVerticalBoxSlot* ButtonSlot = Parent->AddChildToVerticalBox(Button);
    ButtonSlot->SetPadding(FMargin(0.f, 2.f));
    ButtonSlot->SetHorizontalAlignment(HAlign_Left);
    return Button;
}

void UGGJPauseMenuWidget::PopulateSettings()
{
    if (!Manager || !VolumeSlider || !VolumeValueText
        || !ResolutionCombo || !WindowModeCombo)
    {
        return;
    }

    const float Volume = Manager->GetMasterVolume();
    VolumeSlider->SetValue(Volume);
    VolumeValueText->SetText(FText::FromString(
        FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Volume * 100.f))));

    ResolutionCombo->ClearOptions();
    const FIntPoint Current = Manager->GetCurrentResolution();
    const FString CurrentText = FString::Printf(TEXT("%dx%d"), Current.X, Current.Y);
    ResolutionCombo->AddOption(CurrentText);
    const TArray<FIntPoint> CommonResolutions = {
        FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080),
        FIntPoint(2560, 1440), FIntPoint(3840, 2160)
    };
    for (const FIntPoint& Resolution : CommonResolutions)
    {
        const FString Option = FString::Printf(TEXT("%dx%d"), Resolution.X, Resolution.Y);
        if (Option != CurrentText)
        {
            ResolutionCombo->AddOption(Option);
        }
    }
    ResolutionCombo->SetSelectedOption(CurrentText);

    switch (Manager->GetCurrentWindowMode())
    {
    case EGGJWindowMode::Fullscreen:
        WindowModeCombo->SetSelectedOption(TEXT("全屏"));
        break;
    case EGGJWindowMode::WindowedFullscreen:
        WindowModeCombo->SetSelectedOption(TEXT("无边框全屏"));
        break;
    default:
        WindowModeCombo->SetSelectedOption(TEXT("窗口"));
        break;
    }
}

void UGGJPauseMenuWidget::HandleResumeClicked()
{
    if (Manager) { Manager->ResumeGame(); }
}

void UGGJPauseMenuWidget::HandleSettingsClicked()
{
    PopulateSettings();
    if (MainPanel) { MainPanel->SetVisibility(ESlateVisibility::Collapsed); }
    if (SettingsPanel) { SettingsPanel->SetVisibility(ESlateVisibility::Visible); }
}

void UGGJPauseMenuWidget::HandleMainMenuClicked()
{
    if (Manager) { Manager->ReturnToMainMenu(); }
}

void UGGJPauseMenuWidget::HandleDeveloperLevelClicked()
{
    if (Manager) { Manager->OpenDeveloperLevel(); }
}

void UGGJPauseMenuWidget::HandleQuitClicked()
{
    if (Manager) { Manager->QuitGame(); }
}

void UGGJPauseMenuWidget::HandleSettingsBackClicked()
{
    if (SettingsPanel) { SettingsPanel->SetVisibility(ESlateVisibility::Collapsed); }
    if (MainPanel) { MainPanel->SetVisibility(ESlateVisibility::Visible); }
}

void UGGJPauseMenuWidget::HandleApplySettingsClicked()
{
    if (!Manager || !ResolutionCombo || !WindowModeCombo || !VolumeSlider)
    {
        return;
    }

    FIntPoint Resolution = Manager->GetCurrentResolution();
    TArray<FString> Parts;
    ResolutionCombo->GetSelectedOption().ParseIntoArray(Parts, TEXT("x"), true);
    if (Parts.Num() == 2)
    {
        Resolution.X = FCString::Atoi(*Parts[0]);
        Resolution.Y = FCString::Atoi(*Parts[1]);
    }

    EGGJWindowMode WindowMode = EGGJWindowMode::Windowed;
    const FString SelectedMode = WindowModeCombo->GetSelectedOption();
    if (SelectedMode == TEXT("全屏"))
    {
        WindowMode = EGGJWindowMode::Fullscreen;
    }
    else if (SelectedMode == TEXT("无边框全屏"))
    {
        WindowMode = EGGJWindowMode::WindowedFullscreen;
    }
    Manager->ApplyUserSettings(VolumeSlider->GetValue(), Resolution, WindowMode);
    PopulateSettings();
}

void UGGJPauseMenuWidget::HandleVolumeChanged(float Value)
{
    if (VolumeValueText)
    {
        VolumeValueText->SetText(FText::FromString(
            FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Value * 100.f))));
    }
    if (Manager)
    {
        Manager->PreviewMasterVolume(Value);
    }
}
