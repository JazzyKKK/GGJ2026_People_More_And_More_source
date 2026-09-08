#include "Gameplay/Effects/GGJPixelBurstVFX.h"

#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Character/GGJCharacterGroupManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

namespace GGJPixelBurst
{
    struct FPixelDescriptor
    {
        FVector Direction = FVector::UpVector;
        float DistanceScale = 1.f;
        float SizeScale = 1.f;
        bool bStreak = false;
        bool bSecondary = false;
    };

    class FPixelBurstSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        explicit FPixelBurstSceneProxy(const UGGJPixelBurstRenderComponent* Component)
            : FPrimitiveSceneProxy(Component)
            , bActive(Component->bBurstActive)
            , StartTime(Component->StartWorldTime)
            , Duration(FMath::Max(0.05f, Component->Duration))
            , BurstRadius(FMath::Max(10.f, Component->BurstRadius))
            , PixelSize(FMath::Clamp(Component->PixelSize, 1.f, 32.f))
            , GridSize(FMath::Max(1.f, Component->GridSize))
            , PrimaryColor(Component->PrimaryColor)
            , SecondaryColor(Component->SecondaryColor)
        {
            FRandomStream Random(Component->RandomSeed);
            const int32 SafeCount = FMath::Clamp(Component->ParticleCount, 8, 512);
            const float SafeStreakRatio = FMath::Clamp(Component->StreakRatio, 0.f, 1.f);
            Pixels.Reserve(SafeCount);
            for (int32 Index = 0; Index < SafeCount; ++Index)
            {
                FPixelDescriptor& Pixel = Pixels.AddDefaulted_GetRef();
                Pixel.Direction = Random.GetUnitVector();
                Pixel.Direction.Z += FMath::Clamp(Component->UpwardBias, -1.f, 1.f);
                Pixel.Direction.Normalize();
                Pixel.DistanceScale = Random.FRandRange(0.55f, 1.f);
                Pixel.SizeScale = Random.FRandRange(0.7f, 1.35f);
                Pixel.bStreak = Random.FRand() < SafeStreakRatio;
                Pixel.bSecondary = Random.FRand() < 0.4f;
            }
        }

        virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
            const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
            FMeshElementCollector& Collector) const override
        {
            if (!bActive)
            {
                return;
            }
            const float NormalizedTime = FMath::Clamp(
                (ViewFamily.Time.GetWorldTimeSeconds() - StartTime) / Duration, 0.f, 1.f);
            const float TravelAlpha = 1.f - FMath::Square(1.f - NormalizedTime);
            const float Fade = 1.f - NormalizedTime;

            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
            {
                if ((VisibilityMap & (1u << ViewIndex)) == 0)
                {
                    continue;
                }
                FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
                for (const FPixelDescriptor& Pixel : Pixels)
                {
                    FVector LocalPosition = Pixel.Direction
                        * BurstRadius * TravelAlpha * Pixel.DistanceScale;
                    LocalPosition.X = FMath::GridSnap(LocalPosition.X, GridSize);
                    LocalPosition.Y = FMath::GridSnap(LocalPosition.Y, GridSize);
                    LocalPosition.Z = FMath::GridSnap(LocalPosition.Z, GridSize);

                    FLinearColor Color = Pixel.bSecondary ? SecondaryColor : PrimaryColor;
                    Color.A *= Fade;
                    PDI->DrawPoint(GetLocalToWorld().TransformPosition(LocalPosition), Color,
                        PixelSize * Pixel.SizeScale, SDPG_World);

                    // [PIXEL_BURST_VFX] 高速粒子沿反方向留出离散像素拖尾。
                    if (Pixel.bStreak)
                    {
                        for (int32 Step = 1; Step <= 2; ++Step)
                        {
                            const FVector Trail = LocalPosition - Pixel.Direction * GridSize * Step;
                            FLinearColor TrailColor = Color;
                            TrailColor.A *= 1.f - Step * 0.32f;
                            PDI->DrawPoint(GetLocalToWorld().TransformPosition(Trail), TrailColor,
                                PixelSize * Pixel.SizeScale * (1.f - Step * 0.16f), SDPG_World);
                        }
                    }
                }
            }
        }

        virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
        {
            FPrimitiveViewRelevance Relevance;
            Relevance.bDrawRelevance = IsShown(View);
            Relevance.bDynamicRelevance = true;
            Relevance.bRenderInMainPass = ShouldRenderInMainPass();
            Relevance.bEditorPrimitiveRelevance = UseEditorCompositing(View);
            return Relevance;
        }

        virtual uint32 GetMemoryFootprint() const override
        {
            return sizeof(*this) + Pixels.GetAllocatedSize();
        }

        virtual SIZE_T GetTypeHash() const override
        {
            static size_t UniquePointer;
            return reinterpret_cast<size_t>(&UniquePointer);
        }

    private:
        bool bActive;
        float StartTime;
        float Duration;
        float BurstRadius;
        float PixelSize;
        float GridSize;
        FLinearColor PrimaryColor;
        FLinearColor SecondaryColor;
        TArray<FPixelDescriptor> Pixels;
    };
}

