#pragma once

// 轨道相机的蓝图可调参数。
// 这是纯数据结构，ACrowdCameraPawn 负责把它转换为实际相机位置和旋转。

#include "CoreMinimal.h"
#include "CrowdCameraSettings.generated.h"

/** 一套可按蓝图或按关卡覆盖的透视轨道相机配置。 */
USTRUCT(BlueprintType)
struct GGJ_GAMEDEMO_API FCrowdCameraSettings
{
    GENERATED_BODY()

    /** 相机到观察中心的距离（厘米）；越大可看到的场地越多。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="View", meta=(ClampMin="200", ClampMax="50000", Units="cm"))
    float OrbitDistance = 3200.f;

    /** 相机俯仰角；负值表示从上方斜看，当前范围避免完全水平或完全垂直。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="View", meta=(ClampMin="-85", ClampMax="-15", Units="deg"))
    float Pitch = -50.f;

    /** 开局环绕中心的水平角度（度）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="View", meta=(Units="deg"))
    float InitialYaw = -45.f;

    /** 透视相机视野角；越小越接近长焦，越大透视感越强。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="View", meta=(ClampMin="20", ClampMax="100", Units="deg"))
    float FieldOfView = 50.f;

    /** 每次 Q/E 转动 90° 所需的时间（秒）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transition", meta=(ClampMin="0.1", ClampMax="5", Units="s"))
    float TurnDuration = 0.65f;

    /** 缓动指数；大于 1 才有明确加速/减速，越大两端越慢、中段越快。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Transition", meta=(ClampMin="1.1", ClampMax="5"))
    float EaseExponent = 2.5f;

    /** 修复非有限值并限制编辑器外部传入的数据；返回副本，不修改原结构体。 */
    FCrowdCameraSettings Sanitized() const
    {
        FCrowdCameraSettings Result = *this;
        // IsFinite 防止 NaN 绕过 Clamp；InitialYaw 用 Unwind 归一到 [-180, 180]。
        Result.OrbitDistance = FMath::Clamp(FMath::IsFinite(OrbitDistance) ? OrbitDistance : 3200.f, 200.f, 50000.f);
        Result.Pitch = FMath::Clamp(FMath::IsFinite(Pitch) ? Pitch : -50.f, -85.f, -15.f);
        Result.InitialYaw = FMath::UnwindDegrees(FMath::IsFinite(InitialYaw) ? InitialYaw : -45.f);
        Result.FieldOfView = FMath::Clamp(FMath::IsFinite(FieldOfView) ? FieldOfView : 50.f, 20.f, 100.f);
        Result.TurnDuration = FMath::Clamp(FMath::IsFinite(TurnDuration) ? TurnDuration : 0.65f, 0.1f, 5.f);
        Result.EaseExponent = FMath::Clamp(FMath::IsFinite(EaseExponent) ? EaseExponent : 2.5f, 1.1f, 5.f);
        return Result;
    }
};
