#pragma once

// 全局 ESC 暂停、输入模式切换、用户设置应用和退出流程。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJPauseMenuManager.generated.h"

class UGGJPauseMenuWidget;
class USoundClass;
class USoundMix;
class UWorld;

UENUM(BlueprintType)
enum class EGGJWindowMode : uint8
{
    Fullscreen UMETA(DisplayName="全屏"),
    WindowedFullscreen UMETA(DisplayName="无边框全屏"),
    Windowed UMETA(DisplayName="窗口")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGGJPauseMenuEvent);

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPauseMenuManager : public AActor
{
    GENERATED_BODY()

public:
    AGGJPauseMenuManager();

    UFUNCTION(BlueprintCallable, Category="Pause Menu")
    void TogglePauseMenu();

    UFUNCTION(BlueprintCallable, Category="Pause Menu")
    void OpenPauseMenu();

    UFUNCTION(BlueprintCallable, Category="Pause Menu")
    void ResumeGame();

    UFUNCTION(BlueprintPure, Category="Pause Menu")
    bool IsPauseMenuOpen() const { return bMenuOpen; }

    UFUNCTION(BlueprintCallable, Category="Pause Menu")
    void ReturnToMainMenu();

    UFUNCTION(BlueprintCallable, Category="Pause Menu")
    void QuitGame();

    UFUNCTION(BlueprintCallable, Category="Pause Menu|Settings")
    void ApplyUserSettings(float MasterVolume, FIntPoint Resolution,
        EGGJWindowMode WindowMode);

    UFUNCTION(BlueprintCallable, Category="Pause Menu|Settings")
    void PreviewMasterVolume(float MasterVolume);

    UFUNCTION(BlueprintPure, Category="Pause Menu|Settings")
    float GetMasterVolume() const { return CurrentMasterVolume; }

    UFUNCTION(BlueprintPure, Category="Pause Menu|Settings")
    FIntPoint GetCurrentResolution() const;

    UFUNCTION(BlueprintPure, Category="Pause Menu|Settings")
    EGGJWindowMode GetCurrentWindowMode() const;

    /** 留空时使用代码生成的简约界面；可替换为 UGGJPauseMenuWidget 蓝图子类。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|UI")
    TSubclassOf<UGGJPauseMenuWidget> WidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|UI")
    int32 WidgetZOrder = 1000;

    /** 设置后“退出到主菜单”会打开该关卡；留空时只广播事件，方便蓝图自行处理。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Navigation")
    TSoftObjectPtr<UWorld> MainMenuLevel;

    UPROPERTY(BlueprintAssignable, Category="Pause Menu|Events")
    FGGJPauseMenuEvent OnPauseMenuOpened;

    UPROPERTY(BlueprintAssignable, Category="Pause Menu|Events")
    FGGJPauseMenuEvent OnPauseMenuClosed;

    UPROPERTY(BlueprintAssignable, Category="Pause Menu|Events")
    FGGJPauseMenuEvent OnMainMenuRequested;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void SetupEscapeInput();
    void EnsureWidget();
    void LoadSavedAudioSettings();
    void ApplyMasterVolumeInternal(float Volume);

    UPROPERTY(Transient)
    TObjectPtr<UGGJPauseMenuWidget> PauseWidget;

    UPROPERTY(Transient)
    TObjectPtr<USoundClass> MasterSoundClass;

    UPROPERTY(Transient)
    TObjectPtr<USoundMix> RuntimeSoundMix;

    FTimerHandle InputRetryTimer;
    float CurrentMasterVolume = 1.f;
    bool bMenuOpen = false;
    bool bPreviousShowMouseCursor = false;
    bool bEscapeInputBound = false;
};