UGGJPixelBurstRenderComponent::UGGJPixelBurstRenderComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    CastShadow = false;
    bReceivesDecals = false;
}

FPrimitiveSceneProxy* UGGJPixelBurstRenderComponent::CreateSceneProxy()
{
    return new GGJPixelBurst::FPixelBurstSceneProxy(this);
}

FBoxSphereBounds UGGJPixelBurstRenderComponent::CalcBounds(
    const FTransform& LocalToWorld) const
{
    const FVector Extent(FMath::Max(10.f, BurstRadius) + GridSize * 2.f);
    return FBoxSphereBounds(FBox(-Extent, Extent)).TransformBy(LocalToWorld);
}

AGGJPixelBurstVFX::AGGJPixelBurstVFX()
{
    // [PIXEL_BURST_DETECTION] 与项目 FlowZone 一致，直接检测群体成员而非依赖碰撞预设。
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
    TriggerSphere->ShapeColor = FColor(80, 225, 255);

    PixelRenderer = CreateDefaultSubobject<UGGJPixelBurstRenderComponent>(TEXT("PixelRenderer"));
    PixelRenderer->SetupAttachment(TriggerSphere);
}

void AGGJPixelBurstVFX::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    TriggerSphere->SetSphereRadius(FMath::Max(10.f, TriggerRadius));
    SynchronizeRenderer();
}

void AGGJPixelBurstVFX::BeginPlay()
{
    Super::BeginPlay();
    TriggerSphere->OnComponentBeginOverlap.AddDynamic(
        this, &AGGJPixelBurstVFX::HandleTriggerBeginOverlap);
}

void AGGJPixelBurstVFX::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bTriggered && FindTouchingCharacter())
    {
        TriggerBurst();
    }
}

AGGJPhysicalAnimationCharacter* AGGJPixelBurstVFX::FindTouchingCharacter() const
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
    for (TActorIterator<AGGJPhysicalAnimationCharacter> It(World); It; ++It)
    {
        if (TouchesTrigger(*It))
        {
            return *It;
        }
    }
    return nullptr;
}

void AGGJPixelBurstVFX::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    if (!bTriggered && Cast<AGGJPhysicalAnimationCharacter>(OtherActor))
    {
        TriggerBurst();
    }
}

void AGGJPixelBurstVFX::TriggerBurst()
{
    if (bTriggered || !GetWorld())
    {
        return;
    }

    // [PIXEL_BURST_ONESHOT] 触发后立即关闭碰撞，确保多人同时进入也只播放一次。
    bTriggered = true;
    TriggerSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SynchronizeRenderer();
    PixelRenderer->bBurstActive = true;
    PixelRenderer->StartWorldTime = GetWorld()->GetTimeSeconds();
    PixelRenderer->MarkRenderStateDirty();

    SetLifeSpan(FMath::Max(0.05f, Duration) + 0.05f);
}

void AGGJPixelBurstVFX::SynchronizeRenderer()
{
    PixelRenderer->Duration = FMath::Max(0.05f, Duration);
    PixelRenderer->BurstRadius = FMath::Max(10.f, BurstRadius);
    PixelRenderer->ParticleCount = FMath::Clamp(ParticleCount, 8, 512);
    PixelRenderer->PixelSize = FMath::Clamp(PixelSize, 1.f, 32.f);
    PixelRenderer->GridSize = FMath::Max(1.f, GridSize);
    PixelRenderer->UpwardBias = FMath::Clamp(UpwardBias, -1.f, 1.f);
    PixelRenderer->StreakRatio = FMath::Clamp(StreakRatio, 0.f, 1.f);
    PixelRenderer->PrimaryColor = PrimaryColor;
    PixelRenderer->SecondaryColor = SecondaryColor;
    PixelRenderer->RandomSeed = RandomSeed;
    PixelRenderer->UpdateBounds();
    PixelRenderer->MarkRenderStateDirty();
}
