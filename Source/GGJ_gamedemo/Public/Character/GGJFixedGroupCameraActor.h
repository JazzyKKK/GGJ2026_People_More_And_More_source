#pragma once

// 关卡设计师自由摆放的固定群体相机；位置和旋转在运行时保持不变。

#include "CoreMinimal.h"
#include "Character/GGJGroupCameraActor.h"
#include "GGJFixedGroupCameraActor.generated.h"

/**
 * 固定场景相机。
 * 选中 Actor 时 UE 编辑器会显示 CameraComponent 的实时预览，也可以使用 Pilot 调整构图。
 * WASD 仍由基类 ScreenInputToWorld 根据此相机的实际朝向转换到水平地面。
 */
UCLASS(Blueprintable, ClassGroup=(Camera), meta=(DisplayName="GGJ Fixed Group Camera"))
class GGJ_GAMEDEMO_API AGGJFixedGroupCameraActor : public AGGJGroupCameraActor
{
    GENERATED_BODY()

public:
    AGGJFixedGroupCameraActor();

    virtual void ActivateForPlayer(APlayerController* PlayerController,
        AGGJCharacterGroupManager* NewGroupManager, AActor* InitialTarget) override;

    /** 固定模式忽略角色上的 Q/E 转向输入。 */
    virtual bool RequestQuarterTurn(int32 Direction) override;

    virtual FVector ScreenInputToWorld(FVector2D ScreenInput) const override;

    /** 常见方向错误直接改这个值：90、-90 或 180，不需要修改代码。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fixed Camera|Input",
        meta=(ClampMin="-180.0", ClampMax="180.0", Units="deg"))
    float InputYawOffset = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fixed Camera|Input")
    bool bInvertHorizontalInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fixed Camera|Input")
    bool bInvertVerticalInput = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fixed Camera|Input")
    bool bSwapInputAxes = false;

    /**
     * 蓝图可覆写的最终方向接口。CameraRelativeDirection 已经过上面四个参数校正；
     * 返回任意水平世界向量即可完全自定义人物的移动方向。
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Fixed Camera|Input")
    FVector AdjustMovementDirection(FVector CameraRelativeDirection,
        FVector2D RawScreenInput) const;
    virtual FVector AdjustMovementDirection_Implementation(FVector CameraRelativeDirection,
        FVector2D RawScreenInput) const;

    /** 切入固定视角所需时间；0 表示立即切换。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fixed Camera",
        meta=(ClampMin="0.0", ClampMax="5.0", Units="s"))
    float ViewBlendTime = 0.f;

protected:
    virtual void BeginPlay() override;
};
