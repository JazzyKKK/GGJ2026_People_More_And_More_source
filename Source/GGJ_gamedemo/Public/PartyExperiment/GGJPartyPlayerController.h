#pragma once

// 一个本地 PlayerController 同时读取多套键盘方向，避免 UE 把整块键盘只分给 LocalPlayer 0。

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PartyExperiment/GGJPartyTypes.h"
#include "GGJPartyPlayerController.generated.h"

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPartyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    AGGJPartyPlayerController();

    virtual void PlayerTick(float DeltaTime) override;

    /**
     * 默认包含 P1=WASD、P2=方向键。添加新的数组元素即可扩展 IJKL 等键盘玩家；
     * 手柄以后也可以把输入值转发给 GameMode.ApplyPlayerScreenMovement。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Input")
    TArray<FGGJPartyKeyboardControlScheme> KeyboardControlSchemes;

    /** Q/E 是共享视角的唯一旋转按键，不属于 P2 的控制方案。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Input")
    FKey RotateCameraLeftKey = EKeys::Q;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Party Experiment|Input")
    FKey RotateCameraRightKey = EKeys::E;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    float ReadDigitalAxis(const FKey& PositiveKey, const FKey& NegativeKey) const;
    void RotateCameraLeft();
    void RotateCameraRight();
};
