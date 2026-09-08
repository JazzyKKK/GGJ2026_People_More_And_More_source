#pragma once

// 独立、可自由放置的像素风气流特效。只负责显示，不检测角色，也不施加任何力。

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GGJPixelWindVFX.generated.h"

class UArrowComponent;
class UBoxComponent;

/** [PIXEL_WIND_VFX] 实际向渲染线程提交像素点的轻量组件。 */
UCLASS(NotBlueprintable)
class GGJ_GAMEDEMO_API UGGJPixelWindRenderComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    UGGJPixelWindRenderComponent();

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

    // 以下值由 AGGJPixelWindVFX 在构造阶段同步，蓝图请调 Actor 上同名中文参数。
    FVector EffectExtent = FVector(250.f, 250.f, 500.f);
    FLinearColor PrimaryColor = FLinearColor(0.42f, 0.9f, 1.f, 0.78f);
    FLinearColor SecondaryColor = FLinearColor(0.85f, 1.f, 1.f, 0.52f);
    int32 ParticleCount = 96;
    float RiseSpeed = 360.f;
    float PixelSize = 6.f;
    float GridSize = 18.f;
    float Turbulence = 28.f;
    float StreakRatio = 0.18f;
    int32 RandomSeed = 1337;
};

/**
 * [PIXEL_WIND_VFX] 独立像素气流 Actor。
 * 本地 +Z 是粒子流动方向；旋转 Actor 即可制作斜风或横向风。
 */
UCLASS(Blueprintable, meta=(DisplayName="Pixel Wind VFX"))
class GGJ_GAMEDEMO_API AGGJPixelWindVFX : public AActor
{
    GENERATED_BODY()

public:
    AGGJPixelWindVFX();
    virtual void OnConstruction(const FTransform& Transform) override;

    /** [PIXEL_WIND_VFX] 编辑器线框范围；游戏中自动隐藏。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="像素气流|组件")
    TObjectPtr<UBoxComponent> PreviewBounds;

    /** [PIXEL_WIND_VFX] 本地 +Z 流动方向预览；游戏中自动隐藏。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="像素气流|组件")
    TObjectPtr<UArrowComponent> DirectionArrow;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="像素气流|组件")
    TObjectPtr<UGGJPixelWindRenderComponent> PixelRenderer;

    /** 特效未缩放半尺寸；也可以直接使用关卡缩放工具整体缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|范围",
        meta=(ClampMin="10.0", Units="cm"))
    FVector EffectExtent = FVector(250.f, 250.f, 500.f);

    /** 同时存在的像素数量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|外观",
        meta=(ClampMin="8", ClampMax="512"))
    int32 ParticleCount = 96;

    /** 像素沿本地 +Z 上升的视觉速度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|运动",
        meta=(ClampMin="1.0", Units="cm/s"))
    float RiseSpeed = 360.f;

    /** 普通像素在屏幕上的方块边长。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|外观",
        meta=(ClampMin="1.0", ClampMax="32.0"))
    float PixelSize = 6.f;

    /** 世界空间跳格间距；越大，像素风的阶梯运动越明显。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|运动",
        meta=(ClampMin="1.0", Units="cm"))
    float GridSize = 18.f;

    /** 左右摆动幅度，只影响视觉。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|运动",
        meta=(ClampMin="0.0", Units="cm"))
    float Turbulence = 28.f;

    /** 高速亮点比例；这部分像素更亮、更大、移动更快。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|外观",
        meta=(ClampMin="0.0", ClampMax="1.0"))
    float StreakRatio = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|颜色")
    FLinearColor PrimaryColor = FLinearColor(0.42f, 0.9f, 1.f, 0.78f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|颜色")
    FLinearColor SecondaryColor = FLinearColor(0.85f, 1.f, 1.f, 0.52f);

    /** 改变它可以快速获得不同但稳定的粒子分布。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素气流|外观")
    int32 RandomSeed = 1337;

private:
    /** [PIXEL_WIND_VFX_PARAMS] 将 Actor 参数一次性同步给渲染组件。 */
    void SynchronizeRenderer();
};
