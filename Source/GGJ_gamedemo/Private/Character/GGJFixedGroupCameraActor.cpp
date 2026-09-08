#include "Character/GGJFixedGroupCameraActor.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

AGGJFixedGroupCameraActor::AGGJFixedGroupCameraActor()
{
    // 基类负责自适应跟随；固定相机关闭 Tick，完全采用编辑器中的 Actor Transform。
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    if (Camera)
    {
        Camera->SetRelativeLocation(FVector::ZeroVector);
        Camera->SetRelativeRotation(FRotator::ZeroRotator);
    }
}

void AGGJFixedGroupCameraActor::BeginPlay()
{
    // 不调用 AGGJGroupCameraActor::BeginPlay，避免它按焦点、距离和俯角重写摆放结果。
    AActor::BeginPlay();
    // 虽然固定相机跳过基类视角初始化，仍需继承同一套遮挡轮廓后处理功能。
    RefreshOcclusionOutlinePostProcess();
    SetActorTickEnabled(false);
}

void AGGJFixedGroupCameraActor::ActivateForPlayer(APlayerController* PlayerController,
    AGGJCharacterGroupManager* NewGroupManager, AActor* InitialTarget)
{
    if (!PlayerController)
    {
        return;
    }

    if (ViewBlendTime > 0.f)
    {
        PlayerController->SetViewTargetWithBlend(this, ViewBlendTime,
            EViewTargetBlendFunction::VTBlend_EaseInOut, 2.f);
    }
    else
    {
        PlayerController->SetViewTarget(this);
    }
}

bool AGGJFixedGroupCameraActor::RequestQuarterTurn(const int32 Direction)
{
    return false;
}

FVector AGGJFixedGroupCameraActor::ScreenInputToWorld(const FVector2D ScreenInput) const
{
    FVector2D AdjustedInput = ScreenInput;
    if (bSwapInputAxes)
    {
        Swap(AdjustedInput.X, AdjustedInput.Y);
    }
    if (bInvertHorizontalInput)
    {
        AdjustedInput.X *= -1.f;
    }
    if (bInvertVerticalInput)
    {
        AdjustedInput.Y *= -1.f;
    }

    FVector Direction = Super::ScreenInputToWorld(AdjustedInput);
    Direction = Direction.RotateAngleAxis(InputYawOffset, FVector::UpVector);
    Direction = AdjustMovementDirection(Direction, ScreenInput);
    if (Direction.ContainsNaN())
    {
        return FVector::ZeroVector;
    }
    Direction.Z = 0.f;
    return Direction.GetClampedToMaxSize(1.f);
}

FVector AGGJFixedGroupCameraActor::AdjustMovementDirection_Implementation(
    const FVector CameraRelativeDirection, const FVector2D RawScreenInput) const
{
    return CameraRelativeDirection;
}
