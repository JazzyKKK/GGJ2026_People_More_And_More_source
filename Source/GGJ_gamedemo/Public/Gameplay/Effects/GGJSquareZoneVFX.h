#pragma once

// 轻量方形区域提示特效：在矩形四边绘制发光边界和循环上升的方块粒子。
// 不包含碰撞、计数或触发逻辑，可安全附加在任意检测区域上。

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "GGJSquareZoneVFX.generated.h"

/**
 * 方形区域提示的运行时渲染组件。
 * 参数由拥有它的检测区域同步；关卡设计者通常只需要调整 CountZone 上的公开参数。
 */
UCLASS(NotBlueprintable, ClassGroup=(Rendering))
class GGJ_GAMEDEMO_API UGGJSquareZoneVFXComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    UGGJSquareZoneVFXComponent();

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

    /** 本地空间下方形边界的半尺寸。 */
    FVector2D ZoneExtent = FVector2D(350.f, 350.f);

    /** 粒子从边界平面向上升起的高度。 */
    float RiseHeight = 190.f;

    /** 粒子向上运动速度，单位厘米/秒。 */
    float RiseSpeed = 90.f;

    /** 粒子沿四边环绕运动速度，单位厘米/秒。 */
    float OrbitSpeed = 65.f;

    /** 是否将粒子尺寸解释为世界空间厘米；开启后镜头拉远时屏幕上的粒子会自然变小。 */
    bool bUseWorldSpaceParticleSize = true;

    /** 世界空间中普通方块的近似边长，单位厘米。 */
    float WorldParticleSize = 22.f;

    /** 关闭世界空间模式后使用的固定屏幕像素边长。 */
    float PixelSize = 7.f;

    /** 地面方形边框的线宽。 */
    float BoundaryThickness = 4.f;

    int32 ParticleCount = 72;
    int32 RandomSeed = 2026;
    float LargeParticleRatio = 0.2f;
    bool bDrawBoundary = true;
    FLinearColor PrimaryColor = FLinearColor(0.12f, 0.72f, 1.f, 0.9f);
    FLinearColor SecondaryColor = FLinearColor(0.55f, 0.95f, 1.f, 0.7f);
};
