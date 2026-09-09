// 群体质心跟随、自适应取景和 Q/E 四向观察的实现。
#include "Character/GGJGroupCameraActor.h"

#include "Camera/CameraComponent.h"
#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJPhysicalAnimationCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GGJOcclusionOutline
{
    const FName ColorParameter(TEXT("OutlineColor"));
    const FName WidthParameter(TEXT("OutlineWidth"));
    const FName IntensityParameter(TEXT("OutlineIntensity"));
    const FName DepthBiasParameter(TEXT("DepthBias"));
}

AGGJGroupCameraActor::AGGJGroupCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FocusRoot")));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("GroupCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->ProjectionMode = ECameraProjectionMode::Perspective;
    Camera->bConstrainAspectRatio = true;
    Camera->AspectRatio = 16.f / 9.f;
    Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    Camera->PostProcessSettings.MotionBlurAmount = 0.f;
    // 后处理材质直接挂在相机上，因此每张关卡无需重复放置无限范围 PostProcessVolume。
    Camera->PostProcessBlendWeight = 1.f;
}

void AGGJGroupCameraActor::BeginPlay()
{
    Super::BeginPlay();
    CurrentYaw = StartYaw = TargetYaw = FMath::UnwindDegrees(InitialYaw);
    CurrentDistance = FMath::Clamp(MinDistance, 200.f, FMath::Max(MinDistance, MaxDistance));
    RefreshOcclusionOutlinePostProcess();
    ApplyCameraPose();
}

void AGGJGroupCameraActor::SetOcclusionOutlineEnabled(const bool bEnabled)
{
    bOcclusionOutlineEnabled = bEnabled;
    RefreshOcclusionOutlinePostProcess();
}

void AGGJGroupCameraActor::SetOcclusionOutlineMaterial(UMaterialInterface* NewMaterial)
{
    OcclusionOutlineMaterial = NewMaterial;
    RefreshOcclusionOutlinePostProcess();
}

void AGGJGroupCameraActor::SetOcclusionOutlineBlendWeight(const float NewWeight)
{
    OcclusionOutlineBlendWeight = FMath::Clamp(NewWeight, 0.f, 1.f);
    RefreshOcclusionOutlinePostProcess();
}

void AGGJGroupCameraActor::SetOcclusionOutlineStyle(const FLinearColor NewColor,
    const float NewWidth, const float NewIntensity, const float NewDepthBias)
{
    OcclusionOutlineColor = NewColor;
    OcclusionOutlineWidth = FMath::Clamp(NewWidth, 0.f, 12.f);
    OcclusionOutlineIntensity = FMath::Clamp(NewIntensity, 0.f, 50.f);
    OcclusionOutlineDepthBias = FMath::Clamp(NewDepthBias, 0.f, 100.f);
    ApplyOcclusionOutlineMaterialParameters();
}

void AGGJGroupCameraActor::RefreshOcclusionOutlinePostProcess()
{
    if (!Camera)
    {
        return;
    }

    // 只删除上次由本类插入的条目，保留景深、调色等其他相机 Blendable。
    if (AppliedOcclusionOutlineBlendable)
    {
        Camera->PostProcessSettings.WeightedBlendables.Array.RemoveAll(
            [this](const FWeightedBlendable& Blendable)
            {
                return Blendable.Object == AppliedOcclusionOutlineBlendable;
            });
    }
    AppliedOcclusionOutlineBlendable = nullptr;
    OcclusionOutlineMID = nullptr;

    if (!bOcclusionOutlineEnabled || !OcclusionOutlineMaterial
        || OcclusionOutlineBlendWeight <= 0.f)
    {
        return;
    }

    OcclusionOutlineMID = UMaterialInstanceDynamic::Create(OcclusionOutlineMaterial, this);
    if (!OcclusionOutlineMID)
    {
        return;
    }

    ApplyOcclusionOutlineMaterialParameters();
    Camera->PostProcessSettings.WeightedBlendables.Array.Add(
        FWeightedBlendable(FMath::Clamp(OcclusionOutlineBlendWeight, 0.f, 1.f),
            OcclusionOutlineMID));
    AppliedOcclusionOutlineBlendable = OcclusionOutlineMID;
}

