// 出生点标记 Actor 的默认组件实现。
// 本文件不生成人口，生成行为集中在 CrowdLevelDirector。
#include "Crowd/Agents/CrowdSpawnPoint.h"
#include "Components/ArrowComponent.h"

ACrowdSpawnPoint::ACrowdSpawnPoint()
{
    // 纯标记 Actor 不需要逐帧逻辑。
    PrimaryActorTick.bCanEverTick = false;
    Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("SpawnMarker"));
    SetRootComponent(Arrow);
    // 青色仅帮助在关卡视口中和绿色 Director 原点区分。
    Arrow->ArrowColor = FColor::Cyan;
}
