#pragma once

// 角色进入后立即从群体中安全移除的无模型触发区域。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "GGJDestroyZone.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class AGGJPhysicalAnimationCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGGJMemberDestroyedSignature,
    AGGJPhysicalAnimationCharacter*, DestroyedMember);

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJDestroyZone : public AActor
{
    GENERATED_BODY()

public:
    AGGJDestroyZone();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Destroy")
    bool bEnabled = true;

    /** 开启时 Leader 也可被销毁；若还有成员，Manager 会自动移交控制权。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Destroy")
    bool bCanDestroyLeader = true;

    /** 在 Box 边界外增加少量检测容差；检测角色 Actor 原点，不依赖碰撞事件。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Destroy",
        meta=(ClampMin="0.0", Units="cm"))
    float DetectionPadding = 40.f;

    /** 碰撞事件失效时的几何扫描间隔。0.05 秒已足够及时且开销很低。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gameplay Zone|Destroy",
        meta=(ClampMin="0.01", ClampMax="1.0", Units="s"))
    float DetectionInterval = 0.05f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gameplay Zone|Components")
    TObjectPtr<UBoxComponent> DestroyBounds;

    UPROPERTY(BlueprintAssignable, Category="Gameplay Zone|Events")
    FGGJMemberDestroyedSignature OnMemberDestroyed;

private:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void ScanMembersInside();
    void TryDestroyCharacter(AGGJPhysicalAnimationCharacter* Character);

    FTimerHandle DetectionTimer;

    UFUNCTION()
    void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);
};
