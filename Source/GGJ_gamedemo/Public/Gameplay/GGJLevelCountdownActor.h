#pragma once

// 由关卡蓝图/机关启动的毫秒倒计时控制器；结束时可自动重载当前关卡。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UI/GGJCountdownWidget.h"
#include "GGJLevelCountdownActor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGGJCountdownSimpleSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGGJCountdownUpdatedSignature,
    float, RemainingSeconds, FText, FormattedTime);

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJLevelCountdownActor : public AActor
{
    GENERATED_BODY()

public:
    AGGJLevelCountdownActor();
    virtual void Tick(float DeltaSeconds) override;

    /**
     * 开始或重新开始倒计时。DurationSeconds < 0 时使用 DefaultDuration。
     * 第一帧才显示 UI，因此可以直接由任意蓝图条件调用。
     */
    UFUNCTION(BlueprintCallable, Category="Level Countdown", meta=(AdvancedDisplay="DurationSeconds"))
    void StartCountdown(float DurationSeconds = -1.f);

    UFUNCTION(BlueprintCallable, Category="Level Countdown")
    void PauseCountdown();

    UFUNCTION(BlueprintCallable, Category="Level Countdown")
    void ResumeCountdown();

    /** 停止倒计时；bHideWidget=true 时同时隐藏 UI。 */
    UFUNCTION(BlueprintCallable, Category="Level Countdown")
    void CancelCountdown(bool bHideWidget = true);

    /** 立即重新加载当前关卡，也可以由蓝图单独调用。 */
    UFUNCTION(BlueprintCallable, Category="Level Countdown")
    void ResetCurrentLevel();

    UFUNCTION(BlueprintPure, Category="Level Countdown")
    bool IsCountdownRunning() const { return bIsRunning; }

    UFUNCTION(BlueprintPure, Category="Level Countdown")
    bool IsCountdownPaused() const { return bIsPaused; }

    UFUNCTION(BlueprintPure, Category="Level Countdown")
    float GetRemainingSeconds() const;

    /** 格式固定为 MM:SS.mmm，例如 01:05.037。 */
    UFUNCTION(BlueprintPure, Category="Level Countdown")
    FText GetFormattedRemainingTime() const;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level Countdown|Setup",
        meta=(ClampMin="0.0", Units="s"))
    float DefaultDuration = 10.f;

    /** 开启后归零自动重载当前关卡；关闭后只广播 OnCountdownFinished。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level Countdown|Setup")
    bool bResetLevelOnFinished = true;

    /** 默认 false：ESC 暂停整个游戏时倒计时也冻结。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level Countdown|Setup")
    bool bContinueWhileGamePaused = false;

    /** 留空时使用 C++ 自带左上角样式；可指定 UGGJCountdownWidget 的蓝图子类。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level Countdown|UI")
    TSubclassOf<UGGJCountdownWidget> WidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level Countdown|UI")
    int32 WidgetZOrder = 100;

    /** 开场居中提示、平滑归位和最后30秒颜色呼吸的全部可调参数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level Countdown|UI")
    FGGJCountdownPresentationSettings PresentationSettings;

    UPROPERTY(BlueprintAssignable, Category="Level Countdown|Events")
    FGGJCountdownSimpleSignature OnCountdownStarted;

    UPROPERTY(BlueprintAssignable, Category="Level Countdown|Events")
    FGGJCountdownSimpleSignature OnCountdownPaused;

    UPROPERTY(BlueprintAssignable, Category="Level Countdown|Events")
    FGGJCountdownSimpleSignature OnCountdownResumed;

    /** 显示发生变化时广播，可用于蓝图音效、颜色或额外动画。 */
    UPROPERTY(BlueprintAssignable, Category="Level Countdown|Events")
    FGGJCountdownUpdatedSignature OnCountdownUpdated;

    UPROPERTY(BlueprintAssignable, Category="Level Countdown|Events")
    FGGJCountdownSimpleSignature OnCountdownFinished;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void EnsureWidget();
    void RefreshWidgetAndEvents();
    void FinishCountdown();
    static FText FormatMilliseconds(int64 TotalMilliseconds);

    UPROPERTY(Transient)
    TObjectPtr<UGGJCountdownWidget> CountdownWidget;

    double CountdownEndRealTime = 0.0;
    double LastObservedRealTime = 0.0;
    double PausedRemainingSeconds = 0.0;
    int64 LastDisplayedMilliseconds = INDEX_NONE;
    bool bIsRunning = false;
    bool bIsPaused = false;
    bool bFinishHandled = false;
};
