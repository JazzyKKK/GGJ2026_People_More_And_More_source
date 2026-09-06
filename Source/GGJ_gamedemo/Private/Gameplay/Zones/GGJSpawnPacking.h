#pragma once

// 出生区和运行时增员共用的紧凑立体排布算法。
// 返回局部空间偏移：X/Y 为层内网格，Z 为向上堆叠的层高。

#include "CoreMinimal.h"

namespace GGJSpawnPacking
{
    inline FVector MakeCompactStackOffset(const int32 Index, const int32 TotalCount,
        const float HorizontalSpacing, const int32 LayerCapacity, const float LayerHeight,
        const float PositionJitter)
    {
        if (TotalCount <= 0)
        {
            return FVector::ZeroVector;
        }

        const int32 SafeIndex = FMath::Clamp(Index, 0, TotalCount - 1);
        const int32 SafeCapacity = FMath::Max(1, LayerCapacity);
        const int32 Layer = SafeIndex / SafeCapacity;
        const int32 IndexInLayer = SafeIndex % SafeCapacity;
        const int32 FirstIndexInLayer = Layer * SafeCapacity;
        const int32 CountInLayer = FMath::Min(SafeCapacity, TotalCount - FirstIndexInLayer);
        const int32 Columns = FMath::Max(1,
            FMath::CeilToInt(FMath::Sqrt(static_cast<float>(CountInLayer))));
        const int32 Rows = FMath::Max(1,
            FMath::CeilToInt(static_cast<float>(CountInLayer) / Columns));
        const int32 Column = IndexInLayer % Columns;
        const int32 Row = IndexInLayer / Columns;
        const int32 ItemsInThisRow = FMath::Min(Columns, CountInLayer - Row * Columns);

        // 最后一行也单独居中，避免三人或五人时阵型明显偏向一侧。
        float OffsetX = (Column - (ItemsInThisRow - 1) * 0.5f) * HorizontalSpacing;
        float OffsetY = (Row - (Rows - 1) * 0.5f) * HorizontalSpacing;

        if (PositionJitter > 0.f)
        {
            // 固定种子保证同一批次可复现，同时打破过于整齐、容易保持平衡的堆叠。
            FRandomStream RandomStream(SafeIndex * 196613 + TotalCount * 12289 + 17);
            OffsetX += RandomStream.FRandRange(-PositionJitter, PositionJitter);
            OffsetY += RandomStream.FRandRange(-PositionJitter, PositionJitter);
        }

        return FVector(OffsetX, OffsetY, Layer * FMath::Max(0.f, LayerHeight));
    }
}
