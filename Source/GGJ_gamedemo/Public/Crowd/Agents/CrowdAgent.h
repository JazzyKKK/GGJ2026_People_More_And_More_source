#pragma once

// 单个“同步小人”的 Actor 定义。
// Body 是唯一玩法碰撞体；VisualRoot 以下全部是可由蓝图替换的 3D 表现。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Crowd/Data/CrowdAgentPhysicsData.h"
#include "CrowdAgent.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UCrowdPhysicsMovementComponent;
class UPhysicalMaterial;
class UMaterialInterface;

/**
 * 人群中的一个物理个体。
 *
 * 小人不是 Character，也没有各自的 Controller。它通过 Movement 组件读取 World
 * 子系统中的共享输入，因此所有实例收到同一方向，却仍由 Chaos 独立计算挤压和碰撞。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API ACrowdAgent : public AActor
{
    GENERATED_BODY()

public:
    /** 创建默认物理球、占位身体/头部网格以及移动组件。 */
    ACrowdAgent();

    /** 编辑器放置或蓝图构造变化时，预览材质覆盖。 */
    virtual void OnConstruction(const FTransform& Transform) override;

    /** 蓝图构造结束后，由 PopulationSubsystem 应用最终物理参数。 */
    void InitializePhysics(const FCrowdPhysicsSettings& Settings);

    /** 确认初始化完成后才打开 PrePhysics 移动 Tick。 */
    void ActivateMovement();

    /** 停止施力并从人口子系统注销。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 根组件与唯一物理碰撞体；锁定 Z 位移和全部旋转。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd")
    TObjectPtr<USphereComponent> Body;

    // 可在子蓝图里替换或追加外观，但 VisualRoot 下所有组件都必须保持 NoCollision。
    /** 视觉组件总挂点；可在蓝图中整体调整模型偏移、朝向和缩放。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd|Presentation")
    TObjectPtr<USceneComponent> VisualRoot;

    /** 默认圆柱身体，占位用；它不定义碰撞。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd|Presentation")
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    /** 默认球形头部，占位用；完整人物模型可在蓝图里隐藏它。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd|Presentation")
    TObjectPtr<UStaticMeshComponent> HeadMesh;

    /** 非空时覆盖 VisualMesh 的第 0 材质槽。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crowd|Presentation")
    TObjectPtr<UMaterialInterface> BodyMaterial;

    /** 非空时覆盖 HeadMesh 的第 0 材质槽。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crowd|Presentation")
    TObjectPtr<UMaterialInterface> HeadMaterial;

    /** 读取共享输入并向 Body 施力的原生组件。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd")
    TObjectPtr<UCrowdPhysicsMovementComponent> Movement;

private:
    /** 把蓝图默认材质安全地应用到两个占位网格。 */
    void ApplyVisualOverrides();

    /** 每个实例独有的低弹性物理材质；Transient 表示不保存进资源。 */
    UPROPERTY(Transient)
    TObjectPtr<UPhysicalMaterial> RuntimeMaterial;
};
