#pragma once

// 单个小人的刚体驱动组件。
// 它没有独立输入状态，每帧从 UCrowdPopulationSubsystem 读取全体共享方向。

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Crowd/Data/CrowdAgentPhysicsData.h"
#include "CrowdPhysicsMovementComponent.generated.h"

class USphereComponent;
class UCrowdPopulationSubsystem;

/**
 * 在 Chaos 模拟前，将共享 XY 输入转换为刚体加速度。
 * 使用 ActorComponent 而非 PawnMovementComponent，是因为小人不是被 Controller 占有的 Pawn。
 */
UCLASS(ClassGroup=(Crowd), meta=(BlueprintSpawnableComponent))
class GGJ_GAMEDEMO_API UCrowdPhysicsMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    /** 配置 PrePhysics Tick；默认关闭，等待 Body 完成物理初始化。 */
    UCrowdPhysicsMovementComponent();

    /** 绑定所属小人的物理球、参数和当前 World 的人口子系统。 */
    void Initialize(USphereComponent* InBody, const FCrowdPhysicsSettings& InSettings);

    /** 读取共享输入、执行软限速并向物理球施加加速度。 */
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** 弱引用：组件不拥有 Body 的生命周期。 */
    TWeakObjectPtr<USphereComponent> Body;

    /** 弱引用：WorldSubsystem 可能在关卡结束时先进入反初始化。 */
    TWeakObjectPtr<UCrowdPopulationSubsystem> Population;

    /** Spawn 时复制的安全参数；运行过程中无需反复读取 Data Asset。 */
    FCrowdPhysicsSettings Settings;
};
