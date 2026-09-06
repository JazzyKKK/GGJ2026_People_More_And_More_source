#pragma once

// 关卡设计师可摆放的出生位置标记。
// 它只保存位置和提供视口箭头，不自行生成小人。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrowdSpawnPoint.generated.h"

class UArrowComponent;

/** 地面 XY 出生标记；真正的生成顺序和高度统一由 LevelDirector 决定。 */
UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API ACrowdSpawnPoint : public AActor
{
    GENERATED_BODY()
public:
    /** 创建无运行开销的可视箭头。 */
    ACrowdSpawnPoint();

    /** 仅作编辑器识别和朝向提示；当前玩法不读取箭头方向。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crowd")
    TObjectPtr<UArrowComponent> Arrow;
};
