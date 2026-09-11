#pragma once

// 默认简约暂停菜单：返回游戏、设置、关卡选择、主菜单、退出，以及显示和音量设置。

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GGJPauseMenuWidget.generated.h"

class AGGJPauseMenuManager;
class UButton;
class UComboBoxString;
class UHorizontalBox;
class UImage;
class USizeBox;
class USlider;
class UTextBlock;
class UVerticalBox;

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API UGGJPauseMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializePauseMenu(AGGJPauseMenuManager* InManager);

    /** 自定义 WBP 子类可通过此节点调用返回游戏、应用设置、退出等接口。 */
    UFUNCTION(BlueprintPure, Category="Pause Menu")
    AGGJPauseMenuManager* GetPauseMenuManager() const { return Manager; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry,
        const FKeyEvent& InKeyEvent) override;

private:
    UButton* MakeMenuButton(UVerticalBox* Parent, const FText& Label, FName Name);
    UButton* MakeLevelCard(UHorizontalBox* Parent, const FText& Label,
        const FText& Placeholder, FName Name, TObjectPtr<UImage>& OutPreviewImage);
    UTextBlock* MakeLabel(UVerticalBox* Parent, const FText& Label, int32 FontSize);
    void PopulateSettings();
    void PopulateLevelPreviews();
    void ShowMainPanel();

    UFUNCTION()
    void HandleResumeClicked();
    UFUNCTION()
    void HandleSettingsClicked();
    UFUNCTION()
    void HandleLevelSelectionClicked();
    UFUNCTION()
    void HandleMainMenuClicked();
    UFUNCTION()
    void HandleDeveloperLevelClicked();
    UFUNCTION()
    void HandleQuitClicked();
    UFUNCTION()
    void HandleSettingsBackClicked();
    UFUNCTION()
    void HandleLevelSelectionBackClicked();
    UFUNCTION()
    void HandleLevel1Clicked();
    UFUNCTION()
    void HandleLevel2Clicked();
    UFUNCTION()
    void HandleLevel3Clicked();
    UFUNCTION()
    void HandleApplySettingsClicked();
    UFUNCTION()
    void HandleVolumeChanged(float Value);

    UPROPERTY(Transient)
    TObjectPtr<AGGJPauseMenuManager> Manager;
    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> MainPanel;
    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> SettingsPanel;
    UPROPERTY(Transient)
    TObjectPtr<UVerticalBox> LevelSelectionPanel;
    UPROPERTY(Transient)
    TObjectPtr<USizeBox> LevelSelectionContainer;
    UPROPERTY(Transient)
    TObjectPtr<UImage> Level1PreviewImage;
    UPROPERTY(Transient)
    TObjectPtr<UImage> Level2PreviewImage;
    UPROPERTY(Transient)
    TObjectPtr<UImage> Level3PreviewImage;
    UPROPERTY(Transient)
    TObjectPtr<UImage> DeveloperPreviewImage;
    UPROPERTY(Transient)
    TObjectPtr<USlider> VolumeSlider;
    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> VolumeValueText;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> ResolutionCombo;
    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> WindowModeCombo;
};
