#pragma once

// 对区域内群体成员持续施加定向速度，可作为传送带、风场或水流使用。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJFlowZone.generated.h"

class AGGJCharacterGroupManager;
class AGGJPhysicalAnimationCharacter;
class UArrowComponent;
class UBoxComponent;

UENUM(BlueprintType)
enum class EGGJFlowZoneMode : uint8
{
    TargetVelocity UMETA(DisplayName="传送带：趋近目标速度"),
    Acceleration UMETA(DisplayName="风力：持续加速")
};

/**
 * X/红色前向轴就是流动方向。旋转 Actor 可改变方向，Box Extent 或缩放工具可改变范围。
 * 不依赖角色碰撞预设，直接检测 GroupManager 中的人物，因此兼容 Physical Animation。
 */
UCLASS(Blueprintable, meta=(DisplayName="GGJ Flow Zone"))
class GGJ_GAMEDEMO_API AGGJFlowZone : public AActor
{
    GENERATED_BODY()

public:
    AGGJFlowZone();

    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    /** 当前 Actor 朝向对应的实际世界作用方向。 */
    UFUNCTION(BlueprintPure, Category="Gameplay Zone|Flow")
    FVector GetFlowDirection() const;

    /** 蓝图可以单独对某个人调用，与区域自动检测使用相同的速度规则。 */
    UFUNCTION(BlueprintCallable, Category="Gameplay Zone|Flow")
    void ApplyFlowToCharacter(AGGJPhysicalAnimationCharacter* Character, float DeltaSeconds);

    /** 本帧检测到的群体人数；PIE 时可在 Details 或蓝图中检查。 */
    UFUNCTION(BlueprintPure, Category="Gameplay Zone|Debug")
    int32 GetAffectedCount() const { return AffectedCount; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UBoxComponent> FlowBounds;

    /** 编辑器中始终显示，用于预览 Actor 的正 X 流动方向。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UArrowComponent> DirectionArrow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Flow")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Flow")
    EGGJFlowZoneMode FlowMode = EGGJFlowZoneMode::TargetVelocity;

    /** 传送带的目标速度，也是风力模式沿作用方向的最高速度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Flow",
        meta=(ClampMin="0.0", Units="cm/s"))
    float FlowSpeed = 650.f;

    /** 速度接近目标值的快慢；数值越大，人物越快被带走。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Flow",
        meta=(ClampMin="0.0", Units="cm/s^2"))
    float Acceleration = 1600.f;

    /** 默认只使用水平旋转；开启后可制作向上或向下的风场。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Flow")
    bool bUseFull3DDirection = false;

    /** 关闭后，已经离地或跌落中的人物不会被区域继续推动。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Flow")
    bool bAffectFallingCharacters = true;

    /** 组件的未缩放半尺寸；也可以直接用关卡缩放工具再次整体缩放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Bounds",
        meta=(ClampMin="10.0", Units="cm"))
    FVector ZoneExtent = FVector(400.f, 400.f, 180.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Bounds",
        meta=(ClampMin="0.0", Units="cm"))
    float DetectionPadding = 25.f;

    /** 开启后箭头在游戏运行画面中也可见，方便调试；发布时建议关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Debug")
    bool bShowDirectionInGame = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Debug",
        meta=(ClampMin="20.0", Units="cm"))
    float DebugArrowLength = 360.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Debug")
    FLinearColor DebugArrowColor = FLinearColor(0.1f, 0.85f, 1.f, 1.f);

    /** 检测到人物时箭头使用该颜色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Debug")
    FLinearColor ActiveDebugArrowColor = FLinearColor(0.2f, 1.f, 0.35f, 1.f);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Runtime")
    int32 AffectedCount = 0;

protected:
    virtual void BeginPlay() override;

private:
    AGGJCharacterGroupManager* ResolveManager();

    UPROPERTY(Transient)
    TWeakObjectPtr<AGGJCharacterGroupManager> GroupManager;
};
