#include "PartyExperiment/GGJPartyGameMode.h"

#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "PartyExperiment/GGJPartyCameraActor.h"
#include "PartyExperiment/GGJPartyPlayerController.h"
#include "PartyExperiment/GGJPartyPopulationGroup.h"
#include "PartyExperiment/Zones/GGJPartySpawnZone.h"

AGGJPartyGameMode::AGGJPartyGameMode()
{
    // 本模式不占有任何小人；唯一 PlayerController 读取整块键盘并分别驱动多组角色。
    DefaultPawnClass = nullptr;
    PlayerControllerClass = AGGJPartyPlayerController::StaticClass();
    PopulationGroupClass = AGGJPartyPopulationGroup::StaticClass();
    PartyCameraClass = AGGJPartyCameraActor::StaticClass();
    DefaultMemberClass = AGGJPhysicalAnimationCharacter::StaticClass();

    FGGJPartyPlayerSetup PlayerOne;
    PlayerOne.PlayerIndex = 0;
    PlayerOne.InitialPopulation = 3;
    PlayerOne.OcclusionStencilValue = 1;
    PlayerSetups.Add(PlayerOne);

    FGGJPartyPlayerSetup PlayerTwo;
    PlayerTwo.PlayerIndex = 1;
    PlayerTwo.InitialPopulation = 3;
    PlayerTwo.OcclusionStencilValue = 2;
    PlayerSetups.Add(PlayerTwo);
}

void AGGJPartyGameMode::StartPlay()
{
    Super::StartPlay();
    CreatePopulationGroups();
    CreateAndActivateCamera();
}

void AGGJPartyGameMode::CreatePopulationGroups()
{
    if (!GetWorld())
    {
        return;
    }

    TArray<AGGJPartySpawnZone*> SpawnZones;
    for (TActorIterator<AGGJPartySpawnZone> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && It->IsSpawnEnabled())
        {
            SpawnZones.Add(*It);
        }
    }

    // 支持设计师手动放置蓝图子类的人口组；同一 PlayerIndex 没有实例时才自动生成。
    TArray<AGGJPartyPopulationGroup*> PlacedGroups;
    for (TActorIterator<AGGJPartyPopulationGroup> It(GetWorld()); It; ++It)
    {
        PlacedGroups.Add(*It);
    }

    TSet<int32> CreatedPlayerIndices;
    for (const FGGJPartyPlayerSetup& Setup : PlayerSetups)
    {
        const int32 SafePlayerIndex = FMath::Max(0, Setup.PlayerIndex);
        if (CreatedPlayerIndices.Contains(SafePlayerIndex))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("PartyGameMode ignored duplicate PlayerIndex %d in PlayerSetups."),
                SafePlayerIndex);
            continue;
        }
        CreatedPlayerIndices.Add(SafePlayerIndex);

        AGGJPartyPopulationGroup* Group = nullptr;
        for (AGGJPartyPopulationGroup* Candidate : PlacedGroups)
        {
            if (IsValid(Candidate) && Candidate->PlayerIndex == SafePlayerIndex
                && !ActiveGroups.Contains(Candidate))
            {
                Group = Candidate;
                break;
            }
        }

        if (!Group && PopulationGroupClass)
        {
            FVector ManagerLocation(0.f, SafePlayerIndex * 700.f, 200.f);
            for (AGGJPartySpawnZone* Zone : SpawnZones)
            {
                if (IsValid(Zone) && Zone->PlayerIndex == SafePlayerIndex)
                {
                    ManagerLocation = Zone->GetActorLocation();
                    break;
                }
            }
            Group = GetWorld()->SpawnActor<AGGJPartyPopulationGroup>(PopulationGroupClass,
                FTransform(FRotator::ZeroRotator, ManagerLocation));
        }

        if (!Group)
        {
            UE_LOG(LogTemp, Error,
                TEXT("PartyGameMode could not create population group for player %d."),
                SafePlayerIndex + 1);
            continue;
        }
        Group->InitializeGroup(Setup, SpawnZones, DefaultMemberClass);
        ActiveGroups.Add(Group);
    }
}

