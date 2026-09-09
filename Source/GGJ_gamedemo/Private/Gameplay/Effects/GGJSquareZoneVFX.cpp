#include "Gameplay/Effects/GGJSquareZoneVFX.h"

#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"

namespace GGJSquareZoneVFX
{
    /** 每个粒子的稳定随机数据只在 SceneProxy 创建时生成。 */
    struct FSquareParticle
    {
        float PerimeterAlpha = 0.f;
        float HeightPhase = 0.f;
        float SpeedScale = 1.f;
        float SideJitter = 0.f;
        float SizeScale = 1.f;
        bool bUseSecondaryColor = false;
    };

    /** 把 0~1 的进度映射到矩形四边，确保粒子不是填满整个区域。 */
    FVector PointOnPerimeter(const float Distance, const FVector2D& Extent)
    {
        const float Width = Extent.X * 2.f;
        const float Height = Extent.Y * 2.f;
        const float Perimeter = FMath::Max(1.f, (Width + Height) * 2.f);
        float Cursor = FMath::Fmod(FMath::Max(0.f, Distance), Perimeter);

        if (Cursor < Width)
        {
            return FVector(-Extent.X + Cursor, -Extent.Y, 0.f);
        }
        Cursor -= Width;
        if (Cursor < Height)
        {
            return FVector(Extent.X, -Extent.Y + Cursor, 0.f);
        }
        Cursor -= Height;
        if (Cursor < Width)
        {
            return FVector(Extent.X - Cursor, Extent.Y, 0.f);
        }
        Cursor -= Width;
        return FVector(-Extent.X, Extent.Y - Cursor, 0.f);
    }

    class FSquareZoneSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        explicit FSquareZoneSceneProxy(const UGGJSquareZoneVFXComponent* Component)
            : FPrimitiveSceneProxy(Component)
            , Extent(
                FMath::Max(10.f, FMath::Abs(Component->ZoneExtent.X)),
                FMath::Max(10.f, FMath::Abs(Component->ZoneExtent.Y)))
            , RiseHeight(FMath::Max(10.f, Component->RiseHeight))
            , RiseSpeed(FMath::Max(1.f, Component->RiseSpeed))
            , OrbitSpeed(Component->OrbitSpeed)
            , bUseWorldSpaceParticleSize(Component->bUseWorldSpaceParticleSize)
            , WorldParticleSize(FMath::Max(0.5f, Component->WorldParticleSize))
            , PixelSize(FMath::Clamp(Component->PixelSize, 1.f, 40.f))
            , BoundaryThickness(FMath::Clamp(Component->BoundaryThickness, 0.5f, 24.f))
            , bDrawBoundary(Component->bDrawBoundary)
            , PrimaryColor(Component->PrimaryColor)
            , SecondaryColor(Component->SecondaryColor)
        {
            FRandomStream Random(Component->RandomSeed);
            const int32 SafeCount = FMath::Clamp(Component->ParticleCount, 4, 512);
            const float SafeLargeRatio = FMath::Clamp(Component->LargeParticleRatio, 0.f, 1.f);
            Particles.Reserve(SafeCount);
            for (int32 Index = 0; Index < SafeCount; ++Index)
            {
                FSquareParticle& Particle = Particles.AddDefaulted_GetRef();
                Particle.PerimeterAlpha = Random.FRand();
                Particle.HeightPhase = Random.FRand();
                Particle.SpeedScale = Random.FRandRange(0.65f, 1.35f);
                Particle.SideJitter = Random.FRandRange(-10.f, 10.f);
                Particle.SizeScale = Random.FRand() < SafeLargeRatio
                    ? Random.FRandRange(1.6f, 2.6f)
                    : Random.FRandRange(0.7f, 1.25f);
                Particle.bUseSecondaryColor = Random.FRand() < 0.38f;
            }
        }

        virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
            const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
            FMeshElementCollector& Collector) const override
        {
            const float Time = ViewFamily.Time.GetWorldTimeSeconds();
            const float Width = Extent.X * 2.f;
            const float Height = Extent.Y * 2.f;
            const float Perimeter = FMath::Max(1.f, (Width + Height) * 2.f);

            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
            {
                if ((VisibilityMap & (1u << ViewIndex)) == 0)
                {
                    continue;
                }

                FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
                if (bDrawBoundary)
                {
                    DrawBoundary(PDI);
                }

                for (const FSquareParticle& Particle : Particles)
                {
                    const float PerimeterDistance =
                        Particle.PerimeterAlpha * Perimeter
                        + Time * OrbitSpeed * Particle.SpeedScale;
                    FVector LocalPosition = PointOnPerimeter(PerimeterDistance, Extent);

                    // 少量向内外抖动，避免所有粒子看起来像一条规整的虚线。
                    const bool bVerticalSide = FMath::IsNearlyEqual(FMath::Abs(LocalPosition.X), Extent.X, 1.f);
                    if (bVerticalSide)
                    {
                        LocalPosition.X += FMath::Sin(Time * 1.7f + Particle.HeightPhase * UE_TWO_PI)
                            * Particle.SideJitter;
                    }
                    else
                    {
                        LocalPosition.Y += FMath::Sin(Time * 1.7f + Particle.HeightPhase * UE_TWO_PI)
                            * Particle.SideJitter;
                    }

                    const float HeightTravel = FMath::Fmod(
                        Particle.HeightPhase * RiseHeight
                        + Time * RiseSpeed * Particle.SpeedScale,
                        RiseHeight);
                    const float HeightAlpha = HeightTravel / RiseHeight;
                    LocalPosition.Z = HeightTravel;

                    // 上下两端淡出，使循环重生不产生突兀跳变。
                    const float Fade = FMath::Clamp(FMath::Sin(HeightAlpha * PI), 0.f, 1.f);
                    FLinearColor Color = Particle.bUseSecondaryColor ? SecondaryColor : PrimaryColor;
                    Color.A *= Fade;
                    const FVector WorldPosition = GetLocalToWorld().TransformPosition(LocalPosition);
                    float DrawSize = PixelSize * Particle.SizeScale;
                    if (bUseWorldSpaceParticleSize)
                    {
                        DrawSize = CalculateWorldSpaceScreenSize(
                            Views[ViewIndex], WorldPosition, WorldParticleSize * Particle.SizeScale);
                    }
                    PDI->DrawPoint(WorldPosition, Color, DrawSize, SDPG_World);
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
            return sizeof(*this) + Particles.GetAllocatedSize();
        }

        virtual SIZE_T GetTypeHash() const override
        {
            static size_t UniquePointer;
            return reinterpret_cast<size_t>(&UniquePointer);
        }

    private:
        /**
         * DrawPoint 接收的是屏幕像素大小。这里按当前相机距离反算像素值，
         * 让粒子在世界中的尺寸保持稳定，远处不会仍占据很大的屏幕面积。
         */
        static float CalculateWorldSpaceScreenSize(const FSceneView* View,
            const FVector& WorldPosition, const float WorldSize)
        {
            if (!View)
            {
                return 1.f;
            }

            const float ViewWidth = FMath::Max(1.f,
                static_cast<float>(View->UnscaledViewRect.Width()));
            const FMatrix& ProjectionMatrix = View->ViewMatrices.GetProjectionMatrix();
            const float HorizontalProjectionScale =
                FMath::Max(UE_KINDA_SMALL_NUMBER, FMath::Abs(ProjectionMatrix.M[0][0]));

            float PixelsPerWorldUnit = ViewWidth * 0.5f * HorizontalProjectionScale;
            if (View->IsPerspectiveProjection())
            {
                const float Distance = FMath::Max(1.f,
                    FVector::Distance(View->ViewMatrices.GetViewOrigin(), WorldPosition));
                PixelsPerWorldUnit /= Distance;
            }

            // 保留极小的可见尺寸，同时限制近距离时的最大尺寸，避免再次糊成色块。
            return FMath::Clamp(WorldSize * PixelsPerWorldUnit, 0.65f, 40.f);
        }

        void DrawBoundary(FPrimitiveDrawInterface* PDI) const
        {
            const FVector Corners[4] =
            {
                FVector(-Extent.X, -Extent.Y, 1.f),
                FVector( Extent.X, -Extent.Y, 1.f),
                FVector( Extent.X,  Extent.Y, 1.f),
                FVector(-Extent.X,  Extent.Y, 1.f)
            };

            // 宽而透明的一层加窄而明亮的一层，形成简单的方形“辉光”边缘。
            FLinearColor GlowColor = PrimaryColor;
            GlowColor.A *= 0.2f;
            FLinearColor CoreColor = FMath::Lerp(PrimaryColor, FLinearColor::White, 0.22f);
            CoreColor.A = PrimaryColor.A;
            for (int32 Side = 0; Side < 4; ++Side)
            {
                const FVector Start = GetLocalToWorld().TransformPosition(Corners[Side]);
                const FVector End = GetLocalToWorld().TransformPosition(Corners[(Side + 1) % 4]);
                PDI->DrawLine(Start, End, GlowColor, SDPG_World,
                    BoundaryThickness * 2.4f, 0.f, true);
                PDI->DrawLine(Start, End, CoreColor, SDPG_World,
                    BoundaryThickness, 0.f, true);
            }
        }

        FVector2D Extent;
        float RiseHeight;
        float RiseSpeed;
        float OrbitSpeed;
        bool bUseWorldSpaceParticleSize;
        float WorldParticleSize;
        float PixelSize;
        float BoundaryThickness;
        bool bDrawBoundary;
        FLinearColor PrimaryColor;
        FLinearColor SecondaryColor;
        TArray<FSquareParticle> Particles;
    };
}

UGGJSquareZoneVFXComponent::UGGJSquareZoneVFXComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    CastShadow = false;
    bReceivesDecals = false;
}

FPrimitiveSceneProxy* UGGJSquareZoneVFXComponent::CreateSceneProxy()
{
    return IsVisible() ? new GGJSquareZoneVFX::FSquareZoneSceneProxy(this) : nullptr;
}

FBoxSphereBounds UGGJSquareZoneVFXComponent::CalcBounds(
    const FTransform& LocalToWorld) const
{
    const FVector SafeExtent(
        FMath::Max(10.f, FMath::Abs(ZoneExtent.X)) + 20.f,
        FMath::Max(10.f, FMath::Abs(ZoneExtent.Y)) + 20.f,
        FMath::Max(10.f, RiseHeight) + 20.f);
    return FBoxSphereBounds(FBox(FVector(-SafeExtent.X, -SafeExtent.Y, -20.f), SafeExtent))
        .TransformBy(LocalToWorld);
}
