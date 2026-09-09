#pragma once

// 派对实验共享相机：复用旧群体相机的平滑移动、自动缩放、Q/E 转向和遮挡轮廓。

#include "CoreMinimal.h"
#include "Character/GGJGroupCameraActor.h"
#include "GGJPartyCameraActor.generated.h"

class AGGJPartyPopulationGroup;

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API AGGJPartyCameraActor : public AGGJGroupCameraActor
{
    GENERATED_BODY()

public:
    /** GameMode 在开局时一次性传入所有玩家的人群。 */
    void SetTrackedGroups(const TArray<AGGJPartyPopulationGroup*>& NewGroups);

    UFUNCTION(BlueprintCallable, Category="Party Experiment|Camera")
    void AddTrackedGroup(AGGJPartyPopulationGroup* NewGroup);

    UFUNCTION(BlueprintCallable, Category="Party Experiment|Camera")
    void RemoveTrackedGroup(AGGJPartyPopulationGroup* Group);

protected:
    virtual void GatherTrackedMembers(
        TArray<AGGJPhysicalAnimationCharacter*>& OutMembers) const override;

private:
    UPROPERTY(Transient)
    TArray<TWeakObjectPtr<AGGJPartyPopulationGroup>> TrackedGroups;
};