void AGGJPartyGameMode::CreateAndActivateCamera()
{
    if (!GetWorld())
    {
        return;
    }

    // 手放相机优先，方便在 BP 子类中设置材质、FOV 和平滑参数。
    for (TActorIterator<AGGJPartyCameraActor> It(GetWorld()); It; ++It)
    {
        PartyCamera = *It;
        break;
    }
    if (!PartyCamera && bAutoSpawnPartyCamera && PartyCameraClass)
    {
        PartyCamera = GetWorld()->SpawnActor<AGGJPartyCameraActor>(PartyCameraClass,
            FTransform::Identity);
    }
    if (!PartyCamera)
    {
        UE_LOG(LogTemp, Error,
            TEXT("PartyGameMode requires a PartyCameraActor or bAutoSpawnPartyCamera=true."));
        return;
    }

    PartyCamera->SetTrackedGroups(GetAllPlayerGroups());
    if (APlayerController* Controller = GetWorld()->GetFirstPlayerController())
    {
        AActor* FallbackTarget = ActiveGroups.IsEmpty() ? nullptr : ActiveGroups[0].Get();
        PartyCamera->ActivateForPlayer(Controller, nullptr, FallbackTarget);
    }
}

AGGJPartyPopulationGroup* AGGJPartyGameMode::GetPlayerGroup(const int32 PlayerIndex) const
{
    for (AGGJPartyPopulationGroup* Group : ActiveGroups)
    {
        if (IsValid(Group) && Group->GetPlayerIndex() == PlayerIndex)
        {
            return Group;
        }
    }
    return nullptr;
}

TArray<AGGJPartyPopulationGroup*> AGGJPartyGameMode::GetAllPlayerGroups() const
{
    TArray<AGGJPartyPopulationGroup*> Result;
    Result.Reserve(ActiveGroups.Num());
    for (AGGJPartyPopulationGroup* Group : ActiveGroups)
    {
        if (IsValid(Group))
        {
            Result.Add(Group);
        }
    }
    return Result;
}

int32 AGGJPartyGameMode::GetPlayerPopulationCount(const int32 PlayerIndex) const
{
    const AGGJPartyPopulationGroup* Group = GetPlayerGroup(PlayerIndex);
    return Group ? Group->GetPopulationCount() : 0;
}

int32 AGGJPartyGameMode::GetTotalPopulationCount() const
{
    int32 Total = 0;
    for (const AGGJPartyPopulationGroup* Group : ActiveGroups)
    {
        if (IsValid(Group))
        {
            Total += Group->GetPopulationCount();
        }
    }
    return Total;
}

int32 AGGJPartyGameMode::GetOwningPlayerIndex(
    const AGGJPhysicalAnimationCharacter* Character) const
{
    for (const AGGJPartyPopulationGroup* Group : ActiveGroups)
    {
        if (IsValid(Group) && Group->ContainsMember(Character))
        {
            return Group->GetPlayerIndex();
        }
    }
    return INDEX_NONE;
}

int32 AGGJPartyGameMode::AddPopulationForPlayer(const int32 PlayerIndex,
    const int32 Amount, const FVector SpawnCenter)
{
    if (AGGJPartyPopulationGroup* Group = GetPlayerGroup(PlayerIndex))
    {
        return Group->AddPopulation(Amount, SpawnCenter);
    }
    return 0;
}

int32 AGGJPartyGameMode::AddPopulationAtSpawnZone(AGGJPartySpawnZone* SpawnZone,
    const int32 Amount)
{
    if (!IsValid(SpawnZone))
    {
        return 0;
    }
    if (AGGJPartyPopulationGroup* Group = GetPlayerGroup(SpawnZone->PlayerIndex))
    {
        return Group->AddPopulationAtSpawnZone(Amount, SpawnZone);
    }
    return 0;
}

int32 AGGJPartyGameMode::RequestPopulationDeltaForPlayer(const int32 PlayerIndex,
    const int32 Delta, const FVector SpawnCenter)
{
    if (AGGJPartyPopulationGroup* Group = GetPlayerGroup(PlayerIndex))
    {
        return Group->RequestPopulationDelta(Delta, SpawnCenter);
    }
    return 0;
}

void AGGJPartyGameMode::ApplyPlayerScreenMovement(const int32 PlayerIndex,
    FVector2D ScreenMovement)
{
    AGGJPartyPopulationGroup* Group = GetPlayerGroup(PlayerIndex);
    if (!Group)
    {
        return;
    }
    if (ScreenMovement.ContainsNaN())
    {
        ScreenMovement = FVector2D::ZeroVector;
    }
    ScreenMovement = ScreenMovement.GetClampedToMaxSize(1.f);
    const FVector WorldMovement = PartyCamera
        ? PartyCamera->ScreenInputToWorld(ScreenMovement)
        : FVector(ScreenMovement.Y, ScreenMovement.X, 0.f);
    Group->ApplySharedMovement(WorldMovement);
}

bool AGGJPartyGameMode::RequestSharedCameraTurn(const int32 Direction)
{
    return PartyCamera && PartyCamera->RequestQuarterTurn(Direction);
}
