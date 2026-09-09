#pragma once

// 派对模式专用出生区域；继承旧 SpawnZone 的紧凑堆叠算法，但额外标记归属玩家。

#include "CoreMinimal.h"
#include "Gameplay/Zones/GGJSpawnZone.h"
#include "GGJPartySpawnZone.generated.h"

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPartySpawnZone : public AGGJSpawnZone
{
    GENERATED_BODY()

public:
    AGGJPartySpawnZone();
    virtual void OnConstruction(const FTransform& Transform) override;

    /**
     * 该区域服务的玩家编号。0=P1（WASD），1=P2（方向键）。
     * 同一玩家放多个区域时，开局人数会像旧模式一样自动平均分配。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Spawn", meta=(ClampMin="0"))
    int32 PlayerIndex = 0;
};
