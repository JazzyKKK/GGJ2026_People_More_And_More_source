#include "UI/GGJPauseMenuManager.h"

#include "UI/GGJPauseMenuWidget.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "TimerManager.h"

namespace GGJPauseMenu
{
    constexpr TCHAR SettingsSection[] = TEXT("/Script/GGJ_gamedemo.GGJPauseMenuSettings");
    constexpr TCHAR VolumeKey[] = TEXT("MasterVolume");
    // ESC 菜单中的“退出到主菜单”固定返回这个开始关卡。
    constexpr TCHAR StartLevelPackage[] = TEXT("/Game/GGJ/Level/start/Start");
    // ESC 菜单中的“开发者关卡”固定进入物理角色测试关卡。
    constexpr TCHAR DeveloperLevelPackage[] =
        TEXT("/Game/GGJ/Character/Maps/L_PhysicalCharacterLab");
}

AGGJPauseMenuManager::AGGJPauseMenuManager()
{
    PrimaryActorTick.bCanEverTick = false;
    WidgetClass = UGGJPauseMenuWidget::StaticClass();
}

void AGGJPauseMenuManager::BeginPlay()
{
    Super::BeginPlay();
    LoadSavedAudioSettings();
    SetupEscapeInput();
}

void AGGJPauseMenuManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(InputRetryTimer);
    if (RuntimeSoundMix)
    {
        UGameplayStatics::PopSoundMixModifier(this, RuntimeSoundMix);
    }
    Super::EndPlay(EndPlayReason);
}

