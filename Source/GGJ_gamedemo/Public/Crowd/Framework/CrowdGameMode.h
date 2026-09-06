#pragma once

// 人群关卡的最小 GameMode。
// 它只指定框架类，不生成灯光、场地或人群，关卡内容仍完全由设计师搭建。

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CrowdGameMode.generated.h"

class AGGJPauseMenuManager;

/** 为关卡选择 ACrowdPlayerController 和唯一的 ACrowdCameraPawn。 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API ACrowdGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    /** 设置原生兜底框架类；BP_CrowdGameMode 仍可在 Class Defaults 中覆盖。 */
    ACrowdGameMode();

    virtual void StartPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pause Menu")
    TSubclassOf<AGGJPauseMenuManager> PauseMenuManagerClass;

private:
    UPROPERTY(Transient)
    TObjectPtr<AGGJPauseMenuManager> PauseMenuManager;
};
