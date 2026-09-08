#pragma once

// 独立径向地雷：检测项目人物、轻微弹开并一次性销毁。

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GGJRadialMineActor.generated.h"

class AGGJPhysicalAnimationCharacter;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable, meta=(DisplayName="Radial Push Mine"))
class GGJ_GAMEDEMO_API AGGJRadialMineActor : public AActor
{
    GENERATED_BODY()

public:
    AGGJRadialMineActor();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="地雷|组件")
    TObjectPtr<USphereComponent> TriggerSphere;

    /** 可选外观；蓝图中可直接指定任意 Static Mesh。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="地雷|组件")
    TObjectPtr<UStaticMeshComponent> MineMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="地雷|范围",
        meta=(ClampMin="10.0", Units="cm"))
    float TriggerRadius = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="地雷|范围",
        meta=(ClampMin="10.0", Units="cm"))
    float BlastRadius = 350.f;

    /** [RADIAL_MINE_PUSH] 水平速度增量，保持较小可产生轻微弹开。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="地雷|弹力",
        meta=(ClampMin="0.0", Units="cm/s"))
    float HorizontalPush = 420.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="地雷|弹力",
        meta=(ClampMin="0.0", Units="cm/s"))
    float UpwardPush = 160.f;

    /** 开启时只弹踩雷者；关闭时弹开 BlastRadius 内所有群体成员。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="地雷|规则")
    bool bAffectTriggeringCharacterOnly = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="地雷|规则")
    bool bEnabled = true;

    UFUNCTION(BlueprintPure, Category="地雷")
    bool HasTriggered() const { return bTriggered; }

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    /** [RADIAL_MINE_PUSH] 只推动角色胶囊，不直接破坏 Physical Animation 骨骼。 */
    void PushCharacter(AGGJPhysicalAnimationCharacter* Character) const;
    void TriggerMine(AGGJPhysicalAnimationCharacter* TriggeringCharacter);
    AGGJPhysicalAnimationCharacter* FindTouchingCharacter() const;

    UPROPERTY(VisibleInstanceOnly, Category="地雷|运行时")
    bool bTriggered = false;
};
