#include "Gameplay/Hazards/GGJRadialMineActor.h"

#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AGGJRadialMineActor::AGGJRadialMineActor()
{
    // [RADIAL_MINE_DETECTION] 不依赖 Overlap 配置，每帧直接检测群体成员胶囊。
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
    SetRootComponent(TriggerSphere);
    TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerSphere->SetGenerateOverlapEvents(true);
    TriggerSphere->SetHiddenInGame(true);
    TriggerSphere->ShapeColor = FColor(255, 90, 45);

    MineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MineMesh"));
    MineMesh->SetupAttachment(TriggerSphere);
    MineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MineMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.15f));

    // [RADIAL_MINE_VISUAL] 使用引擎内置圆柱作为可见占位，蓝图可替换正式地雷模型。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMineMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (DefaultMineMesh.Succeeded())
    {
        MineMesh->SetStaticMesh(DefaultMineMesh.Object);
    }
}

void AGGJRadialMineActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    TriggerSphere->SetSphereRadius(FMath::Max(10.f, TriggerRadius));
}

void AGGJRadialMineActor::BeginPlay()
{
    Super::BeginPlay();
    TriggerSphere->OnComponentBeginOverlap.AddDynamic(
        this, &AGGJRadialMineActor::HandleTriggerBeginOverlap);
}

void AGGJRadialMineActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bEnabled || bTriggered)
    {
        return;
    }
    if (AGGJPhysicalAnimationCharacter* Character = FindTouchingCharacter())
    {
        TriggerMine(Character);
    }
}

AGGJPhysicalAnimationCharacter* AGGJRadialMineActor::FindTouchingCharacter() const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const auto TouchesTrigger = [this](AGGJPhysicalAnimationCharacter* Character)
    {
        if (!IsValid(Character))
        {
            return false;
        }
        const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
        const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;
        const float EffectiveRadius = FMath::Max(10.f, TriggerRadius) + CapsuleRadius;
        return FVector::DistSquared(Character->GetActorLocation(), GetActorLocation())
            <= FMath::Square(EffectiveRadius);
    };

    for (TActorIterator<AGGJCharacterGroupManager> It(World); It; ++It)
    {
        for (AGGJPhysicalAnimationCharacter* Character : It->GetMembers())
        {
            if (TouchesTrigger(Character))
            {
                return Character;
            }
        }
        break;
    }

    // [RADIAL_MINE_DETECTION] 测试关卡没有 GroupManager 时仍能检测单独放置的角色。
    for (TActorIterator<AGGJPhysicalAnimationCharacter> It(World); It; ++It)
    {
        if (TouchesTrigger(*It))
        {
            return *It;
        }
    }
    return nullptr;
}

void AGGJRadialMineActor::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bEnabled || bTriggered)
    {
        return;
    }
    if (AGGJPhysicalAnimationCharacter* Character =
        Cast<AGGJPhysicalAnimationCharacter>(OtherActor))
    {
        TriggerMine(Character);
    }
}

void AGGJRadialMineActor::TriggerMine(AGGJPhysicalAnimationCharacter* TriggeringCharacter)
{
    // [RADIAL_MINE_ONESHOT] 先锁定，避免同一帧多人重叠导致重复触发。
    bTriggered = true;
    TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (bAffectTriggeringCharacterOnly)
    {
        PushCharacter(TriggeringCharacter);
    }
    else if (UWorld* World = GetWorld())
    {
        bool bFoundManager = false;
        for (TActorIterator<AGGJCharacterGroupManager> It(World); It; ++It)
        {
            bFoundManager = true;
            for (AGGJPhysicalAnimationCharacter* Character : It->GetMembers())
            {
                PushCharacter(Character);
            }
            break;
        }
        if (!bFoundManager)
        {
            PushCharacter(TriggeringCharacter);
        }
    }

    // [RADIAL_MINE_ONESHOT] 地雷自身立即消失；独立像素爆炸 Actor 不受影响。
    Destroy();
}

void AGGJRadialMineActor::PushCharacter(AGGJPhysicalAnimationCharacter* Character) const
{
    if (!IsValid(Character))
    {
        return;
    }

    FVector Offset = Character->GetActorLocation() - GetActorLocation();
    Offset.Z = 0.f;
    const float Distance = Offset.Size();
    if (Distance > FMath::Max(10.f, BlastRadius))
    {
        return;
    }

    FVector Direction = Offset.GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        Direction = GetActorForwardVector().GetSafeNormal2D();
    }
    const float Falloff = 1.f - FMath::Clamp(Distance / FMath::Max(10.f, BlastRadius), 0.f, 1.f);
    const FVector LaunchVelocity = Direction * FMath::Max(0.f, HorizontalPush) * Falloff
        + FVector::UpVector * FMath::Max(0.f, UpwardPush) * Falloff;

    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement)
    {
        return;
    }

    // [RADIAL_MINE_PUSH] Walking 会在同一帧用地面约束/摩擦覆盖 LaunchCharacter。
    // 先脱离地面，再直接叠加速度，兼容被 Controller 占有和未占有的群体成员。
    if (LaunchVelocity.Z > 0.f && Movement->MovementMode == MOVE_Walking)
    {
        Movement->SetMovementMode(MOVE_Falling);
    }
    Movement->Velocity += LaunchVelocity;
}
