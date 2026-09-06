// 单个小人的 PrePhysics 刚体驱动实现。
// 所有实例读取同一共享输入，但由 Chaos 分别计算碰撞结果。
#include "Crowd/Agents/CrowdPhysicsMovementComponent.h"
#include "Crowd/Core/CrowdPopulationSubsystem.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UCrowdPhysicsMovementComponent::UCrowdPhysicsMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // Agent 完成刚体初始化和人口注册前不允许施力。
    PrimaryComponentTick.bStartWithTickEnabled = false;
    // 在 Chaos 物理步进之前写入力，保证本帧物理模拟立即使用本帧输入。
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCrowdPhysicsMovementComponent::Initialize(USphereComponent* InBody, const FCrowdPhysicsSettings& InSettings)
{
    Body = InBody;
    Settings = InSettings;
    Population = GetWorld()->GetSubsystem<UCrowdPopulationSubsystem>();

    // Controller 在 PostProcessInput 中发布 SharedInput；显式依赖可确保本组件随后才读取。
    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        AddTickPrerequisiteActor(Controller);
    }
}

void UCrowdPhysicsMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // 任一运行依赖失效或刚体尚未模拟时，本帧不做任何物理写入。
    if (!Body.IsValid() || !Population.IsValid() || !Body->IsSimulatingPhysics())
    {
        return;
    }
    const FVector2D Input = Population->GetSharedInput();
    // Z 恒为 0，并配合 Body 的 Z 锁形成二维玩法、三维表现。
    FVector Acceleration(Input.X * Settings.DriveAcceleration, Input.Y * Settings.DriveAcceleration, 0.f);
    const FVector Velocity = Body->GetPhysicsLinearVelocity();
    if (Velocity.SizeSquared2D() >= FMath::Square(Settings.SoftSpeedLimit))
    {
        const FVector Direction = Velocity.GetSafeNormal2D();
        // 达到软上限后，只移除“继续顺着当前速度加速”的分量。
        // 侧向转弯、反向刹车和碰撞推力仍然保留，所以拥挤感不会被硬速度裁剪破坏。
        Acceleration -= Direction * FMath::Max(0.0, FVector::DotProduct(Acceleration, Direction));
    }
    if (!Acceleration.IsNearlyZero())
    {
        // AddForce 的 bAccelChange=true 表示输入是加速度且不受质量影响。
        // Chaos 会把一帧力分配到物理子步；这里不能再手动乘 DeltaTime。
        Body->AddForce(Acceleration, NAME_None, true);
    }
}
