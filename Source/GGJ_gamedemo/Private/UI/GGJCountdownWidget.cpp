#include "UI/GGJCountdownWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

TSharedRef<SWidget> UGGJCountdownWidget::RebuildWidget()
{
    // 直接使用原生类时自动创建一个简洁的左上角 UI。
    // 如果蓝图子类已经在 Designer 中搭好控件，则保留蓝图的布局。
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), TEXT("CountdownRoot"));
        WidgetTree->RootWidget = RootCanvas;

        UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), TEXT("CountdownBackground"));
        CountdownBackground = Background;
        CountdownContainer = Background;
        Background->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.86f));
        Background->SetPadding(FMargin(18.f, 10.f));

        CountdownText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("CountdownText"));
        CountdownText->SetText(FText::FromString(TEXT("00:00.000")));
        CountdownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.98f, 1.f, 1.f)));
        CountdownText->SetJustification(ETextJustify::Center);
        CountdownText->SetShadowOffset(FVector2D(1.5f, 1.5f));
        CountdownText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
        FSlateFontInfo Font = CountdownText->GetFont();
        Font.Size = 34;
        CountdownText->SetFont(Font);
        Background->SetContent(CountdownText);

        if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Background))
        {
            PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
            PanelSlot->SetAlignment(FVector2D(0.f, 0.f));
            PanelSlot->SetPosition(FVector2D(28.f, 28.f));
            PanelSlot->SetAutoSize(true);
        }
    }
    return Super::RebuildWidget();
}

void UGGJCountdownWidget::SetCountdownText_Implementation(const FText& NewText)
{
    if (CountdownText)
    {
        CountdownText->SetText(NewText);
    }
}

void UGGJCountdownWidget::StartCountdownPresentation(
    const FGGJCountdownPresentationSettings& NewSettings)
{
    PresentationSettings = NewSettings;
    IntroElapsed = 0.f;
    bIntroPlaying = PresentationSettings.bEnableStartIntro;
    bWaitingForIntroGeometry = bIntroPlaying;

    // 只改变 Border 画刷的透明度，不使用整个控件的 RenderOpacity，
    // 否则倒计时文字也会和灰色底板一起消失。
    if (UBorder* Background = GetBackgroundBorder())
    {
        if (!bHasCapturedBackgroundColor)
        {
            RestingBackgroundColor = Background->GetBrushColor();
            bHasCapturedBackgroundColor = true;
        }
        if (bIntroPlaying && PresentationSettings.bHideBackgroundDuringIntro)
        {
            SetBackgroundOpacityMultiplier(0.f);
        }
        else
        {
            RestoreBackgroundColor();
        }
    }

    // 先恢复最终布局，下一次 Slate 完成布局后再计算“屏幕中心到最终位置”的准确偏移。
    if (UWidget* AnimatedWidget = GetAnimatedWidget())
    {
        AnimatedWidget->SetRenderTranslation(FVector2D::ZeroVector);
        AnimatedWidget->SetRenderScale(FVector2D::UnitVector);
        AnimatedWidget->SetRenderTransformAngle(0.f);
        AnimatedWidget->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    }
    ApplyUrgencyColor();
}

void UGGJCountdownWidget::UpdateCountdownPresentation(const float RemainingSeconds)
{
    LastRemainingSeconds = FMath::Max(0.f, RemainingSeconds);
    ApplyUrgencyColor();
}

void UGGJCountdownWidget::ResetCountdownPresentation()
{
    bIntroPlaying = false;
    bWaitingForIntroGeometry = false;
    IntroElapsed = 0.f;
    IntroStartTranslation = FVector2D::ZeroVector;
    LastRemainingSeconds = TNumericLimits<float>::Max();
    if (UWidget* AnimatedWidget = GetAnimatedWidget())
    {
        AnimatedWidget->SetRenderTranslation(FVector2D::ZeroVector);
        AnimatedWidget->SetRenderScale(FVector2D::UnitVector);
        AnimatedWidget->SetRenderTransformAngle(0.f);
    }
    if (CountdownText)
    {
        CountdownText->SetColorAndOpacity(FSlateColor(PresentationSettings.NormalTextColor));
    }
    RestoreBackgroundColor();
}

void UGGJCountdownWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bIntroPlaying)
    {
        return;
    }

    if (bWaitingForIntroGeometry && !TryInitializeIntroGeometry(MyGeometry))
    {
        return;
    }
    IntroElapsed += FMath::Max(0.f, InDeltaTime);
    ApplyIntroTransform();
}

UWidget* UGGJCountdownWidget::GetAnimatedWidget() const
{
    return CountdownContainer ? CountdownContainer.Get() : CountdownText.Get();
}

UBorder* UGGJCountdownWidget::GetBackgroundBorder() const
{
    // 蓝图可以显式提供 CountdownBackground；若整体容器本身就是 Border，也自动兼容。
    if (CountdownBackground)
    {
        return CountdownBackground.Get();
    }
    return Cast<UBorder>(CountdownContainer.Get());
}

