#pragma once

// 全局 ESC 暂停、输入模式切换、用户设置应用和退出流程。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJPauseMenuManager.generated.h"

class UGGJPauseMenuWidget;
class USoundClass;
class USoundMix;
class UTexture2D;
class UWorld;

UENUM(BlueprintType)
enum class EGGJWindowMode : uint8
{
    Fullscreen UMETA(DisplayName="全屏"),
    WindowedFullscreen UMETA(DisplayName="无边框全屏"),
    Windowed UMETA(DisplayName="窗口")
};

/** 暂停菜单中可直接进入的四个关卡。 */
UENUM(BlueprintType)
enum class EGGJLevelDestination : uint8
{
    Level1 UMETA(DisplayName="LEVEL 1"),
    Level2 UMETA(DisplayName="LEVEL 2"),
    Level3 UMETA(DisplayName="LEVEL 3"),
    Developer UMETA(DisplayName="开发者关卡")
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

    /** 直接打开物理角色开发测试关卡。 */
    UFUNCTION(BlueprintCallable, Category="Pause Menu")
    void OpenDeveloperLevel();

    /**
     * 打开关卡选择页中的指定关卡。调用前会解除暂停；自定义 WBP 也可以直接使用本节点。
     */
    UFUNCTION(BlueprintCallable, Category="Pause Menu|Level Selection")
    void OpenSelectedLevel(EGGJLevelDestination Destination);

    /** 返回关卡选择卡片使用的可选预览图；未配置时默认界面显示数字占位图。 */
    UFUNCTION(BlueprintPure, Category="Pause Menu|Level Selection")
    UTexture2D* GetLevelPreview(EGGJLevelDestination Destination) const;

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

    /** “退出到主菜单”的目标；默认 /Game/GGJ/Level/start/Start。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Navigation")
    TSoftObjectPtr<UWorld> MainMenuLevel;

    /** 关卡选择的目标地图。默认值已指向当前项目的三个正式关卡和开发测试关卡。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Maps")
    TSoftObjectPtr<UWorld> Level1Map;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Maps")
    TSoftObjectPtr<UWorld> Level2Map;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Maps")
    TSoftObjectPtr<UWorld> Level3Map;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Maps")
    TSoftObjectPtr<UWorld> DeveloperMap;

    /** 可选的 16:9 预览图；留空时使用默认的 01/02/03/DEV 占位画面。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Previews")
    TObjectPtr<UTexture2D> Level1Preview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Previews")
    TObjectPtr<UTexture2D> Level2Preview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Previews")
    TObjectPtr<UTexture2D> Level3Preview;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pause Menu|Level Selection|Previews")
    TObjectPtr<UTexture2D> DeveloperPreview;

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
