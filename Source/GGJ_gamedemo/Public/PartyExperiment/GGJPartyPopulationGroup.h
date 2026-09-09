#pragma once

// 派对模式中“一名玩家控制的一组人物”。
// 它故意不继承旧 GGJCharacterGroupManager，避免自动寻找并占有旧模式 Leader。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PartyExperiment/GGJPartyTypes.h"
#include "TimerManager.h"
#include "GGJPartyPopulationGroup.generated.h"

class AGGJPartySpawnZone;
class AGGJPhysicalAnimationCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGGJPartyPopulationChangedSignature,
    int32, PlayerIndex, int32, NewPopulation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGGJPartyMemberSpawnedSignature,
    int32, PlayerIndex, AGGJPhysicalAnimationCharacter*, NewMember);

/** 一名本地玩家的人物名册、同步移动入口和逐个增长队列。 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPartyPopulationGroup : public AActor
{
    GENERATED_BODY()

public:
    AGGJPartyPopulationGroup();

    /** 根据上一帧输入处理人物之间的连续推挤；只在派对人口组中运行。 */
    virtual void Tick(float DeltaSeconds) override;

    /** GameMode 调用；开局人物会立刻分配到属于该玩家的全部 SpawnZone。 */
    void InitializeGroup(const FGGJPartyPlayerSetup& Setup,
        const TArray<AGGJPartySpawnZone*>& SpawnZones,
        TSubclassOf<AGGJPhysicalAnimationCharacter> FallbackMemberClass);

    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    int32 GetPlayerIndex() const { return PlayerIndex; }

    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    int32 GetPopulationCount() const;

    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    int32 GetPendingPopulationCount() const { return PendingSpawnTransforms.Num(); }

    /** 返回有效成员快照；蓝图可用于独立计数区或玩家颜色处理。 */
    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    TArray<AGGJPhysicalAnimationCharacter*> GetMembers() const;

    UFUNCTION(BlueprintPure, Category="Party Experiment|Population")
    bool ContainsMember(const AGGJPhysicalAnimationCharacter* Character) const;

    /**
     * 在指定位置增加人口。人物从 RuntimeSpawnDropHeight 高处按间隔逐个出现并掉落。
     * 返回成功进入生成队列的人数。
     */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    int32 AddPopulation(int32 Amount, FVector SpawnCenter);

    /** 使用某个派对 SpawnZone 的范围和旋转逐个增加人口。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    int32 AddPopulationAtSpawnZone(int32 Amount, AGGJPartySpawnZone* SpawnZone);

    /** Delta>0 增长、Delta<0 减少，为复制区/奖励/惩罚统一预留的接口。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    int32 RequestPopulationDelta(int32 Delta, FVector SpawnCenter);

    /** 从后加入的成员开始减少；派对实验允许人数降到 0。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    int32 RemovePopulation(int32 Amount);

    /** 安全销毁指定成员；返回该人物是否属于本玩家。 */
    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    bool DestroyMember(AGGJPhysicalAnimationCharacter* Character);

    UFUNCTION(BlueprintCallable, Category="Party Experiment|Population")
    void CancelPendingPopulation();

    /** PlayerController 每帧把一套键盘方向广播给该玩家的全部人物。 */
    void ApplySharedMovement(const FVector& WorldMovement);

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Party Experiment|Setup")
    int32 PlayerIndex = 0;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Party Experiment|Setup")
    int32 InitialPopulation = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Setup",
        meta=(ClampMin="1", ClampMax="500"))
    int32 MaxPopulation = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Setup")
    TSubclassOf<AGGJPhysicalAnimationCharacter> MemberClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Spawn Packing",
        meta=(ClampMin="40.0", Units="cm"))
    float FormationSpacing = 145.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Spawn Packing",
        meta=(ClampMin="1", ClampMax="25"))
    int32 SpawnLayerCapacity = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Spawn Packing",
        meta=(ClampMin="40.0", Units="cm"))
    float SpawnLayerHeight = 145.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Spawn Packing",
        meta=(ClampMin="0.0", ClampMax="100.0", Units="cm"))
    float SpawnPositionJitter = 18.f;

    /** 游戏中增长时，每两个人出现的间隔。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Runtime Spawn",
        meta=(ClampMin="0.02", ClampMax="5.0", Units="s"))
    float RuntimeSpawnInterval = 0.22f;

    /** 游戏中增长时的掉落高度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Runtime Spawn",
        meta=(ClampMin="0.0", ClampMax="3000.0", Units="cm"))
    float RuntimeSpawnDropHeight = 320.f;

    /**
     * 开启派对人物之间的可推动效果。
     * ACharacter 的胶囊本身不是物理刚体，因此这里额外向被挤角色的 CharacterMovement 施力。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing")
    bool bEnableCharacterPushing = true;

    /** 是否允许同一玩家的人物互相向前挤压。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing",
        meta=(EditCondition="bEnableCharacterPushing"))
    bool bPushOwnGroup = true;

    /** 是否允许推动其他玩家控制的人物。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing",
        meta=(EditCondition="bEnableCharacterPushing"))
    bool bPushOtherPlayers = true;

    /**
     * 持续推力。默认值略高于 CharacterMovement 的地面制动，确保静止人物真的会被推动。
     * 太大会把人群弹飞，太小则会再次产生“墙”的感觉。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing",
        meta=(EditCondition="bEnableCharacterPushing", ClampMin="0.0", ClampMax="1500000.0"))
    float CharacterPushForce = 350000.f;

    /** 在双方胶囊半径之外增加的推挤检测距离，适配摆动中的手脚网格。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing",
        meta=(EditCondition="bEnableCharacterPushing", ClampMin="0.0", ClampMax="200.0", Units="cm"))
    float CharacterPushPadding = 45.f;

    /** 两个人高度差超过此值时不互相推，防止楼上人物隔着地板推动楼下人物。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing",
        meta=(EditCondition="bEnableCharacterPushing", ClampMin="0.0", ClampMax="500.0", Units="cm"))
    float CharacterPushVerticalTolerance = 140.f;

    /**
     * 目标必须位于移动方向前方才受力。-1 表示周围都推，0 表示前半圆，数值越大越集中。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing",
        meta=(EditCondition="bEnableCharacterPushing", ClampMin="-1.0", ClampMax="1.0"))
    float CharacterPushMinForwardDot = -0.05f;

    /** 防止多人叠加推力后水平速度无限增长；不会限制跳跃的 Z 速度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Experiment|Character Pushing",
        meta=(EditCondition="bEnableCharacterPushing", ClampMin="50.0", ClampMax="2000.0", Units="cm/s"))
    float CharacterPushMaxHorizontalSpeed = 450.f;

    UPROPERTY(BlueprintAssignable, Category="Party Experiment|Events")
    FGGJPartyPopulationChangedSignature OnPopulationChanged;

    UPROPERTY(BlueprintAssignable, Category="Party Experiment|Events")
    FGGJPartyMemberSpawnedSignature OnMemberSpawned;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    int32 QueuePopulation(int32 Amount, const TArray<FTransform>& CandidateTransforms);
    AGGJPhysicalAnimationCharacter* SpawnMember(const FTransform& SpawnTransform);
    void ProcessNextPendingSpawn();
    void StartPendingSpawnTimerIfNeeded();
    void RemoveInvalidMembers();
    void BroadcastPopulationChanged();
    void ApplyCharacterPushing();
    void ClampPushedCharacterSpeed(AGGJPhysicalAnimationCharacter* Character) const;
    FVector MakePackedLocation(int32 LocalIndex, int32 BatchCount,
        const FVector& Center) const;

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AGGJPhysicalAnimationCharacter>> Members;

    TArray<FTransform> PendingSpawnTransforms;
    FTimerHandle PopulationSpawnTimer;
    int32 OcclusionStencilValue = 1;
    FVector LastWorldMovement = FVector::ZeroVector;
    bool bInitialized = false;
    bool bEndingPlay = false;
    bool bProcessingSpawnQueue = false;
};
