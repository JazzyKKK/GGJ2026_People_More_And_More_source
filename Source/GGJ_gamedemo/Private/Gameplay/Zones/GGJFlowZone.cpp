#include "Gameplay/Zones/GGJFlowZone.h"

#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Gameplay/Zones/GGJZoneDetection.h"

AGGJFlowZone::AGGJFlowZone()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    FlowBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("FlowBounds"));
    SetRootComponent(FlowBounds);
    FlowBounds->SetBoxExtent(ZoneExtent);
    FlowBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    FlowBounds->SetCollisionObjectType(ECC_WorldDynamic);
    FlowBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    FlowBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    FlowBounds->SetGenerateOverlapEvents(true);
    FlowBounds->SetHiddenInGame(true);
    FlowBounds->ShapeColor = FColor(25, 220, 255);

    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(FlowBounds);
    DirectionArrow->SetArrowColor(DebugArrowColor);
    DirectionArrow->SetArrowLength(DebugArrowLength);
    DirectionArrow->SetArrowSize(2.f);
    DirectionArrow->bIsScreenSizeScaled = true;
    DirectionArrow->SetHiddenInGame(!bShowDirectionInGame);
}

void AGGJFlowZone::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (FlowBounds)
    {
        FlowBounds->SetBoxExtent(FVector(
            FMath::Max(10.f, ZoneExtent.X),
            FMath::Max(10.f, ZoneExtent.Y),
            FMath::Max(10.f, ZoneExtent.Z)));
    }
    if (DirectionArrow)
    {
        DirectionArrow->SetRelativeLocation(FVector::ZeroVector);
        DirectionArrow->SetRelativeRotation(FRotator::ZeroRotator);
        DirectionArrow->SetArrowLength(FMath::Max(20.f, DebugArrowLength));
        DirectionArrow->SetArrowColor(DebugArrowColor);
        DirectionArrow->SetHiddenInGame(!bShowDirectionInGame);
    }
}

void AGGJFlowZone::BeginPlay()
{
    Super::BeginPlay();
    ResolveManager();
}

AGGJCharacterGroupManager* AGGJFlowZone::ResolveManager()
{
    if (GroupManager.IsValid())
    {
        return GroupManager.Get();
    }
    if (!GetWorld())
    {
        return nullptr;
    }
    for (TActorIterator<AGGJCharacterGroupManager> It(GetWorld()); It; ++It)
    {
        GroupManager = *It;
        return *It;
    }
    return nullptr;
}

FVector AGGJFlowZone::GetFlowDirection() const
{
    FVector Direction = GetActorForwardVector();
    if (!bUseFull3DDirection)
    {
        Direction.Z = 0.f;
    }
    return Direction.GetSafeNormal();
}

void AGGJFlowZone::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    AffectedCount = 0;
    if (!bEnabled || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.f)
    {
        return;
    }

    AGGJCharacterGroupManager* Manager = ResolveManager();
    if (!Manager)
    {
        return;
    }

    for (AGGJPhysicalAnimationCharacter* Character : Manager->GetMembers())
    {
        // 传送带通常贴着地面且很薄，因此检测完整胶囊，而不只检测角色中心点。
        const UCapsuleComponent* Capsule = Character
            ? Character->GetCapsuleComponent() : nullptr;
        const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
        const float CapsuleHalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.f;
        const FTransform BoundsTransform = FlowBounds->GetComponentTransform();
        const FVector LocalPosition = BoundsTransform.InverseTransformPosition(
            Character ? Character->GetActorLocation() : FVector::ZeroVector);
        const FVector BoundsScale = BoundsTransform.GetScale3D().GetAbs();
        const FVector Extent = FlowBounds->GetUnscaledBoxExtent() + FVector(
            (FMath::Max(0.f, DetectionPadding) + CapsuleRadius)
                / FMath::Max(BoundsScale.X, UE_KINDA_SMALL_NUMBER),
            (FMath::Max(0.f, DetectionPadding) + CapsuleRadius)
                / FMath::Max(BoundsScale.Y, UE_KINDA_SMALL_NUMBER),
            (FMath::Max(0.f, DetectionPadding) + CapsuleHalfHeight)
                / FMath::Max(BoundsScale.Z, UE_KINDA_SMALL_NUMBER));
        const bool bTouchesBounds = Character
            && FMath::Abs(LocalPosition.X) <= Extent.X
            && FMath::Abs(LocalPosition.Y) <= Extent.Y
            && FMath::Abs(LocalPosition.Z) <= Extent.Z;
        if (bTouchesBounds)
        {
            ++AffectedCount;
            ApplyFlowToCharacter(Character, DeltaSeconds);
        }
    }
    if (DirectionArrow)
    {
        DirectionArrow->SetArrowColor(
            AffectedCount > 0 ? ActiveDebugArrowColor : DebugArrowColor);
    }
}

void AGGJFlowZone::ApplyFlowToCharacter(AGGJPhysicalAnimationCharacter* Character,
    const float DeltaSeconds)
{
    if (!IsValid(Character) || !bEnabled || DeltaSeconds <= 0.f)
    {
        return;
    }

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement || (!bAffectFallingCharacters && Movement->IsFalling()))
    {
        return;
    }

    const FVector Direction = GetFlowDirection();
    if (Direction.IsNearlyZero())
    {
        return;
    }

    const float SafeTargetSpeed = FMath::Max(0.f, FlowSpeed);
    const float CurrentAlongFlow = FVector::DotProduct(Movement->Velocity, Direction);
    const float MaxVelocityChange = FMath::Max(0.f, Acceleration) * DeltaSeconds;

    float VelocityChange = 0.f;
    if (FlowMode == EGGJFlowZoneMode::TargetVelocity)
    {
        // 传送带会把顺向速度拉到目标值；人物仍保留横向和垂直速度。
        VelocityChange = FMath::Clamp(SafeTargetSpeed - CurrentAlongFlow,
            -MaxVelocityChange, MaxVelocityChange);
    }
    else if (CurrentAlongFlow < SafeTargetSpeed)
    {
        // 风力只顺向加速，不会把本来跑得更快的人强行减速。
        VelocityChange = FMath::Min(MaxVelocityChange,
            SafeTargetSpeed - CurrentAlongFlow);
    }

    Movement->Velocity += Direction * VelocityChange;

    // 仅改 Velocity 会被 PhysWalking 的地面摩擦在同一帧抵消。
    // 强制输入让 CharacterMovement 把区域看作持续外部驱动；未被 Controller 占有的小人也有效。
    if (CurrentAlongFlow < SafeTargetSpeed)
    {
        const float MaxCharacterAcceleration = FMath::Max(1.f, Movement->GetMaxAcceleration());
        const float ForcedInputScale = FMath::Clamp(
            FMath::Max(0.f, Acceleration) / MaxCharacterAcceleration, 0.f, 1.f);
        Character->AddMovementInput(Direction, ForcedInputScale, true);
    }
}
