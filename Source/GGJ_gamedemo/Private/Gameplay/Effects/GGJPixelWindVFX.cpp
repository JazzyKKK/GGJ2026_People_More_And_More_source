#include "Gameplay/Effects/GGJPixelWindVFX.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

namespace GGJPixelWind
{
    /** [PIXEL_WIND_VFX] 每个像素的固定随机参数，避免运行时分配和随机数调用。 */
    struct FPixelDescriptor
    {
        FVector BasePosition = FVector::ZeroVector;
        float Phase = 0.f;
        float SpeedScale = 1.f;
        float SwayPhase = 0.f;
        bool bStreak = false;
        bool bUseSecondaryColor = false;
    };

    class FPixelWindSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        explicit FPixelWindSceneProxy(const UGGJPixelWindRenderComponent* Component)
            : FPrimitiveSceneProxy(Component)
            , Extent(Component->EffectExtent.GetAbs())
            , PrimaryColor(Component->PrimaryColor)
            , SecondaryColor(Component->SecondaryColor)
            , RiseSpeed(FMath::Max(1.f, Component->RiseSpeed))
            , PixelSize(FMath::Clamp(Component->PixelSize, 1.f, 32.f))
            , GridSize(FMath::Max(1.f, Component->GridSize))
            , Turbulence(FMath::Max(0.f, Component->Turbulence))
        {
            FRandomStream Random(Component->RandomSeed);
            const int32 SafeCount = FMath::Clamp(Component->ParticleCount, 8, 512);
            const float SafeStreakRatio = FMath::Clamp(Component->StreakRatio, 0.f, 1.f);
            Pixels.Reserve(SafeCount);

            for (int32 Index = 0; Index < SafeCount; ++Index)
            {
                FPixelDescriptor& Pixel = Pixels.AddDefaulted_GetRef();
                Pixel.BasePosition.X = Random.FRandRange(-Extent.X, Extent.X);
                Pixel.BasePosition.Y = Random.FRandRange(-Extent.Y, Extent.Y);
                Pixel.BasePosition.Z = Random.FRandRange(-Extent.Z, Extent.Z);
                Pixel.Phase = Random.FRand();
                Pixel.SpeedScale = Random.FRandRange(0.65f, 1.3f);
                Pixel.SwayPhase = Random.FRandRange(0.f, UE_TWO_PI);
                Pixel.bStreak = Random.FRand() < SafeStreakRatio;
                Pixel.bUseSecondaryColor = Random.FRand() < 0.38f;
            }
        }

        virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
            const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
            FMeshElementCollector& Collector) const override
        {
            const float Time = ViewFamily.Time.GetWorldTimeSeconds();
            const float FullHeight = FMath::Max(20.f, Extent.Z * 2.f);

            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
            {
                if ((VisibilityMap & (1u << ViewIndex)) == 0)
                {
                    continue;
                }

                FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
                for (const FPixelDescriptor& Pixel : Pixels)
                {
                    const float SpeedMultiplier = Pixel.bStreak ? 1.65f : 1.f;
                    const float Travel = FMath::Fmod(
                        (Time * RiseSpeed * Pixel.SpeedScale * SpeedMultiplier)
                            + Pixel.Phase * FullHeight,
                        FullHeight);
                    const float LocalZ = -Extent.Z + Travel;
                    const float LifeAlpha = Travel / FullHeight;

                    // [PIXEL_WIND_VFX] 位置量化到网格，形成逐格跳动而非平滑烟雾。
                    const float Sway = FMath::Sin(
                        Pixel.SwayPhase + Time * (1.7f + Pixel.SpeedScale)) * Turbulence;
                    FVector LocalPosition(
                        Pixel.BasePosition.X + Sway,
                        Pixel.BasePosition.Y + FMath::Cos(Pixel.SwayPhase + Time * 1.2f) * Turbulence * 0.45f,
                        LocalZ);
                    LocalPosition.X = FMath::GridSnap(LocalPosition.X, GridSize);
                    LocalPosition.Y = FMath::GridSnap(LocalPosition.Y, GridSize);
                    LocalPosition.Z = FMath::GridSnap(LocalPosition.Z, GridSize);

                    // [PIXEL_WIND_VFX] 底部和顶部淡出，防止粒子在边界突然跳变。
                    const float Fade = FMath::Clamp(FMath::Sin(LifeAlpha * PI), 0.f, 1.f);
                    FLinearColor Color = Pixel.bUseSecondaryColor ? SecondaryColor : PrimaryColor;
                    Color.A *= Fade;
                    if (Pixel.bStreak)
                    {
                        Color = FMath::Lerp(Color, FLinearColor::White, 0.28f);
                    }

                    PDI->DrawPoint(
                        GetLocalToWorld().TransformPosition(LocalPosition),
                        Color,
                        PixelSize * (Pixel.bStreak ? 1.75f : 1.f),
                        SDPG_World);

                    // [PIXEL_WIND_VFX] 高速像素额外绘制两个相邻格，组成离散竖向短线。
                    if (Pixel.bStreak)
                    {
                        for (int32 Step = 1; Step <= 2; ++Step)
                        {
                            FVector TrailPosition = LocalPosition;
                            TrailPosition.Z -= GridSize * Step;
                            FLinearColor TrailColor = Color;
                            TrailColor.A *= 1.f - Step * 0.3f;
                            PDI->DrawPoint(
                                GetLocalToWorld().TransformPosition(TrailPosition),
                                TrailColor,
                                PixelSize * (1.45f - Step * 0.18f),
                                SDPG_World);
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
        FVector Extent;
        FLinearColor PrimaryColor;
        FLinearColor SecondaryColor;
        float RiseSpeed;
        float PixelSize;
        float GridSize;
        float Turbulence;
        TArray<FPixelDescriptor> Pixels;
    };
}

UGGJPixelWindRenderComponent::UGGJPixelWindRenderComponent()
{
    // [PIXEL_WIND_VFX_ONLY] 无 Tick、无碰撞、无阴影，绝不参与气流或角色逻辑。
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    CastShadow = false;
    bReceivesDecals = false;
}

FPrimitiveSceneProxy* UGGJPixelWindRenderComponent::CreateSceneProxy()
{
    return IsVisible() ? new GGJPixelWind::FPixelWindSceneProxy(this) : nullptr;
}

FBoxSphereBounds UGGJPixelWindRenderComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    const FVector SafeExtent = EffectExtent.GetAbs() + FVector(Turbulence + GridSize);
    return FBoxSphereBounds(FBox(-SafeExtent, SafeExtent)).TransformBy(LocalToWorld);
}

AGGJPixelWindVFX::AGGJPixelWindVFX()
{
    PrimaryActorTick.bCanEverTick = false;

    PreviewBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("PreviewBounds"));
    SetRootComponent(PreviewBounds);
    PreviewBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewBounds->SetGenerateOverlapEvents(false);
    PreviewBounds->SetHiddenInGame(true);
    PreviewBounds->ShapeColor = FColor(80, 230, 255);

    PixelRenderer = CreateDefaultSubobject<UGGJPixelWindRenderComponent>(TEXT("PixelRenderer"));
    PixelRenderer->SetupAttachment(PreviewBounds);

    DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
    DirectionArrow->SetupAttachment(PreviewBounds);
    DirectionArrow->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
    DirectionArrow->SetArrowColor(FLinearColor(0.4f, 0.95f, 1.f));
    DirectionArrow->SetArrowLength(180.f);
    DirectionArrow->SetHiddenInGame(true);
}

void AGGJPixelWindVFX::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    SynchronizeRenderer();
}

void AGGJPixelWindVFX::SynchronizeRenderer()
{
    const FVector SafeExtent(
        FMath::Max(10.f, FMath::Abs(EffectExtent.X)),
        FMath::Max(10.f, FMath::Abs(EffectExtent.Y)),
        FMath::Max(10.f, FMath::Abs(EffectExtent.Z)));

    PreviewBounds->SetBoxExtent(SafeExtent);
    PixelRenderer->EffectExtent = SafeExtent;
    PixelRenderer->PrimaryColor = PrimaryColor;
    PixelRenderer->SecondaryColor = SecondaryColor;
    PixelRenderer->ParticleCount = FMath::Clamp(ParticleCount, 8, 512);
    PixelRenderer->RiseSpeed = FMath::Max(1.f, RiseSpeed);
    PixelRenderer->PixelSize = FMath::Clamp(PixelSize, 1.f, 32.f);
    PixelRenderer->GridSize = FMath::Max(1.f, GridSize);
    PixelRenderer->Turbulence = FMath::Max(0.f, Turbulence);
    PixelRenderer->StreakRatio = FMath::Clamp(StreakRatio, 0.f, 1.f);
    PixelRenderer->RandomSeed = RandomSeed;
    PixelRenderer->UpdateBounds();
    PixelRenderer->MarkRenderStateDirty();
}
