#include "Gameplay/Zones/GGJDestroyZone.h"

#include "Character/GGJCharacterGroupManager.h"
#include "Character/GGJPhysicalAnimationCharacter.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Gameplay/Zones/GGJZoneDetection.h"

AGGJDestroyZone::AGGJDestroyZone()
{
    PrimaryActorTick.bCanEverTick = false;
    DestroyBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("DestroyBounds"));
    SetRootComponent(DestroyBounds);
    DestroyBounds->SetBoxExtent(FVector(300.f, 300.f, 150.f));
    DestroyBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DestroyBounds->SetCollisionObjectType(ECC_WorldDynamic);
    DestroyBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    DestroyBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    DestroyBounds->SetGenerateOverlapEvents(true);
    DestroyBounds->SetHiddenInGame(true);
    DestroyBounds->ShapeColor = FColor(230, 55, 65);
    DestroyBounds->OnComponentBeginOverlap.AddDynamic(this, &AGGJDestroyZone::HandleBeginOverlap);
}

void AGGJDestroyZone::BeginPlay()
{
    Super::BeginPlay();
    GetWorldTimerManager().SetTimer(DetectionTimer, this,
        &AGGJDestroyZone::ScanMembersInside, FMath::Max(0.01f, DetectionInterval), true, 0.f);
}

void AGGJDestroyZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(DetectionTimer);
    Super::EndPlay(EndPlayReason);
}

void AGGJDestroyZone::ScanMembersInside()
{
    if (!bEnabled || !GetWorld()) { return; }

    // 使用成员快照，销毁过程中 Manager 的真实数组可以安全变化。
    for (TActorIterator<AGGJCharacterGroupManager> It(GetWorld()); It; ++It)
    {
        const TArray<AGGJPhysicalAnimationCharacter*> Members = It->GetMembers();
        for (AGGJPhysicalAnimationCharacter* Character : Members)
        {
            if (GGJZoneDetection::IsActorOriginInsideBox(DestroyBounds, Character, DetectionPadding))
            {
                TryDestroyCharacter(Character);
            }
        }
        break;
    }
}

void AGGJDestroyZone::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    TryDestroyCharacter(Cast<AGGJPhysicalAnimationCharacter>(OtherActor));
}

void AGGJDestroyZone::TryDestroyCharacter(AGGJPhysicalAnimationCharacter* Character)
{
    if (!bEnabled || !IsValid(Character)) { return; }
    AGGJCharacterGroupManager* Manager = Character ? Character->GetCharacterGroupManager() : nullptr;
    if (!Character || !Manager || (!bCanDestroyLeader && Manager->GetLeader() == Character))
    {
        return;
    }

    // 先广播，让蓝图仍能读取即将销毁角色的信息；随后由 Manager 处理 Leader 移交和销毁。
    OnMemberDestroyed.Broadcast(Character);
    Manager->DestroyMember(Character);
}
