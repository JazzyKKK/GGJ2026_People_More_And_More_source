#pragma once

// 可在蓝图覆写判定规则、并在场景上方显示人数状态的计数区域。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJCountZone.generated.h"

class UBoxComponent;
class UTextRenderComponent;
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

    UPROPERTY(Transient)
    TWeakObjectPtr<AGGJCharacterGroupManager> GroupManager;

    float VisualTime = 0.f;
};
