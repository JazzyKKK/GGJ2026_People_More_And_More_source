#pragma once

// 可在蓝图覆写判定规则、并在场景上方显示人数状态的计数区域。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJCountZone.generated.h"

class UBoxComponent;
class UTextRenderComponent;
class UGGJSquareZoneVFXComponent;
class AGGJCharacterGroupManager;

UENUM(BlueprintType)
enum class EGGJCountComparison : uint8
{
    Equal UMETA(DisplayName="等于 ="),
    NotEqual UMETA(DisplayName="不等于 !="),
    Greater UMETA(DisplayName="大于 >"),
    GreaterOrEqual UMETA(DisplayName="大于等于 >="),
    Less UMETA(DisplayName="小于 <"),
    LessOrEqual UMETA(DisplayName="小于等于 <="),
    Custom UMETA(DisplayName="蓝图自定义")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGGJCountZoneTriggeredSignature, bool, bTriggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGGJCountZoneUpdatedSignature,
    int32, InsidePopulation, int32, ScenePopulation);

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJCountZone : public AActor
{
    GENERATED_BODY()

public:
    AGGJCountZone();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    /**
     * 蓝图子类可 Override 此函数并返回自己的规则。
     * 默认根据 Comparison 比较区域内人数与 TargetCount。
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Gameplay Zone|Count")
    bool EvaluateCountCondition(int32 InsideCount, int32 TotalCount, int32 Target) const;
    virtual bool EvaluateCountCondition_Implementation(int32 InsideCount,
        int32 TotalCount, int32 Target) const;

    UFUNCTION(BlueprintPure, Category="Gameplay Zone|Count")
    bool IsTriggered() const { return bIsTriggered; }

    UFUNCTION(BlueprintPure, Category="Gameplay Zone|Count")
    int32 GetInsidePopulation() const { return InsidePopulation; }

    UFUNCTION(BlueprintPure, Category="Gameplay Zone|Count")
    int32 GetScenePopulation() const { return ScenePopulation; }

    /** 手动立即刷新，通常无需调用，区域默认每帧自动刷新。 */
    UFUNCTION(BlueprintCallable, Category="Gameplay Zone|Count")
    void RefreshCountState();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Count", meta=(ClampMin="0"))
    int32 TargetCount = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Count")
    EGGJCountComparison Comparison = EGGJCountComparison::GreaterOrEqual;

    /** Comparison=Custom 时显示在数字之间的短文本，例如 x2、奇数、ALL。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Count")
    FText CustomComparisonLabel = FText::FromString(TEXT("?"));

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Count")
    bool bEnabled = true;

    /** 在 Box 边界外增加少量检测容差；检测角色 Actor 原点，不依赖碰撞事件。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Count",
        meta=(ClampMin="0.0", Units="cm"))
    float DetectionPadding = 40.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Runtime")
    int32 InsidePopulation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Runtime")
    int32 ScenePopulation = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Runtime")
    bool bIsTriggered = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UBoxComponent> CountBounds;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UTextRenderComponent> StatusText;

    /** 与人数状态完全独立的附加说明文字，例如“至少十人”或“出口供能”。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UTextRenderComponent> CustomFloatingText;

    /** 方形边界与方块粒子提示，只负责表现，不参与人数检测。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UGGJSquareZoneVFXComponent> SquareZoneVFX;

    /** 是否在游戏中显示方形区域提示特效。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX")
    bool bShowSquareZoneVFX = true;

    /** 是否绘制贴合检测范围的方形地面边框。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX"))
    bool bShowSquareBoundary = true;

    /** 特效相对于检测区域中心的高度；用于贴合实际地面。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX", Units="cm"))
    float SquareVFXHeightOffset = 5.f;

    /** 同时显示的方块粒子数量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX", ClampMin="4", ClampMax="512"))
    int32 SquareParticleCount = 72;

    /** 方块粒子向上升起的最大高度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX", ClampMin="10.0", Units="cm"))
    float SquareParticleRiseHeight = 190.f;

    /** 方块粒子的上升速度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX", ClampMin="1.0", Units="cm/s"))
    float SquareParticleRiseSpeed = 90.f;

    /** 方块粒子沿区域四边循环运动的速度；设为 0 时只会上升。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX", Units="cm/s"))
    float SquareParticleOrbitSpeed = 65.f;

    /**
     * 使用世界空间粒子尺寸。开启后方块在场景中的实际大小稳定，
     * 镜头拉远时会在屏幕中自然缩小，避免大量粒子糊在一起。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX"))
    bool bUseWorldSpaceSquareParticleSize = true;

    /** 世界空间中方块粒子的近似边长。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX && bUseWorldSpaceSquareParticleSize", ClampMin="0.5", ClampMax="200.0", Units="cm"))
    float SquareParticleWorldSize = 22.f;

    /** 固定屏幕像素大小；仅在关闭世界空间尺寸时使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX && !bUseWorldSpaceSquareParticleSize", ClampMin="1.0", ClampMax="40.0", EditConditionHides))
    float SquareParticleSize = 7.f;

    /** 较大方块在全部粒子中的比例。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX", ClampMin="0.0", ClampMax="1.0"))
    float SquareLargeParticleRatio = 0.2f;

    /** 方形地面边框线宽。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX && bShowSquareBoundary", ClampMin="0.5", ClampMax="24.0"))
    float SquareBoundaryThickness = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX"))
    FLinearColor SquareWaitingColor = FLinearColor(0.12f, 0.72f, 1.f, 0.9f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX"))
    FLinearColor SquareActiveColor = FLinearColor(1.f, 0.5f, 0.08f, 0.95f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX"))
    FLinearColor SquareSuccessColor = FLinearColor(0.15f, 1.f, 0.42f, 0.95f);

    /** 改变随机种子可获得不同但稳定的方块分布。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Square VFX",
        meta=(EditCondition="bShowSquareZoneVFX"))
    int32 SquareParticleRandomSeed = 2026;

    /** 蓝图在运行时修改范围或外观参数后，可调用此函数立即刷新特效。 */
    UFUNCTION(BlueprintCallable, Category="Gameplay Zone|Square VFX")
    void RefreshSquareZoneVFX();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual", meta=(Units="cm"))
    float TextHeight = 260.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual",
        meta=(ClampMin="10.0", Units="cm"))
    float TextWorldSize = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual")
    FLinearColor WaitingColor = FLinearColor(0.85f, 0.85f, 0.9f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual")
    FLinearColor ActiveColor = FLinearColor(1.f, 0.68f, 0.12f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual")
    FLinearColor SuccessColor = FLinearColor(0.18f, 1.f, 0.48f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual")
    bool bFacePlayerCamera = true;

    /** 是否显示独立的自定义悬浮文字；不会隐藏或修改原有状态文字。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Custom Text")
    bool bShowCustomFloatingText = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Custom Text",
        meta=(MultiLine="true", EditCondition="bShowCustomFloatingText"))
    FText CustomFloatingTextContent = FText::FromString(TEXT("区域说明"));

    /** 相对于检测区域中心的位置，可把说明文字放在状态数字上方、下方或侧面。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Custom Text",
        meta=(EditCondition="bShowCustomFloatingText", Units="cm"))
    FVector CustomFloatingTextOffset = FVector(0.f, 0.f, 400.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Custom Text",
        meta=(EditCondition="bShowCustomFloatingText", ClampMin="10.0", Units="cm"))
    float CustomFloatingTextWorldSize = 65.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Custom Text",
        meta=(EditCondition="bShowCustomFloatingText"))
    FLinearColor CustomFloatingTextColor = FLinearColor(0.92f, 0.95f, 1.f, 1.f);

    /** 可单独决定说明文字是否朝向玩家相机。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Custom Text",
        meta=(EditCondition="bShowCustomFloatingText"))
    bool bCustomTextFacesPlayerCamera = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual",
        meta=(ClampMin="0.0", ClampMax="0.5"))
    float SuccessPulseAmplitude = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Visual",
        meta=(ClampMin="0.0", ClampMax="20.0"))
    float SuccessPulseSpeed = 4.f;

    UPROPERTY(BlueprintAssignable, Category="Gameplay Zone|Events")
    FGGJCountZoneTriggeredSignature OnTriggeredChanged;

    UPROPERTY(BlueprintAssignable, Category="Gameplay Zone|Events")
    FGGJCountZoneUpdatedSignature OnCountUpdated;

protected:
    virtual void BeginPlay() override;

private:
    AGGJCharacterGroupManager* ResolveManager();
    FText GetComparisonLabel() const;
    void UpdateVisual(float DeltaSeconds);
    void SynchronizeSquareZoneVFX(bool bForceRefresh = false);

    UPROPERTY(Transient)
    TWeakObjectPtr<AGGJCharacterGroupManager> GroupManager;

    float VisualTime = 0.f;
    FVector2D LastSquareVFXExtent = FVector2D::ZeroVector;
    FLinearColor LastSquareVFXColor = FLinearColor::Transparent;
    bool bLastSquareVFXVisible = false;
};
