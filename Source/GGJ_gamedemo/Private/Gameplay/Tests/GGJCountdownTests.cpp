// 倒计时状态、暂停和零秒结束的轻量自动化测试。
#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/GGJLevelCountdownActor.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGGJLevelCountdownStateTest,
    "GGJ.UI.CountdownState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGGJLevelCountdownStateTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values;
    Values.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false)
        .CreatePhysicsScene(false).ShouldSimulatePhysics(false)
        .SetTransactional(false).CreateFXSystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Values);
    if (!TestNotNull(TEXT("Countdown test world is created"), World)) { return false; }

    FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
    Context.SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());
    World->GetWorldSettings()->NotifyBeginPlay();
    World->GetWorldSettings()->NotifyMatchStarted();
    World->BeginPlay();

    AGGJLevelCountdownActor* Countdown = World->SpawnActor<AGGJLevelCountdownActor>();
    if (TestNotNull(TEXT("Countdown actor can be spawned"), Countdown))
    {
        Countdown->bResetLevelOnFinished = false;
        Countdown->StartCountdown(5.f);
        TestTrue(TEXT("Positive duration starts countdown"), Countdown->IsCountdownRunning());
        TestTrue(TEXT("Countdown initially reports about five seconds"),
            Countdown->GetRemainingSeconds() > 4.9f);

        Countdown->PauseCountdown();
        TestTrue(TEXT("Countdown can be paused"), Countdown->IsCountdownPaused());
        Countdown->ResumeCountdown();
        TestFalse(TEXT("Countdown can resume"), Countdown->IsCountdownPaused());
        Countdown->CancelCountdown();
        TestFalse(TEXT("Cancel stops countdown"), Countdown->IsCountdownRunning());

        Countdown->StartCountdown(0.f);
        TestFalse(TEXT("Zero duration finishes immediately"), Countdown->IsCountdownRunning());
        TestEqual(TEXT("Finished countdown displays millisecond zero"),
            Countdown->GetFormattedRemainingTime().ToString(), FString(TEXT("00:00.000")));
    }

    World->EndPlay(EEndPlayReason::Quit);
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}

#endif