void AGGJGroupCameraActor::ApplyOcclusionOutlineMaterialParameters()
{
    if (!OcclusionOutlineMID)
    {
        return;
    }

    OcclusionOutlineMID->SetVectorParameterValue(
        GGJOcclusionOutline::ColorParameter, OcclusionOutlineColor);
    OcclusionOutlineMID->SetScalarParameterValue(
        GGJOcclusionOutline::WidthParameter, OcclusionOutlineWidth);
    OcclusionOutlineMID->SetScalarParameterValue(
        GGJOcclusionOutline::IntensityParameter, OcclusionOutlineIntensity);
    OcclusionOutlineMID->SetScalarParameterValue(
        GGJOcclusionOutline::DepthBiasParameter, OcclusionOutlineDepthBias);
}

void AGGJGroupCameraActor::ActivateForPlayer(APlayerController* PlayerController,
    AGGJCharacterGroupManager* NewGroupManager, AActor* InitialTarget)
{
    OwningPlayer = PlayerController;
    GroupManager = NewGroupManager;
    FallbackTarget = InitialTarget;

    FVector DesiredFocus;
    float DesiredDistance = MinDistance;
    if (CalculateDesiredFrame(DesiredFocus, DesiredDistance))
    {
        // 第一次显示前直接放到正确画面；此后所有运行中变化都经过临界阻尼弹簧。
        CurrentFocus = DesiredFocus;
        CurrentDistance = DesiredDistance;
        bViewInitialized = true;
        FocusSpringState.Reset();
        ZoomSpringState.Reset();
        ApplyCameraPose();
    }
    if (PlayerController)
    {
        PlayerController->SetViewTarget(this);
    }
}

void AGGJGroupCameraActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f) { return; }

    AdvanceRotation(DeltaSeconds);

    FVector DesiredFocus;
    float DesiredDistance = MinDistance;
    if (CalculateDesiredFrame(DesiredFocus, DesiredDistance))
    {
        if (!bViewInitialized)
        {
            CurrentFocus = DesiredFocus;
            CurrentDistance = DesiredDistance;
            bViewInitialized = true;
        }
        else
        {
            CurrentFocus = UKismetMathLibrary::VectorSpringInterp(CurrentFocus, DesiredFocus,
                FocusSpringState, FMath::Max(1.f, FocusSpringStiffness),
                FMath::Max(0.1f, CriticalDamping), DeltaSeconds, 1.f, 0.f);
            CurrentDistance = UKismetMathLibrary::FloatSpringInterp(CurrentDistance, DesiredDistance,
                ZoomSpringState, FMath::Max(1.f, ZoomSpringStiffness),
                FMath::Max(0.1f, CriticalDamping), DeltaSeconds, 1.f, 0.f);
        }
    }
    ApplyCameraPose();
}

bool AGGJGroupCameraActor::CalculateDesiredFrame(FVector& OutFocus, float& OutDistance) const
{
    TArray<AGGJPhysicalAnimationCharacter*> Members;
    GatherTrackedMembers(Members);

    if (Members.IsEmpty())
    {
        const AActor* Target = FallbackTarget.Get();
        if (!Target) { return false; }
        OutFocus = Target->GetActorLocation() + FVector::UpVector * FocusHeightOffset;
        OutDistance = FMath::Clamp(MinDistance, 200.f, FMath::Max(MinDistance, MaxDistance));
        return true;
    }

    FVector Center = FVector::ZeroVector;
    int32 ValidCount = 0;
    for (const AGGJPhysicalAnimationCharacter* Member : Members)
    {
        if (IsValid(Member))
        {
            Center += Member->GetActorLocation();
            ++ValidCount;
        }
    }
    if (ValidCount == 0) { return false; }

    Center /= static_cast<float>(ValidCount);
    OutFocus = Center + FVector::UpVector * FocusHeightOffset;

    float Radius = 0.f;
    for (const AGGJPhysicalAnimationCharacter* Member : Members)
    {
        if (IsValid(Member))
        {
            Radius = FMath::Max(Radius, FVector::Distance(Member->GetActorLocation(), Center));
        }
    }
    Radius += FMath::Max(0.f, MemberBoundsRadius) + FMath::Max(0.f, FramingPadding);

    const float SafeFov = FMath::Clamp(FieldOfView, 20.f, 100.f);
    const float HorizontalHalfFov = FMath::DegreesToRadians(SafeFov * 0.5f);
    const float Aspect = Camera && Camera->AspectRatio > UE_KINDA_SMALL_NUMBER
        ? Camera->AspectRatio : 16.f / 9.f;
    const float VerticalHalfFov = FMath::Atan(FMath::Tan(HorizontalHalfFov) / Aspect);
    const float LimitingHalfFov = FMath::Min(HorizontalHalfFov, VerticalHalfFov);
    const float FitDistance = Radius / FMath::Max(0.05f, FMath::Sin(LimitingHalfFov));
    const float SafeMax = FMath::Max(MinDistance, MaxDistance);
    OutDistance = FMath::Clamp(FitDistance, FMath::Max(200.f, MinDistance), SafeMax);
    return true;
}