bool UGGJCountdownWidget::TryInitializeIntroGeometry(const FGeometry& WidgetGeometry)
{
    UWidget* AnimatedWidget = GetAnimatedWidget();
    if (!AnimatedWidget)
    {
        bIntroPlaying = false;
        bWaitingForIntroGeometry = false;
        return false;
    }

    const FVector2D ViewportSize = WidgetGeometry.GetLocalSize();
    const FGeometry ItemGeometry = AnimatedWidget->GetCachedGeometry();
    const FVector2D ItemSize = ItemGeometry.GetLocalSize();
    if (ViewportSize.X <= 1.f || ViewportSize.Y <= 1.f || ItemSize.X <= 1.f || ItemSize.Y <= 1.f)
    {
        // AddToViewport 后的首帧可能还没有有效 Slate Geometry，安全等待下一帧。
        return false;
    }

    const FVector2D ItemTopLeft = WidgetGeometry.AbsoluteToLocal(ItemGeometry.GetAbsolutePosition());
    const FVector2D FinalItemCenter = ItemTopLeft + ItemSize * 0.5f;
    const FVector2D NormalizedPosition(
        FMath::Clamp(PresentationSettings.IntroViewportPosition.X, 0.f, 1.f),
        FMath::Clamp(PresentationSettings.IntroViewportPosition.Y, 0.f, 1.f));
    const FVector2D DesiredStartCenter = ViewportSize * NormalizedPosition;
    IntroStartTranslation = DesiredStartCenter - FinalItemCenter;
    bWaitingForIntroGeometry = false;
    ApplyIntroTransform();
    return true;
}

void UGGJCountdownWidget::ApplyIntroTransform()
{
    UWidget* AnimatedWidget = GetAnimatedWidget();
    if (!AnimatedWidget)
    {
        bIntroPlaying = false;
        return;
    }

    const float HoldDuration = FMath::Max(0.f, PresentationSettings.CenterHoldDuration);
    const float MoveDuration = FMath::Max(0.05f, PresentationSettings.MoveToCornerDuration);
    const float IntroScale = FMath::Max(1.f, PresentationSettings.IntroScale);

    if (IntroElapsed < HoldDuration && HoldDuration > UE_KINDA_SMALL_NUMBER)
    {
        const float HoldAlpha = FMath::Clamp(IntroElapsed / HoldDuration, 0.f, 1.f);
        const float Angle = FMath::Sin(HoldAlpha * UE_TWO_PI
            * FMath::Max(0.5f, PresentationSettings.RollCycles))
            * FMath::Max(0.f, PresentationSettings.RollAmplitudeDegrees)
            * (1.f - HoldAlpha * 0.2f);
        AnimatedWidget->SetRenderTranslation(IntroStartTranslation);
        AnimatedWidget->SetRenderScale(FVector2D(IntroScale));
        AnimatedWidget->SetRenderTransformAngle(Angle);
        if (PresentationSettings.bHideBackgroundDuringIntro)
        {
            SetBackgroundOpacityMultiplier(0.f);
        }
        return;
    }

    const float MoveAlpha = FMath::Clamp((IntroElapsed - HoldDuration) / MoveDuration, 0.f, 1.f);
    const float EasedAlpha = FMath::InterpEaseInOut(0.f, 1.f, MoveAlpha,
        FMath::Max(1.f, PresentationSettings.IntroEaseExponent));
    AnimatedWidget->SetRenderTranslation(FMath::Lerp(IntroStartTranslation,
        FVector2D::ZeroVector, EasedAlpha));
    AnimatedWidget->SetRenderScale(FVector2D(FMath::Lerp(IntroScale, 1.f, EasedAlpha)));
    AnimatedWidget->SetRenderTransformAngle(0.f);

    if (PresentationSettings.bHideBackgroundDuringIntro)
    {
        const float FadeStart = FMath::Clamp(PresentationSettings.BackgroundFadeInStart, 0.f, 0.95f);
        const float FadeLinear = FMath::Clamp((EasedAlpha - FadeStart) / (1.f - FadeStart), 0.f, 1.f);
        const float FadeAlpha = FMath::InterpEaseInOut(0.f, 1.f, FadeLinear, 2.f);
        SetBackgroundOpacityMultiplier(FadeAlpha);
    }

    if (MoveAlpha >= 1.f)
    {
        bIntroPlaying = false;
        AnimatedWidget->SetRenderTranslation(FVector2D::ZeroVector);
        AnimatedWidget->SetRenderScale(FVector2D::UnitVector);
        RestoreBackgroundColor();
    }
}

void UGGJCountdownWidget::SetBackgroundOpacityMultiplier(const float OpacityMultiplier)
{
    if (!bHasCapturedBackgroundColor)
    {
        return;
    }
    if (UBorder* Background = GetBackgroundBorder())
    {
        FLinearColor Color = RestingBackgroundColor;
        Color.A *= FMath::Clamp(OpacityMultiplier, 0.f, 1.f);
        Background->SetBrushColor(Color);
    }
}

void UGGJCountdownWidget::RestoreBackgroundColor()
{
    if (bHasCapturedBackgroundColor)
    {
        if (UBorder* Background = GetBackgroundBorder())
        {
            Background->SetBrushColor(RestingBackgroundColor);
        }
    }
}

void UGGJCountdownWidget::ApplyUrgencyColor()
{
    if (!CountdownText)
    {
        return;
    }

    FLinearColor DesiredColor = PresentationSettings.NormalTextColor;
    const float Threshold = FMath::Max(0.f, PresentationSettings.UrgencyThresholdSeconds);
    if (PresentationSettings.bEnableUrgencyPulse && LastRemainingSeconds <= Threshold)
    {
        const float Period = FMath::Max(0.1f, PresentationSettings.UrgencyPulsePeriod);
        const float UrgencyElapsed = FMath::Max(0.f, Threshold - LastRemainingSeconds);
        // 0.5 - 0.5*cos 从正常色平滑到警告色再返回，不产生颜色跳变。
        const float PulseAlpha = 0.5f - 0.5f
            * FMath::Cos(UE_TWO_PI * UrgencyElapsed / Period);
        DesiredColor = FMath::Lerp(PresentationSettings.NormalTextColor,
            PresentationSettings.UrgencyTextColor, PulseAlpha);
    }
    CountdownText->SetColorAndOpacity(FSlateColor(DesiredColor));
}
