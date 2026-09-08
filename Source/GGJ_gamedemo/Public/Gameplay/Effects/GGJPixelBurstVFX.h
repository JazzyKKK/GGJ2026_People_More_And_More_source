#pragma once

// 独立、一次性像素爆炸。由人物进入触发球启动，不参与伤害或受力。

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GGJPixelBurstVFX.generated.h"

class AGGJPhysicalAnimationCharacter;
class USphereComponent;

UCLASS(NotBlueprintable)
class GGJ_GAMEDEMO_API UGGJPixelBurstRenderComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    UGGJPixelBurstRenderComponent();
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

    bool bBurstActive = false;
    float StartWorldTime = 0.f;
    float Duration = 0.65f;
    float BurstRadius = 320.f;
    float PixelSize = 6.f;
    float GridSize = 16.f;
    float UpwardBias = 0.2f;
    float StreakRatio = 0.22f;
    int32 ParticleCount = 80;
    int32 RandomSeed = 2026;
    FLinearColor PrimaryColor = FLinearColor(0.4f, 0.9f, 1.f, 0.9f);
    FLinearColor SecondaryColor = FLinearColor(0.9f, 1.f, 1.f, 0.7f);
};

UCLASS(Blueprintable, meta=(DisplayName="One Shot Pixel Burst VFX"))
class GGJ_GAMEDEMO_API AGGJPixelBurstVFX : public AActor
{
    GENERATED_BODY()

public:
    AGGJPixelBurstVFX();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="像素爆炸|组件")
    TObjectPtr<USphereComponent> TriggerSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="像素爆炸|组件")
    TObjectPtr<UGGJPixelBurstRenderComponent> PixelRenderer;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|触发",
        meta=(ClampMin="10.0", Units="cm"))
    float TriggerRadius = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|范围",
        meta=(ClampMin="10.0", Units="cm"))
    float BurstRadius = 320.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|时间",
        meta=(ClampMin="0.05", Units="s"))
    float Duration = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|外观",
        meta=(ClampMin="8", ClampMax="512"))
    int32 ParticleCount = 80;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|外观",
        meta=(ClampMin="1.0", ClampMax="32.0"))
    float PixelSize = 6.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|外观",
        meta=(ClampMin="1.0", Units="cm"))
    float GridSize = 16.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|运动",
        meta=(ClampMin="-1.0", ClampMax="1.0"))
    float UpwardBias = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|外观",
        meta=(ClampMin="0.0", ClampMax="1.0"))
    float StreakRatio = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|颜色")
    FLinearColor PrimaryColor = FLinearColor(0.4f, 0.9f, 1.f, 0.9f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|颜色")
    FLinearColor SecondaryColor = FLinearColor(0.9f, 1.f, 1.f, 0.7f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="像素爆炸|外观")
    int32 RandomSeed = 2026;

    UFUNCTION(BlueprintCallable, Category="像素爆炸")
    void TriggerBurst();

    UFUNCTION(BlueprintPure, Category="像素爆炸")
    bool HasTriggered() const { return bTriggered; }

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

    void SynchronizeRenderer();
    AGGJPhysicalAnimationCharacter* FindTouchingCharacter() const;

    UPROPERTY(VisibleInstanceOnly, Category="像素爆炸|运行时")
    bool bTriggered = false;
};