void AGGJPauseMenuManager::SetupEscapeInput()
{
    if (bEscapeInputBound || !GetWorld())
    {
        return;
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (!PlayerController)
    {
        GetWorldTimerManager().SetTimer(InputRetryTimer, this,
            &AGGJPauseMenuManager::SetupEscapeInput, 0.1f, false);
        return;
    }

    EnableInput(PlayerController);
    if (!InputComponent)
    {
        return;
    }

    FInputKeyBinding EscapeBinding(FInputChord(EKeys::Escape), IE_Pressed);
    EscapeBinding.bConsumeInput = true;
    EscapeBinding.bExecuteWhenPaused = true;
    EscapeBinding.KeyDelegate.GetDelegateForManualSet().BindUObject(
        this, &AGGJPauseMenuManager::TogglePauseMenu);
    InputComponent->KeyBindings.Add(MoveTemp(EscapeBinding));
    bEscapeInputBound = true;
}

void AGGJPauseMenuManager::EnsureWidget()
{
    if (PauseWidget || !WidgetClass || !GetWorld())
    {
        return;
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (!PlayerController)
    {
        return;
    }

    PauseWidget = CreateWidget<UGGJPauseMenuWidget>(PlayerController, WidgetClass);
    if (PauseWidget)
    {
        PauseWidget->InitializePauseMenu(this);
        PauseWidget->AddToViewport(WidgetZOrder);
        PauseWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void AGGJPauseMenuManager::TogglePauseMenu()
{
    if (bMenuOpen)
    {
        ResumeGame();
    }
    else
    {
        OpenPauseMenu();
    }
}

void AGGJPauseMenuManager::OpenPauseMenu()
{
    if (bMenuOpen || !GetWorld())
    {
        return;
    }

    EnsureWidget();
    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (!PauseWidget || !PlayerController)
    {
        return;
    }

    bPreviousShowMouseCursor = PlayerController->bShowMouseCursor;
    PauseWidget->SetVisibility(ESlateVisibility::Visible);
    bMenuOpen = true;

    UGameplayStatics::SetGamePaused(this, true);
    PlayerController->bShowMouseCursor = true;

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(PauseWidget->TakeWidget());
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    PlayerController->SetInputMode(InputMode);
    PauseWidget->SetKeyboardFocus();

    OnPauseMenuOpened.Broadcast();
}

void AGGJPauseMenuManager::ResumeGame()
{
    if (!bMenuOpen || !GetWorld())
    {
        return;
    }

    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (PauseWidget)
    {
        PauseWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    bMenuOpen = false;
    UGameplayStatics::SetGamePaused(this, false);
    if (PlayerController)
    {
        PlayerController->SetInputMode(FInputModeGameOnly());
        PlayerController->bShowMouseCursor = bPreviousShowMouseCursor;
    }
    OnPauseMenuClosed.Broadcast();
}

void AGGJPauseMenuManager::ReturnToMainMenu()
{
    OnMainMenuRequested.Broadcast();
    UGameplayStatics::SetGamePaused(this, false);
    UGameplayStatics::OpenLevel(this, FName(GGJPauseMenu::StartLevelPackage));
}

void AGGJPauseMenuManager::OpenDeveloperLevel()
{
    UGameplayStatics::SetGamePaused(this, false);
    UGameplayStatics::OpenLevel(this, FName(GGJPauseMenu::DeveloperLevelPackage));
}

void AGGJPauseMenuManager::QuitGame()
{
    APlayerController* PlayerController = GetWorld()
        ? GetWorld()->GetFirstPlayerController() : nullptr;
    UGameplayStatics::SetGamePaused(this, false);
    UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
}

void AGGJPauseMenuManager::LoadSavedAudioSettings()
{
    float SavedVolume = 1.f;
    if (GConfig)
    {
        GConfig->GetFloat(GGJPauseMenu::SettingsSection,
            GGJPauseMenu::VolumeKey, SavedVolume, GGameUserSettingsIni);
    }
    ApplyMasterVolumeInternal(SavedVolume);
}

void AGGJPauseMenuManager::ApplyMasterVolumeInternal(float Volume)
{
    CurrentMasterVolume = FMath::Clamp(Volume, 0.f, 1.f);

    if (!MasterSoundClass)
    {
        MasterSoundClass = LoadObject<USoundClass>(nullptr,
            TEXT("/Engine/EngineSounds/Master.Master"));
    }
    if (!RuntimeSoundMix)
    {
        RuntimeSoundMix = NewObject<USoundMix>(this, TEXT("GGJRuntimeSoundMix"));
        if (RuntimeSoundMix)
        {
            UGameplayStatics::PushSoundMixModifier(this, RuntimeSoundMix);
        }
    }
    if (MasterSoundClass && RuntimeSoundMix)
    {
        UGameplayStatics::SetSoundMixClassOverride(this, RuntimeSoundMix,
            MasterSoundClass, CurrentMasterVolume, 1.f, 0.f, true);
    }
}

void AGGJPauseMenuManager::PreviewMasterVolume(float MasterVolume)
{
    ApplyMasterVolumeInternal(MasterVolume);
}

void AGGJPauseMenuManager::ApplyUserSettings(float MasterVolume,
    FIntPoint Resolution, EGGJWindowMode WindowMode)
{
    ApplyMasterVolumeInternal(MasterVolume);
    if (GConfig)
    {
        GConfig->SetFloat(GGJPauseMenu::SettingsSection,
            GGJPauseMenu::VolumeKey, CurrentMasterVolume, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }

    UGameUserSettings* UserSettings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!UserSettings)
    {
        return;
    }

    const FIntPoint SafeResolution(FMath::Max(640, Resolution.X),
        FMath::Max(360, Resolution.Y));
    UserSettings->SetScreenResolution(SafeResolution);
    switch (WindowMode)
    {
    case EGGJWindowMode::Fullscreen:
        UserSettings->SetFullscreenMode(EWindowMode::Fullscreen);
        break;
    case EGGJWindowMode::WindowedFullscreen:
        UserSettings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
        break;
    default:
        UserSettings->SetFullscreenMode(EWindowMode::Windowed);
        break;
    }
    UserSettings->ApplySettings(false);
    UserSettings->SaveSettings();
}

FIntPoint AGGJPauseMenuManager::GetCurrentResolution() const
{
    if (const UGameUserSettings* UserSettings = GEngine
        ? GEngine->GetGameUserSettings() : nullptr)
    {
        return UserSettings->GetScreenResolution();
    }
    return FIntPoint(1920, 1080);
}

EGGJWindowMode AGGJPauseMenuManager::GetCurrentWindowMode() const
{
    const UGameUserSettings* UserSettings = GEngine
        ? GEngine->GetGameUserSettings() : nullptr;
    if (!UserSettings)
    {
        return EGGJWindowMode::WindowedFullscreen;
    }

    switch (UserSettings->GetFullscreenMode())
    {
    case EWindowMode::Fullscreen:
        return EGGJWindowMode::Fullscreen;
    case EWindowMode::WindowedFullscreen:
        return EGGJWindowMode::WindowedFullscreen;
    default:
        return EGGJWindowMode::Windowed;
    }
}
