#pragma once

// 当前 World 的“人群数据中心”。
// 它不负责关卡规则，只保存小人注册表和全体共用输入，并提供唯一的安全生成入口。

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Crowd/Data/CrowdAgentPhysicsData.h"
#include "CrowdPopulationSubsystem.generated.h"

class ACrowdAgent;

/**
 * 每个游戏/PIE World 各有一份的轻量人口子系统。
 *
 * 生命周期由 UE 自动管理，因此关卡重开或 PIE 结束时不会把另一个 World 的小人混进来。
 * Controller 写入 SharedInput，MovementComponent 读取它，LevelDirector 调用 Spawn/Clear。
 */
UCLASS()
class GGJ_GAMEDEMO_API UCrowdPopulationSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** 只为真正的 Game 和 PIE 世界创建；资源预览等编辑器 World 不需要人口系统。 */
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

    /** World 销毁时丢弃输入与弱引用；Actor 的销毁仍由 World 自己完成。 */
    virtual void Deinitialize() override;

    /** 返回当前仍有效的小人数，不把已经销毁但尚未清理的弱引用计算进去。 */
    UFUNCTION(BlueprintPure, Category="Crowd")
    int32 GetPopulationCount() const;

    /** 获取已经限制在单位圆内的世界 XY 输入。 */
    UFUNCTION(BlueprintPure, Category="Crowd")
    FVector2D GetSharedInput() const { return SharedInput; }

    // 修改接口保持为 Native-only：Controller 写输入，Director 管人口，避免任意蓝图破坏所有权。
    /** 写入全体共享输入；非法值归零，斜向输入限制到长度 1，手柄小幅模拟量会保留。 */
    void SetSharedInput(FVector2D Input);

    /** 防重叠检查并延迟生成一个小人；成功返回实例，失败返回 nullptr。 */
    ACrowdAgent* SpawnAgent(TSubclassOf<ACrowdAgent> AgentClass, const FVector& Location,
        const FCrowdPhysicsSettings& Settings);

    /** 小人 EndPlay 时注销自己，同时顺手删除表中的失效弱引用。 */
    void UnregisterAgent(ACrowdAgent* Agent);

    /** 清空共享输入并销毁所有已注册小人，用于重开关卡。 */
    void ClearPopulation();

    /** 只读访问弱引用数组，供原生规则系统枚举；调用者必须检查每项 IsValid。 */
    const TArray<TWeakObjectPtr<ACrowdAgent>>& GetAgents() const { return Agents; }

private:
    /** 当前帧所有小人共同使用的世界 XY 方向/强度。 */
    FVector2D SharedInput = FVector2D::ZeroVector;

    /** 弱引用不会阻止 Actor 被垃圾回收，也不会形成由 Subsystem 持有 Actor 的生命周期环。 */
    TArray<TWeakObjectPtr<ACrowdAgent>> Agents;
};
