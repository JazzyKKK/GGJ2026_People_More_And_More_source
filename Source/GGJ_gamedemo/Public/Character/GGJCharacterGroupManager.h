#pragma once

// 单个 World 中的物理动画人物名册与共享控制入口。
// 复制区、按钮和出口以后只与本类交互，不直接保存或销毁人物引用。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJCharacterGroupManager.generated.h"

class AGGJPhysicalAnimationCharacter;
class AGGJGroupCameraActor;

/** 人数变化事件：UI、相机或关卡规则可以在蓝图中监听。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGGJPopulationChangedSignature, int32, NewPopulation);

/**
 * 管理一组同步输入的 Physical Animation Character。
 *
 * 只有 Leader 被 PlayerController 占有并提供镜头；其它成员没有 Controller，
 * 由 ApplySharedMovement 使用强制 Movement Input 驱动。人物仍各自进行碰撞和物理求解。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJCharacterGroupManager : public AActor
{
    GENERATED_BODY()

public:
    AGGJCharacterGroupManager();

    /** 当前有效成员数量；失效弱引用不会被计入。 */
    UFUNCTION(BlueprintPure, Category="Character Group|Population")
    int32 GetPopulationCount() const;

    /** 返回当前有效成员快照，蓝图可用于按钮/出口统计，但不应长期保存该数组。 */
    UFUNCTION(BlueprintPure, Category="Character Group|Population")
    TArray<AGGJPhysicalAnimationCharacter*> GetMembers() const;

    UFUNCTION(BlueprintPure, Category="Character Group|Population")
    AGGJPhysicalAnimationCharacter* GetLeader() const { return Leader.Get(); }

    /** GameMode 设置当前群体相机，角色借此转发 Q/E 并按镜头方向解释移动。 */
    void SetGroupCamera(AGGJGroupCameraActor* NewCamera);

    UFUNCTION(BlueprintPure, Category="Character Group|Camera")
    AGGJGroupCameraActor* GetGroupCamera() const { return GroupCamera.Get(); }

    /** 登记场景中已有的人物；bAsLeader=true 时让它成为镜头和输入来源。 */
    UFUNCTION(BlueprintCallable, Category="Character Group|Population")
    bool RegisterMember(AGGJPhysicalAnimationCharacter* Character, bool bAsLeader = false);

    /** 人物销毁时由 Character::EndPlay 自动注销。 */
    UFUNCTION(BlueprintCallable, Category="Character Group|Population")
    void UnregisterMember(AGGJPhysicalAnimationCharacter* Character);

    /**
     * 安全销毁指定成员。若销毁 Leader，会自动选择剩余成员并把 PlayerController 移交给它；
     * 返回该角色是否原本属于当前群体。
     */
    UFUNCTION(BlueprintCallable, Category="Character Group|Population")
    bool DestroyMember(AGGJPhysicalAnimationCharacter* Character);

    /**
     * 把新增人物加入逐个生成队列；第一人立即出现，其余按 RuntimeSpawnInterval 出现。
     * 返回本次成功接受的数量（包含仍在排队的人），并受 MaxPopulation 限制。
     */
    UFUNCTION(BlueprintCallable, Category="Character Group|Population")
    int32 AddPopulation(int32 Amount, FVector SpawnCenter);

    /** 尚未真正生成、正在等待依次掉落的人数。 */
    UFUNCTION(BlueprintPure, Category="Character Group|Population")
    int32 GetPendingPopulationCount() const { return PendingSpawnLocations.Num(); }

    /** 取消尚未出现的人物；已生成的成员不会被删除。切关或失败时可调用。 */
    UFUNCTION(BlueprintCallable, Category="Character Group|Population")
    void CancelPendingPopulation();

    /** 从最后生成的非 Leader 成员开始减少；永远保留 Leader。返回实际减少数量。 */
    UFUNCTION(BlueprintCallable, Category="Character Group|Population")
    int32 RemovePopulation(int32 Amount);

    /**
     * 复制/缩减规则以后统一调用的接口。Delta>0 增加，Delta<0 减少；
     * 本版本没有任何场景 Actor 会主动调用它。
     */
    UFUNCTION(BlueprintCallable, Category="Character Group|Population")
    int32 RequestPopulationDelta(int32 Delta, FVector SpawnCenter);

    /** PlayerController 每帧只计算一次世界方向，本函数把同一输入发给所有有效成员。 */
    UFUNCTION(BlueprintCallable, Category="Character Group|Control")
    void ApplySharedMovement(FVector WorldMovement);

    /** 将一次跳跃按下广播给所有成员。 */
    UFUNCTION(BlueprintCallable, Category="Character Group|Control")
    void ApplySharedJumpStart();

    /** 将跳跃松开广播给所有成员，保证可变跳跃高度保持一致。 */
    UFUNCTION(BlueprintCallable, Category="Character Group|Control")
    void ApplySharedJumpEnd();

    /** 默认 3：一个被占有的 Leader，加两个无 Controller 的成员。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Setup", meta=(ClampMin="1", ClampMax="100"))
    int32 InitialPopulation = 3;

    /** 主动布娃娃成本较高，Game Jam 阶段先用硬上限防止指数增长失控。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Setup", meta=(ClampMin="1", ClampMax="200"))
    int32 MaxPopulation = 30;

    /** 每层角色中心的横向间距；越小越拥挤，过小会因初始穿插而猛烈弹开。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Spawn Packing",
        meta=(ClampMin="40.0", Units="cm"))
    float FormationSpacing = 145.f;

    /** 一层最多放多少人，超过后继续向上叠一层。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Spawn Packing",
        meta=(ClampMin="1", ClampMax="25"))
    int32 SpawnLayerCapacity = 4;

    /** 每层之间的垂直距离；略小于人物高度会产生挤压和倒塌。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Spawn Packing",
        meta=(ClampMin="40.0", Units="cm"))
    float SpawnLayerHeight = 145.f;

    /** 每个出生点的水平随机扰动，破坏整齐平衡，让堆叠更容易倒塌。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Spawn Packing",
        meta=(ClampMin="0.0", ClampMax="100.0", Units="cm"))
    float SpawnPositionJitter = 18.f;

    /** 运行时 Add Population 连续生成两个人之间的时间。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Runtime Spawn",
        meta=(ClampMin="0.02", ClampMax="5.0", Units="s"))
    float RuntimeSpawnInterval = 0.22f;

    /** Add Population 的整批出生点额外抬高，让人物依次从空中掉入场景。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Runtime Spawn",
        meta=(ClampMin="0.0", ClampMax="3000.0", Units="cm"))
    float RuntimeSpawnDropHeight = 320.f;

    /** 留空时自动采用当前 Leader 的蓝图类，因此会保留模型、动画和碰撞配置。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character Group|Setup")
    TSubclassOf<AGGJPhysicalAnimationCharacter> MemberClass;

    UPROPERTY(BlueprintAssignable, Category="Character Group|Events")
    FGGJPopulationChangedSignature OnPopulationChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** 等待 GameMode 生成并占有玩家 Pawn，然后建立三人小组。 */
    void TryInitializeGroup();
    void ScheduleInitializationRetry();
    void RemoveInvalidMembers();
    FVector GetFormationLocation(int32 LocalIndex, int32 BatchCount,
        const FVector& Center) const;
    AGGJPhysicalAnimationCharacter* SpawnMember(const FVector& Location);
    void ProcessNextPendingSpawn();
    void EnsurePopulationSpawnTimer();
    bool CreateInitialPopulationFromSpawnZones(AGGJPhysicalAnimationCharacter* PlayerCharacter);
    void PromoteLeaderAfterRemoval(AGGJPhysicalAnimationCharacter* RemovedLeader);
    void BroadcastPopulationChanged();

    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AGGJPhysicalAnimationCharacter>> Members;

    UPROPERTY(Transient)
    TWeakObjectPtr<AGGJPhysicalAnimationCharacter> Leader;

    UPROPERTY(Transient)
    TWeakObjectPtr<AGGJGroupCameraActor> GroupCamera;

    /** 预先算好的世界出生位置；TArray 足以覆盖当前最多 200 人的 Game Jam 规模。 */
    TArray<FVector> PendingSpawnLocations;

    FTimerHandle InitializationTimer;
    FTimerHandle PopulationSpawnTimer;
    bool bInitialGroupCreated = false;
    bool bEndingPlay = false;
    bool bProcessingSpawnQueue = false;
};
