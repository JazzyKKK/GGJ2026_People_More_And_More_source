#include "Gameplay/Zones/GGJSpawnZone.h"

#include "Components/BoxComponent.h"
#include "Gameplay/Zones/GGJSpawnPacking.h"

AGGJSpawnZone::AGGJSpawnZone()
{
    PrimaryActorTick.bCanEverTick = false;
    SpawnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
    SetRootComponent(SpawnBounds);
    SpawnBounds->SetBoxExtent(FVector(450.f, 450.f, 100.f));
    SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpawnBounds->SetGenerateOverlapEvents(false);
    SpawnBounds->SetHiddenInGame(true);
    SpawnBounds->ShapeColor = FColor(40, 210, 110);
}

FVector AGGJSpawnZone::GetSpawnLocation(const int32 LocalIndex, const int32 CountInThisZone,
    const float PreferredSpacing, const int32 LayerCapacity, const float LayerHeight,
    const float PositionJitter) const
{
    if (!SpawnBounds || CountInThisZone <= 0) { return GetActorLocation(); }

    FVector LocalOffset = GGJSpawnPacking::MakeCompactStackOffset(LocalIndex, CountInThisZone,
        FMath::Max(40.f, PreferredSpacing), LayerCapacity, LayerHeight, PositionJitter);

    // 横向始终限制在出生 Box 内；Z 允许超出 Box，形成向上堆叠后落下的效果。
    const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
    LocalOffset.X = FMath::Clamp(LocalOffset.X, -Extent.X, Extent.X);
    LocalOffset.Y = FMath::Clamp(LocalOffset.Y, -Extent.Y, Extent.Y);
    LocalOffset.Z += SpawnHeightOffset;
    return SpawnBounds->GetComponentTransform().TransformPosition(LocalOffset);
}
