#pragma once

// 默认的左上角毫秒倒计时界面；可直接使用，也可创建蓝图子类替换视觉。

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GGJCountdownWidget.generated.h"

class UTextBlock;
class UWidget;
class UBorder;

/** 倒计时开场提示与最后阶段颜色呼吸的可调参数。 */
USTRUCT(BlueprintType)
struct FGGJCountdownPresentationSettings
{
    GENERATED_BODY()

    /** 开始倒计时时，是否先在屏幕中央强调，再移动到 Widget 原本布局位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro")
    bool bEnableStartIntro = true;

    /** 中央提示阶段相对于最终左上角大小的倍率。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro", ClampMin="1.0", ClampMax="6.0"))
    float IntroScale = 2.35f;

    /** 在屏幕中央停留并摇晃的时间。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro", ClampMin="0.0", ClampMax="5.0", Units="s"))
    float CenterHoldDuration = 0.65f;

    /** 从屏幕中央平滑移动到左上角原布局的时间。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro", ClampMin="0.05", ClampMax="5.0", Units="s"))
    float MoveToCornerDuration = 0.85f;

    /** 中央提示的最大 2D Roll 角度；正负交替形成轻微摇晃。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro", ClampMin="0.0", ClampMax="30.0", Units="deg"))
    float RollAmplitudeDegrees = 4.f;

    /** 中央停留期间完成几次左右摇摆。整数可以保证移动开始时自然回正。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro", ClampMin="0.5", ClampMax="8.0"))
    float RollCycles = 2.f;

    /** 位移动画的 Ease In/Out 指数；越大越强调加速与减速。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro", ClampMin="1.0", ClampMax="6.0"))
    float IntroEaseExponent = 2.5f;

    /** 默认正中央；使用归一化视口坐标，(0.5, 0.5) 表示屏幕中心。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro"))
    FVector2D IntroViewportPosition = FVector2D(0.5f, 0.5f);

    /** 中央停留和摇晃时隐藏倒计时底板，只保留文字。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro"))
    bool bHideBackgroundDuringIntro = true;

    /** 移向左上角的进度达到该比例后，底板才开始平滑显现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Intro",
        meta=(EditCondition="bEnableStartIntro && bHideBackgroundDuringIntro", ClampMin="0.0", ClampMax="0.95"))
    float BackgroundFadeInStart = 0.35f;

    /** 是否在最后阶段让文字在正常色和警告色之间平滑变化。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Urgency")
    bool bEnableUrgencyPulse = true;

    /** 剩余时间小于等于该值时开始颜色呼吸。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Urgency",
        meta=(EditCondition="bEnableUrgencyPulse", ClampMin="0.0", ClampMax="600.0", Units="s"))
    float UrgencyThresholdSeconds = 30.f;

    /** 一次“白→红→白”的完整周期。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Urgency",
        meta=(EditCondition="bEnableUrgencyPulse", ClampMin="0.1", ClampMax="10.0", Units="s"))
    float UrgencyPulsePeriod = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Urgency")
    FLinearColor NormalTextColor = FLinearColor(0.95f, 0.98f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Countdown Urgency")
    FLinearColor UrgencyTextColor = FLinearColor(1.f, 0.055f, 0.035f, 1.f);
};

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API UGGJCountdownWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 更新显示文本。蓝图自定义 Widget 也可以覆写此事件。 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Countdown UI")
    void SetCountdownText(const FText& NewText);
    virtual void SetCountdownText_Implementation(const FText& NewText);

    /** 每次 StartCountdown 调用，用最新配置重新播放一次中央提示动画。 */
    UFUNCTION(BlueprintCallable, Category="Countdown UI|Presentation")
    void StartCountdownPresentation(const FGGJCountdownPresentationSettings& NewSettings);

    /** CountdownActor 传入当前剩余时间，用它确定最后阶段颜色，暂停时颜色也会自然冻结。 */
    UFUNCTION(BlueprintCallable, Category="Countdown UI|Presentation")
    void UpdateCountdownPresentation(float RemainingSeconds);

    /** 取消倒计时或隐藏 Widget 时恢复最终布局和正常颜色。 */
    UFUNCTION(BlueprintCallable, Category="Countdown UI|Presentation")
    void ResetCountdownPresentation();

    /** 名为 CountdownText 的蓝图 TextBlock 会自动绑定；不做蓝图时由 C++ 创建默认控件。 */
    UPROPERTY(BlueprintReadOnly, Category="Countdown UI", meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> CountdownText;

    /**
     * 可选的整体动画容器。蓝图 Widget 若创建名为 CountdownContainer 的控件，
     * 中央放大、位移和 Roll 会作用于它；否则自动退回只动画 CountdownText。
     */
    UPROPERTY(BlueprintReadOnly, Category="Countdown UI", meta=(BindWidgetOptional))
    TObjectPtr<UWidget> CountdownContainer;

    /**
     * 可选的底板 Border。原生 Widget 会自动赋值；自定义蓝图若希望开场隐藏底板，
     * 请把对应 Border 命名为 CountdownBackground。
     */
    UPROPERTY(BlueprintReadOnly, Category="Countdown UI", meta=(BindWidgetOptional))
    TObjectPtr<UBorder> CountdownBackground;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UWidget* GetAnimatedWidget() const;
    UBorder* GetBackgroundBorder() const;
    bool TryInitializeIntroGeometry(const FGeometry& WidgetGeometry);
    void ApplyIntroTransform();
    void ApplyUrgencyColor();
    void SetBackgroundOpacityMultiplier(float OpacityMultiplier);
    void RestoreBackgroundColor();

    FGGJCountdownPresentationSettings PresentationSettings;
    FVector2D IntroStartTranslation = FVector2D::ZeroVector;
    float IntroElapsed = 0.f;
    float LastRemainingSeconds = TNumericLimits<float>::Max();
    FLinearColor RestingBackgroundColor = FLinearColor::White;
    bool bIntroPlaying = false;
    bool bWaitingForIntroGeometry = false;
    bool bHasCapturedBackgroundColor = false;
};