void AGGJGroupCameraActor::GatherTrackedMembers(
    TArray<AGGJPhysicalAnimationCharacter*>& OutMembers) const
{
    OutMembers.Reset();
    if (const AGGJCharacterGroupManager* Manager = GroupManager.Get())
    {
        OutMembers = Manager->GetMembers();
    }
}

bool AGGJGroupCameraActor::RequestQuarterTurn(const int32 Direction)
{
    if (bRotating || Direction == 0) { return false; }
    StartYaw = CurrentYaw;
    TargetYaw = StartYaw + (Direction > 0 ? 90.f : -90.f);
    RotationElapsed = 0.f;
    bRotating = true;
    OnRotationStarted.Broadcast();
    return true;
}

void AGGJGroupCameraActor::AdvanceRotation(const float DeltaSeconds)
{
    if (!bRotating) { return; }
    const float Duration = FMath::Max(0.1f, QuarterTurnDuration);
    RotationElapsed = FMath::Min(RotationElapsed + DeltaSeconds, Duration);
    const float Alpha = RotationElapsed / Duration;
    const float EasedAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha,
        FMath::Max(1.1f, RotationEaseExponent));
    CurrentYaw = FMath::Lerp(StartYaw, TargetYaw, EasedAlpha);
    if (Alpha >= 1.f)
    {
        CurrentYaw = FMath::UnwindDegrees(TargetYaw);
        bRotating = false;
        OnRotationFinished.Broadcast();
    }
}

void AGGJGroupCameraActor::ApplyCameraPose()
{
    SetActorLocation(CurrentFocus);
    const FRotator ViewRotation(FMath::Clamp(Pitch, -85.f, -20.f), CurrentYaw, 0.f);
    Camera->SetRelativeLocation(-ViewRotation.Vector() * CurrentDistance);
    Camera->SetRelativeRotation(ViewRotation);
    Camera->SetFieldOfView(FMath::Clamp(FieldOfView, 20.f, 100.f));
}

FVector AGGJGroupCameraActor::ScreenInputToWorld(const FVector2D ScreenInput) const
{
    if (!Camera)
    {
        return FVector::ZeroVector;
    }

    FVector Right = Camera->GetRightVector().GetSafeNormal2D();
    FVector ScreenUp = Camera->GetUpVector().GetSafeNormal2D();

    // 镜头接近水平时 Camera Up 几乎垂直于地面，改用镜头前方的水平投影。
    // 正俯视时 Forward 投影会退化，但 Camera Up 正好有效，因此两种摆法都能覆盖。
    if (ScreenUp.IsNearlyZero())
    {
        ScreenUp = Camera->GetForwardVector().GetSafeNormal2D();
    }
    if (Right.IsNearlyZero() && !ScreenUp.IsNearlyZero())
    {
        Right = FVector::CrossProduct(ScreenUp, FVector::UpVector).GetSafeNormal();
    }
    return (Right * ScreenInput.X + ScreenUp * ScreenInput.Y).GetClampedToMaxSize(1.f);
}
