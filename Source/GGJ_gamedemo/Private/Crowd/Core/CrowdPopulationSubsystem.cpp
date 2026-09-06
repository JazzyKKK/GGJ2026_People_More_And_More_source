// 当前 World 的人口注册、共享输入和安全生成实现。
// Controller、Director 与各 MovementComponent 通过这个中心解耦。
#include "Crowd/Core/CrowdPopulationSubsystem.h"
#include "Crowd/Agents/CrowdAgent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

// 资源编辑器、缩略图等 World 不应生成游戏运行时子系统。
bool UCrowdPopulationSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCrowdPopulationSubsystem::Deinitialize()
{
    // 这里不主动 Destroy：World 正在统一结束生命周期，只需清掉自己的缓存。
    SharedInput = FVector2D::ZeroVector;
    Agents.Reset();
    Super::Deinitialize();
}

int32 UCrowdPopulationSubsystem::GetPopulationCount() const
{
    int32 Count = 0;
    // TWeakObjectPtr 可能在 Actor 销毁后暂时留在数组中，所以实时判断有效性。
    for (const TWeakObjectPtr<ACrowdAgent>& Agent : Agents)
    {
        Count += Agent.IsValid() ? 1 : 0;
    }
    return Count;
}

void UCrowdPopulationSubsystem::SetSharedInput(FVector2D Input)
{
    // 物理输入绝不能携带 NaN/Infinity，否则会污染刚体速度和位置。
    if (!FMath::IsFinite(Input.X) || !FMath::IsFinite(Input.Y))
    {
        SharedInput = FVector2D::ZeroVector;
        return;
    }
    // 只归一化超出单位圆的值；0.2 之类的手柄模拟量仍保持 0.2。
    SharedInput = Input.SizeSquared() > 1.0 ? Input.GetSafeNormal() : Input;
}

ACrowdAgent* UCrowdPopulationSubsystem::SpawnAgent(TSubclassOf<ACrowdAgent> AgentClass,
    const FVector& Location, const FCrowdPhysicsSettings& Settings)
{
    // 类、位置和配置都先验证，失败时不对 World 或注册表产生任何副作用。
    if (!AgentClass || Location.ContainsNaN())
    {
        return nullptr;
    }
    const FCrowdPhysicsSettings SafeSettings = Settings.Sanitized();
    // 出生位置被阻挡时直接失败，不让 Chaos 用“强行分离”制造爆炸式速度。
    if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_PhysicsBody,
        FCollisionShape::MakeSphere(SafeSettings.Radius + 1.f), FCollisionQueryParams(SCENE_QUERY_STAT(CrowdSpawn))))
    {
        return nullptr;
    }
    const FTransform Transform(FRotator::ZeroRotator, Location);
    // Deferred Spawn 允许蓝图 Construction Script 先完成，然后再覆盖最终权威物理参数。
    ACrowdAgent* Agent = GetWorld()->SpawnActorDeferred<ACrowdAgent>(AgentClass, Transform, nullptr,
        nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Agent)
    {
        return nullptr;
    }
    Agent->FinishSpawning(Transform);
    if (!IsValid(Agent))
    {
        return nullptr;
    }
    // 蓝图构造已经结束，最后应用权威物理配置，避免蓝图默认值误改碰撞/质量。
    Agent->InitializePhysics(SafeSettings);
    Agents.Add(Agent);
    Agent->ActivateMovement();
    return Agent;
}

void UCrowdPopulationSubsystem::UnregisterAgent(ACrowdAgent* Agent)
{
    // RemoveAllSwap 不保证数组顺序，但这里不依赖顺序，删除效率更高。
    Agents.RemoveAllSwap([Agent](const TWeakObjectPtr<ACrowdAgent>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Agent;
    });
}

void UCrowdPopulationSubsystem::ClearPopulation()
{
    SetSharedInput(FVector2D::ZeroVector);
    // 先移走注册表再 Destroy，避免每个 Agent::EndPlay 回调修改正在遍历的同一数组。
    const TArray<TWeakObjectPtr<ACrowdAgent>> PreviousAgents = MoveTemp(Agents);
    Agents.Reset();
    for (const TWeakObjectPtr<ACrowdAgent>& Agent : PreviousAgents)
    {
        if (Agent.IsValid())
        {
            Agent->Destroy();
        }
    }
}
