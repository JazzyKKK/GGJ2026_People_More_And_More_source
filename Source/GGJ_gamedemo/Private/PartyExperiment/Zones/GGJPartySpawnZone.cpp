#include "PartyExperiment/Zones/GGJPartySpawnZone.h"

#include "Components/BoxComponent.h"

AGGJPartySpawnZone::AGGJPartySpawnZone()
{
    // P1/P2 使用不同编辑器线框色，运行时依然不渲染任何模型。
    if (SpawnBounds)
    {
        SpawnBounds->ShapeColor = FColor(60, 150, 255);
    }
}

void AGGJPartySpawnZone::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (SpawnBounds)
    {
        SpawnBounds->ShapeColor = PlayerIndex == 0
            ? FColor(60, 150, 255)
            : (PlayerIndex == 1 ? FColor(255, 120, 70) : FColor(180, 90, 255));
    }
}
