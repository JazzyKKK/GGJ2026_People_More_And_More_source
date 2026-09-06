#pragma once

// 本文件定义“小人刚体参数”及其 Data Asset 包装。
// 关卡与蓝图只配置数据；真正应用这些数据的是 ACrowdAgent 和移动组件。

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CrowdAgentPhysicsData.generated.h"

/**
 * 单个小人的共享物理参数。
 *
 * 结构体使用 BlueprintType，因此可以直接显示在蓝图 Class Defaults 和 Data Asset 中。
 * 碰撞尺寸只由 Radius 决定；VisualMesh/HeadMesh 只是外观，不能参与游戏碰撞。
 */
USTRUCT(BlueprintType)
struct GGJ_GAMEDEMO_API FCrowdPhysicsSettings
{
    GENERATED_BODY()

    /** 球形刚体半径（厘米），同时用于出生点防重叠和自动阵列安全间距。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin="8", ClampMax="100"))
    float Radius = 30.f;

    /** Chaos 刚体质量（千克）；驱动采用“加速度模式”，所以改质量主要影响互相推挤。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin="0.1"))
    float MassKg = 1.f;

    /** 有输入时施加在 XY 平面上的加速度，单位约为 cm/s²。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0"))
    float DriveAcceleration = 1800.f;

    /** Chaos 线性阻尼；数值越大，松开输入后越快停下。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="0"))
    float LinearDamping = 6.f;

    /** 主动驱动的软速度上限；碰撞造成的瞬时超速不会被硬裁剪。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Movement", meta=(ClampMin="1"))
    float SoftSpeedLimit = 300.f;

    /** 返回经过有限值检查和安全范围限制的副本，避免 NaN/非法蓝图数据进入物理场景。 */
    FCrowdPhysicsSettings Sanitized() const;
};

/**
 * 可复用的物理参数资产。
 * 在内容浏览器创建其蓝图子类/实例后，可让多个关卡的 Director 共用同一套手感。
 */
UCLASS(BlueprintType)
class GGJ_GAMEDEMO_API UCrowdAgentPhysicsData : public UDataAsset
{
    GENERATED_BODY()

public:
    /** 编辑器中可调的完整物理参数；游戏运行时会复制并净化后使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crowd")
    FCrowdPhysicsSettings Settings;
};
