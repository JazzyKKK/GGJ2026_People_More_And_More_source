#include "Character/GGJPhysicalAnimationGameMode.h"

#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJGroupCameraActor.h"
#include "Character/GGJPhysicalAnimationCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Zones/GGJSpawnZone.h"
#include "UI/GGJPauseMenuManager.h"

AGGJPhysicalAnimationGameMode::AGGJPhysicalAnimationGameMode()
{
    // 蓝图子类会把它替换为带正式模型和动画的 BP_PhysicalAnimationCharacter。
    DefaultPawnClass = AGGJPhysicalAnimationCharacter::StaticClass();
    GroupManagerClass = AGGJCharacterGroupManager::StaticClass();
    GroupCameraClass = AGGJGroupCameraActor::StaticClass();
    PauseMenuManagerClass = AGGJPauseMenuManager::StaticClass();
}

AActor* AGGJPhysicalAnimationGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    if (AActor* PlayerStart = Super::ChoosePlayerStart_Implementation(Player))
    {
        return PlayerStart;
    }

    // OpenLevel/重开 PIE 后不会保留“从这里开始游戏”的临时出生点。
    // 因此美术关卡没有 PlayerStart 时，复用正式的出生区域作为可靠回退。
    AGGJSpawnZone* BestZone = nullptr;
    for (TActorIterator<AGGJSpawnZone> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && It->IsSpawnEnabled()
            && (!BestZone || It->SpawnOrder < BestZone->SpawnOrder))
        {
            BestZone = *It;
        }
    }
    if (BestZone)
    {
        UE_LOG(LogTemp, Display,
            TEXT("PhysicalAnimationGameMode: no PlayerStart; using spawn zone %s."),
            *BestZone->GetName());
    }
    return BestZone;
}

APawn* AGGJPhysicalAnimationGameMode::SpawnDefaultPawnAtTransform_Implementation(
    AController* NewPlayer, const FTransform& SpawnTransform)
{
    if (!DefaultPawnClass || !GetWorld()) { return nullptr; }

    FActorSpawnParameters SpawnInfo;
    SpawnInfo.Instigator = GetInstigator();
    SpawnInfo.ObjectFlags |= RF_Transient;
    SpawnInfo.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    return GetWorld()->SpawnActor<APawn>(DefaultPawnClass, SpawnTransform, SpawnInfo);
}

void AGGJPhysicalAnimationGameMode::StartPlay()
{
    Super::StartPlay();

    // 支持设计师在关卡手动放置可调实例；没有时才自动创建默认管理器。
    for (TActorIterator<AGGJCharacterGroupManager> It(GetWorld()); It; ++It)
    {
        GroupManager = *It;
        break;
    }
    if (!GroupManager && GroupManagerClass)
    {
        GroupManager = GetWorld()->SpawnActor<AGGJCharacterGroupManager>(GroupManagerClass,
            FTransform::Identity);
    }

    // 相机是独立 ViewTarget，Controller 继续占有 Leader，因此角色移动和物理逻辑不变。
    AGGJGroupCameraActor* FallbackCamera = nullptr;
    for (TActorIterator<AGGJGroupCameraActor> It(GetWorld()); It; ++It)
    {
        if (!FallbackCamera)
        {
            FallbackCamera = *It;
        }
        if (!GroupCameraClass || It->IsA(GroupCameraClass))
        {
            GroupCamera = *It;
            break;
        }
    }
    // 普通模式可以接受旧相机作为回退；固定模式必须使用指定的固定相机类型。
    if (!GroupCamera && bAutoSpawnGroupCamera)
    {
        GroupCamera = FallbackCamera;
    }
    if (!GroupCamera && GroupCameraClass && bAutoSpawnGroupCamera)
    {
        GroupCamera = GetWorld()->SpawnActor<AGGJGroupCameraActor>(GroupCameraClass,
            FTransform::Identity);
    }
    if (!GroupCamera && !bAutoSpawnGroupCamera)
    {
        UE_LOG(LogTemp, Error,
            TEXT("PhysicalAnimationGameMode: place a %s actor in this level for the fixed camera view."),
            *GetNameSafe(GroupCameraClass.Get()));
    }
    if (GroupManager && GroupCamera)
    {
        GroupManager->SetGroupCamera(GroupCamera);
        if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
        {
            GroupCamera->ActivateForPlayer(Controller, GroupManager, Controller->GetPawn());
        }
    }

    // 所有使用该 GameMode 的关卡天然拥有 ESC 菜单；手放蓝图子类可覆盖界面和主菜单关卡。
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
