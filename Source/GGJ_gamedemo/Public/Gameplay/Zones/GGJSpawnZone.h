#pragma once

// 初始人数分布用的无模型区域；在编辑器中通过 Box 范围摆放。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJSpawnZone.generated.h"

class UBoxComponent;

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJSpawnZone : public AActor
{
    GENERATED_BODY()

public:
    AGGJSpawnZone();

    /** Manager 按区域内局部序号取得一个紧凑、可向上堆叠的位置。 */
    UFUNCTION(BlueprintPure, Category="Gameplay Zone|Spawn")
    FVector GetSpawnLocation(int32 LocalIndex, int32 CountInThisZone,
        float PreferredSpacing, int32 LayerCapacity, float LayerHeight,
        float PositionJitter) const;

    UFUNCTION(BlueprintPure, Category="Gameplay Zone|Spawn")
    bool IsSpawnEnabled() const { return bEnabled; }

    /** 数字越小越先分配；人数除不尽时，余数依次给靠前区域。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Spawn")
    int32 SpawnOrder = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Spawn")
    bool bEnabled = true;

    /** 在 Box 中心基础上额外抬高角色 Actor 原点。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Spawn", meta=(Units="cm"))
    float SpawnHeightOffset = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UBoxComponent> SpawnBounds;
};
