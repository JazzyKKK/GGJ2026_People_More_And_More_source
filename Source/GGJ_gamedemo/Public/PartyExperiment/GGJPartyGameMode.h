#pragma once

// 与正式单人流程隔离的同屏派对实验 GameMode。
// 只有关卡显式选择本 GameMode 时，才会创建双人人群、共享相机和双套键盘控制。

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PartyExperiment/GGJPartyTypes.h"
#include "GGJPartyGameMode.generated.h"

class AGGJPartyCameraActor;
class AGGJPartyPopulationGroup;
class AGGJPartySpawnZone;
class AGGJPhysicalAnimationCharacter;

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPartyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AGGJPartyGameMode();
    virtual void StartPlay() override;

    UFUNCTION(BlueprintPure, Category="Party Experiment|Players")
    AGGJPartyPopulationGroup* GetPlayerGroup(int32 PlayerIndex) const;

    UFUNCTION(BlueprintPure, Category="Party Experiment|Players")
    TArray<AGGJPartyPopulationGroup*> GetAllPlayerGroups() const;

    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    int32 GetPlayerPopulationCount(int32 PlayerIndex) const;

    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    int32 GetTotalPopulationCount() const;

    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    int32 GetOwningPlayerIndex(const AGGJPhysicalAnimationCharacter* Character) const;

    /** 在世界位置为指定玩家逐个掉落新增人物。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    int32 AddPopulationForPlayer(int32 PlayerIndex, int32 Amount, FVector SpawnCenter);

    /** 在指定出生区为它所属的玩家逐个掉落新增人物。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    int32 AddPopulationAtSpawnZone(AGGJPartySpawnZone* SpawnZone, int32 Amount);

    /** 统一增长接口：正数增加、负数减少，返回实际变化量。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    int32 RequestPopulationDeltaForPlayer(int32 PlayerIndex, int32 Delta,
        FVector SpawnCenter);

    /**
     * 屏幕空间移动入口，键盘控制器和未来手柄都调用这里。
     * X=画面右，Y=画面上；相机转向过程中方向会实时保持符合画面。
     */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Control")
    void ApplyPlayerScreenMovement(int32 PlayerIndex, FVector2D ScreenMovement);

    /** 共享相机旋转接口；默认只有 PlayerController 的 Q/E 调用。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Camera")
    bool RequestSharedCameraTurn(int32 Direction);

    UFUNCTION(BlueprintPure, Category="Party Experiment|Camera")
    AGGJPartyCameraActor* GetPartyCamera() const { return PartyCamera; }

    /**
     * 默认两项分别是玩家0和玩家1。添加玩家2/3并增加键位方案与 SpawnZone 即可扩展。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Players")
    TArray<FGGJPartyPlayerSetup> PlayerSetups;

    /** PlayerSetup 没有单独填写人物类时使用它。请在 BP GameMode 中选择现有人物蓝图。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Players")
    TSubclassOf<AGGJPhysicalAnimationCharacter> DefaultMemberClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Classes")
    TSubclassOf<AGGJPartyPopulationGroup> PopulationGroupClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Classes")
    TSubclassOf<AGGJPartyCameraActor> PartyCameraClass;

    /** 关卡没有手动放置 PartyCamera 时是否自动创建。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Camera")
    bool bAutoSpawnPartyCamera = true;

private:
    void CreatePopulationGroups();
    void CreateAndActivateCamera();

    UPROPERTY(Transient)
    TArray<TObjectPtr<AGGJPartyPopulationGroup>> ActiveGroups;

    UPROPERTY(Transient)
    TObjectPtr<AGGJPartyCameraActor> PartyCamera;
};
