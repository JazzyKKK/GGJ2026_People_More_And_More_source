#pragma once

// 当前 Physical Character 群体专用的斜俯视自适应相机。
// 相机只负责观察，不占有角色；PlayerController 仍然占有 Leader 并接收移动输入。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/KismetMathLibrary.h"
#include "GGJGroupCameraActor.generated.h"

class APlayerController;
class AGGJCharacterGroupManager;
class AGGJPhysicalAnimationCharacter;
class UCameraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGGJGroupCameraRotationEvent);

/**
 * 固定俯角、可按 Q/E 绕群体旋转、自动容纳全部成员的透视相机。
 * 焦点取成员位置的平均值（近似质心），距离由群体包围球和当前 FOV 自动计算。
 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJGroupCameraActor : public AActor
{
    GENERATED_BODY()

public:
    AGGJGroupCameraActor();

    virtual void Tick(float DeltaSeconds) override;

    /** GameMode 调用：绑定群体、设置初始目标并把 Controller 的视图切到本相机。 */
    UFUNCTION(BlueprintCallable, Category="Group Camera")
    virtual void ActivateForPlayer(APlayerController* PlayerController,
        AGGJCharacterGroupManager* GroupManager, AActor* InitialTarget);

    /** Direction>0 为 Q（画面向左），Direction<0 为 E（画面向右）；旋转中不接受下一次请求。 */
    UFUNCTION(BlueprintCallable, Category="Group Camera")
    virtual bool RequestQuarterTurn(int32 Direction);

    /** 把屏幕输入转换为当前相机方向下的地面世界方向。 */
    UFUNCTION(BlueprintPure, Category="Group Camera")
    virtual FVector ScreenInputToWorld(FVector2D ScreenInput) const;

    UFUNCTION(BlueprintPure, Category="Group Camera")
    float GetCurrentYaw() const { return CurrentYaw; }

    UFUNCTION(BlueprintPure, Category="Group Camera")
    float GetCurrentDistance() const { return CurrentDistance; }

    UFUNCTION(BlueprintPure, Category="Group Camera")
    FVector GetCurrentFocus() const { return CurrentFocus; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Group Camera|Components")
    TObjectPtr<UCameraComponent> Camera;

    /** 固定斜俯视角。负值表示从上方看向地面。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|View",
        meta=(ClampMin="-85.0", ClampMax="-20.0", Units="deg"))
    float Pitch = -50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|View", meta=(Units="deg"))
    float InitialYaw = -45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|View",
        meta=(ClampMin="20.0", ClampMax="100.0", Units="deg"))
    float FieldOfView = 50.f;

    /** 相机不会比它更近，避免三人聚在一起时画面贴脸。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Framing",
        meta=(ClampMin="200.0", Units="cm"))
    float MinDistance = 1800.f;

    /** 防止某个掉出关卡的小人把相机拉到无限远。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Framing",
        meta=(ClampMin="500.0", Units="cm"))
    float MaxDistance = 9000.f;

    /** 每个人按这个半径参与包围计算，应略大于角色最高/最宽尺寸的一半。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Framing",
        meta=(ClampMin="0.0", Units="cm"))
    float MemberBoundsRadius = 140.f;

    /** 画面边缘额外留白，也为平滑跟随产生的少量滞后提供安全余量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Framing",
        meta=(ClampMin="0.0", Units="cm"))
    float FramingPadding = 320.f;

    /** 在角色 Actor 原点之上观察，通常设为人物胸口附近。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Framing", meta=(Units="cm"))
    float FocusHeightOffset = 55.f;

    /** 越大越紧跟质心；临界阻尼保证加速后减速且不来回振荡。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Smoothing",
        meta=(ClampMin="1.0", ClampMax="200.0"))
    float FocusSpringStiffness = 55.f;

    /** 越大越快适应群体展开/收拢。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Smoothing",
        meta=(ClampMin="1.0", ClampMax="200.0"))
    float ZoomSpringStiffness = 45.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Smoothing",
        meta=(ClampMin="0.1", ClampMax="2.0"))
    float CriticalDamping = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Rotation",
        meta=(ClampMin="0.1", ClampMax="5.0", Units="s"))
    float QuarterTurnDuration = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Camera|Rotation",
        meta=(ClampMin="1.1", ClampMax="5.0"))
    float RotationEaseExponent = 2.5f;

    UPROPERTY(BlueprintAssignable, Category="Group Camera|Events")
    FGGJGroupCameraRotationEvent OnRotationStarted;

    UPROPERTY(BlueprintAssignable, Category="Group Camera|Events")
    FGGJGroupCameraRotationEvent OnRotationFinished;

protected:
    virtual void BeginPlay() override;

private:
    bool CalculateDesiredFrame(FVector& OutFocus, float& OutDistance) const;
    void ApplyCameraPose();
    void AdvanceRotation(float DeltaSeconds);

    UPROPERTY(Transient)
    TWeakObjectPtr<AGGJCharacterGroupManager> GroupManager;

    UPROPERTY(Transient)
    TWeakObjectPtr<AActor> FallbackTarget;

    UPROPERTY(Transient)
    TWeakObjectPtr<APlayerController> OwningPlayer;

    FVector CurrentFocus = FVector::ZeroVector;
    float CurrentDistance = 1800.f;
    float CurrentYaw = -45.f;
    float StartYaw = -45.f;
    float TargetYaw = -45.f;
    float RotationElapsed = 0.f;
    bool bViewInitialized = false;
    bool bRotating = false;
    FVectorSpringState FocusSpringState;
    FFloatSpringState ZoomSpringState;
};
