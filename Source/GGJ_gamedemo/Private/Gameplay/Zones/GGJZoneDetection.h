#pragma once

// 区域内部共享的纯几何检测。它不依赖 Collision Profile 或 Generate Overlap Events，
// 特别适合胶囊与 Physical Animation 骨骼刚体并存的角色。

#include "Components/BoxComponent.h"
#include "GameFramework/Actor.h"

namespace GGJZoneDetection
{
    inline bool IsActorOriginInsideBox(const UBoxComponent* Box, const AActor* Actor,
        const float WorldPadding)
    {
        if (!Box || !IsValid(Actor)) { return false; }
        const FTransform BoxTransform = Box->GetComponentTransform();
        const FVector LocalPosition = BoxTransform.InverseTransformPosition(Actor->GetActorLocation());
        const FVector Scale = BoxTransform.GetScale3D().GetAbs();
        const float SafePadding = FMath::Max(0.f, WorldPadding);
        const FVector LocalPadding(
            SafePadding / FMath::Max(Scale.X, UE_KINDA_SMALL_NUMBER),
            SafePadding / FMath::Max(Scale.Y, UE_KINDA_SMALL_NUMBER),
            SafePadding / FMath::Max(Scale.Z, UE_KINDA_SMALL_NUMBER));
        const FVector Extent = Box->GetUnscaledBoxExtent() + LocalPadding;
        return FMath::Abs(LocalPosition.X) <= Extent.X
            && FMath::Abs(LocalPosition.Y) <= Extent.Y
            && FMath::Abs(LocalPosition.Z) <= Extent.Z;
    }
}
