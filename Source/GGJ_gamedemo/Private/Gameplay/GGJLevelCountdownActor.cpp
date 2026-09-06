#include "Gameplay/GGJLevelCountdownActor.h"

#include "Kismet/GameplayStatics.h"
#include "UI/GGJCountdownWidget.h"

AGGJLevelCountdownActor::AGGJLevelCountdownActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    WidgetClass = UGGJCountdownWidget::StaticClass();
}

void AGGJLevelCountdownActor::StartCountdown(const float DurationSeconds)
{
    const double Duration = FMath::Max(0.0,
        static_cast<double>(DurationSeconds < 0.f ? DefaultDuration : DurationSeconds));
    CountdownEndRealTime = FPlatformTime::Seconds() + Duration;
    LastObservedRealTime = FPlatformTime::Seconds();
    PausedRemainingSeconds = Duration;
    LastDisplayedMilliseconds = INDEX_NONE;
    bIsRunning = true;
    bIsPaused = false;
    bFinishHandled = false;

    EnsureWidget();
    if (CountdownWidget)
    {
        CountdownWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    SetActorTickEnabled(true);
    OnCountdownStarted.Broadcast();
    RefreshWidgetAndEvents();
    if (Duration <= 0.0)
    {
        FinishCountdown();
    }
}

void AGGJLevelCountdownActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bIsRunning || bIsPaused)
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    if (!bContinueWhileGamePaused && GetWorld() && GetWorld()->IsPaused())
    {
        // Actor 允许在暂停时 Tick，只把暂停期间流逝的真实时间补回结束时间。
        CountdownEndRealTime += FMath::Max(0.0, Now - LastObservedRealTime);
    }
    LastObservedRealTime = Now;
    RefreshWidgetAndEvents();
    if (GetRemainingSeconds() <= 0.f)
    {
        FinishCountdown();
    }
}

void AGGJLevelCountdownActor::PauseCountdown()
{
    if (!bIsRunning || bIsPaused) { return; }
    PausedRemainingSeconds = FMath::Max(0.0,
        CountdownEndRealTime - FPlatformTime::Seconds());
    LastObservedRealTime = FPlatformTime::Seconds();
    bIsPaused = true;
    OnCountdownPaused.Broadcast();
}

void AGGJLevelCountdownActor::ResumeCountdown()
{
    if (!bIsRunning || !bIsPaused) { return; }
    CountdownEndRealTime = FPlatformTime::Seconds() + PausedRemainingSeconds;
    LastObservedRealTime = FPlatformTime::Seconds();
    bIsPaused = false;
    OnCountdownResumed.Broadcast();
}

void AGGJLevelCountdownActor::CancelCountdown(const bool bHideWidget)
{
    bIsRunning = false;
    bIsPaused = false;
    bFinishHandled = false;
    PausedRemainingSeconds = 0.0;
    SetActorTickEnabled(false);
    if (bHideWidget && CountdownWidget)
    {
        CountdownWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

float AGGJLevelCountdownActor::GetRemainingSeconds() const
{
    if (!bIsRunning) { return 0.f; }
    const double Remaining = bIsPaused ? PausedRemainingSeconds
        : CountdownEndRealTime - FPlatformTime::Seconds();
    return static_cast<float>(FMath::Max(0.0, Remaining));
}

FText AGGJLevelCountdownActor::GetFormattedRemainingTime() const
{
    const int64 Milliseconds = FMath::Max<int64>(0,
        FMath::CeilToInt64(static_cast<double>(GetRemainingSeconds()) * 1000.0));
    return FormatMilliseconds(Milliseconds);
}

FText AGGJLevelCountdownActor::FormatMilliseconds(const int64 TotalMilliseconds)
{
    const int64 SafeMilliseconds = FMath::Max<int64>(0, TotalMilliseconds);
    const int64 Minutes = SafeMilliseconds / 60000;
    const int64 Seconds = (SafeMilliseconds / 1000) % 60;
    const int64 Milliseconds = SafeMilliseconds % 1000;
    return FText::FromString(FString::Printf(TEXT("%02lld:%02lld.%03lld"),
        Minutes, Seconds, Milliseconds));
}

void AGGJLevelCountdownActor::EnsureWidget()
{
    if (CountdownWidget || !GetWorld()) { return; }
    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (!PlayerController) { return; }

    TSubclassOf<UGGJCountdownWidget> ClassToCreate = WidgetClass;
    if (!ClassToCreate)
    {
        ClassToCreate = UGGJCountdownWidget::StaticClass();
    }
    CountdownWidget = CreateWidget<UGGJCountdownWidget>(PlayerController, ClassToCreate);
    if (CountdownWidget)
    {
        CountdownWidget->AddToViewport(WidgetZOrder);
        CountdownWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void AGGJLevelCountdownActor::RefreshWidgetAndEvents()
{
    const float RemainingSeconds = GetRemainingSeconds();
    const int64 Milliseconds = FMath::Max<int64>(0,
        FMath::CeilToInt64(static_cast<double>(RemainingSeconds) * 1000.0));
    if (Milliseconds == LastDisplayedMilliseconds) { return; }

    LastDisplayedMilliseconds = Milliseconds;
    const FText FormattedTime = FormatMilliseconds(Milliseconds);
    if (CountdownWidget)
    {
        CountdownWidget->SetCountdownText(FormattedTime);
    }
    OnCountdownUpdated.Broadcast(RemainingSeconds, FormattedTime);
}

void AGGJLevelCountdownActor::FinishCountdown()
{
    if (bFinishHandled) { return; }
    bFinishHandled = true;
    bIsRunning = false;
    bIsPaused = false;
    PausedRemainingSeconds = 0.0;
    LastDisplayedMilliseconds = 0;
    if (CountdownWidget)
    {
        CountdownWidget->SetCountdownText(FormatMilliseconds(0));
    }
    SetActorTickEnabled(false);
    OnCountdownUpdated.Broadcast(0.f, FormatMilliseconds(0));
    OnCountdownFinished.Broadcast();

    if (bResetLevelOnFinished && GetWorld())
    {
        // 延到下一 Tick，让蓝图绑定的 Finished 事件有机会先执行完。
        GetWorldTimerManager().SetTimerForNextTick(this,
            &AGGJLevelCountdownActor::ResetCurrentLevel);
    }
}

void AGGJLevelCountdownActor::ResetCurrentLevel()
{
    if (!GetWorld()) { return; }
    FString LevelName = GetWorld()->GetMapName();
    LevelName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);
    UGameplayStatics::OpenLevel(this, FName(*LevelName), false);
}

void AGGJLevelCountdownActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CountdownWidget)
    {
        CountdownWidget->RemoveFromParent();
        CountdownWidget = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}
