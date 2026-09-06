// 人群关卡 GameMode 的默认框架类选择。
// 这里只指定 Controller 与 CameraPawn，不接管设计师的场地内容。
#include "Crowd/Framework/CrowdGameMode.h"
#include "Crowd/Framework/CrowdCameraPawn.h"
#include "Crowd/Framework/CrowdPlayerController.h"
#include "EngineUtils.h"
#include "UI/GGJPauseMenuManager.h"

ACrowdGameMode::ACrowdGameMode()
{
    // Controller 只采集一份输入；CameraPawn 是被占有的观察者，不是人群中的小人。
    PlayerControllerClass = ACrowdPlayerController::StaticClass();
    DefaultPawnClass = ACrowdCameraPawn::StaticClass();
    PauseMenuManagerClass = AGGJPauseMenuManager::StaticClass();
}

void ACrowdGameMode::StartPlay()
{
    Super::StartPlay();

    for (TActorIterator<AGGJPauseMenuManager> It(GetWorld()); It; ++It)
    {
        PauseMenuManager = *It;
        break;
    }
    if (!PauseMenuManager && PauseMenuManagerClass)
    {
        PauseMenuManager = GetWorld()->SpawnActor<AGGJPauseMenuManager>(
            PauseMenuManagerClass, FTransform::Identity);
    }
}
