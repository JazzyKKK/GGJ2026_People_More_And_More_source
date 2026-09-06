#include "Character/GGJFixedCameraGameMode.h"

#include "Character/GGJFixedGroupCameraActor.h"

AGGJFixedCameraGameMode::AGGJFixedCameraGameMode()
{
    FixedCameraClass = AGGJFixedGroupCameraActor::StaticClass();
    GroupCameraClass = FixedCameraClass;
    bAutoSpawnGroupCamera = false;
}

void AGGJFixedCameraGameMode::StartPlay()
{
    // 从旧 BP_GameMode 复制并 Reparent 时，蓝图可能残留 BP_GroupCamera_C 覆盖值。
    // 每次运行前以本模式独立的 FixedCameraClass 覆盖，保证关卡里的固定相机能被识别。
    GroupCameraClass = FixedCameraClass
        ? FixedCameraClass.Get()
        : AGGJFixedGroupCameraActor::StaticClass();
    bAutoSpawnGroupCamera = false;
    Super::StartPlay();
}
