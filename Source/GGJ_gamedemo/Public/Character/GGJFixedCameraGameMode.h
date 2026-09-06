#pragma once

// 固定场景相机关卡专用 GameMode，保留群体、区域、暂停菜单等完整玩法框架。

#include "CoreMinimal.h"
#include "Character/GGJPhysicalAnimationGameMode.h"
#include "GGJFixedCameraGameMode.generated.h"

/**
 * 使用关卡内手动摆放的 AGGJFixedGroupCameraActor。
 * 其余逻辑全部继承自 PhysicalAnimationGameMode：默认人物、三人群体、Spawn Zone、
 * Add Population、Destroy Zone、Count Zone 和 ESC 菜单均保持不变。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJFixedCameraGameMode : public AGGJPhysicalAnimationGameMode
{
    GENERATED_BODY()

public:
    AGGJFixedCameraGameMode();

    virtual void StartPlay() override;

    /** 可替换为 GGJFixedGroupCameraActor 的蓝图子类；不会继承旧 GameMode 的相机覆盖值。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Fixed Camera")
    TSubclassOf<class AGGJFixedGroupCameraActor> FixedCameraClass;
};
