#pragma once

// 独立物理角色测试地图使用的 GameMode；不会覆盖现有 CrowdGameMode。

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GGJPhysicalAnimationGameMode.generated.h"

class AGGJCharacterGroupManager;
class AGGJGroupCameraActor;
class AGGJPauseMenuManager;

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPhysicalAnimationGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGGJPhysicalAnimationGameMode();

    /** 优先使用 PlayerStart；缺失时回退到启用的第一个 GGJSpawnZone。 */
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    /** 允许出生点附近存在轻微碰撞，避免重载后 Pawn 因碰撞直接生成失败。 */
    virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,
        const FTransform& SpawnTransform) override;

    /** 生成一个全局群体管理器；若关卡已手动放置则复用关卡中的实例。 */
    virtual void StartPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Group")
    TSubclassOf<AGGJCharacterGroupManager> GroupManagerClass;

    /** 可在蓝图中替换相机外观/参数；关卡已放置同类 Actor 时优先复用。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Group|Camera")
    TSubclassOf<AGGJGroupCameraActor> GroupCameraClass;

    /** 普通关卡为 true；固定相机模式为 false，要求设计师在关卡里摆放相机。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Character Group|Camera")
    bool bAutoSpawnGroupCamera = true;

    /** 默认自动生成；若关卡已放置同类实例，则复用关卡实例以保留其主菜单等配置。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pause Menu")
    TSubclassOf<AGGJPauseMenuManager> PauseMenuManagerClass;

    UFUNCTION(BlueprintPure, Category="Character Group")
    AGGJCharacterGroupManager* GetGroupManager() const { return GroupManager; }

    UFUNCTION(BlueprintPure, Category="Character Group|Camera")
    AGGJGroupCameraActor* GetGroupCamera() const { return GroupCamera; }

private:
    UPROPERTY(Transient)
    TObjectPtr<AGGJCharacterGroupManager> GroupManager;

    UPROPERTY(Transient)
    TObjectPtr<AGGJGroupCameraActor> GroupCamera;

    UPROPERTY(Transient)
    TObjectPtr<AGGJPauseMenuManager> PauseMenuManager;
};
