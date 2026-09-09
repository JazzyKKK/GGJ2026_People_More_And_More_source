#include "PartyExperiment/GGJPartyCameraActor.h"

#include "PartyExperiment/GGJPartyPopulationGroup.h"

void AGGJPartyCameraActor::SetTrackedGroups(
    const TArray<AGGJPartyPopulationGroup*>& NewGroups)
{
    TrackedGroups.Reset();
    for (AGGJPartyPopulationGroup* Group : NewGroups)
    {
        AddTrackedGroup(Group);
    }
}

void AGGJPartyCameraActor::AddTrackedGroup(AGGJPartyPopulationGroup* NewGroup)
{
    if (!IsValid(NewGroup))
    {
        return;
    }
    const bool bAlreadyTracked = TrackedGroups.ContainsByPredicate(
        [NewGroup](const TWeakObjectPtr<AGGJPartyPopulationGroup>& Entry)
        {
            return Entry.Get() == NewGroup;
        });
    if (!bAlreadyTracked)
    {
        TrackedGroups.Add(NewGroup);
    }
}

void AGGJPartyCameraActor::RemoveTrackedGroup(AGGJPartyPopulationGroup* Group)
{
    TrackedGroups.RemoveAll(
        [Group](const TWeakObjectPtr<AGGJPartyPopulationGroup>& Entry)
        {
            return !Entry.IsValid() || Entry.Get() == Group;
        });
}

void AGGJPartyCameraActor::GatherTrackedMembers(
    TArray<AGGJPhysicalAnimationCharacter*>& OutMembers) const
{
    OutMembers.Reset();
    for (const TWeakObjectPtr<AGGJPartyPopulationGroup>& Group : TrackedGroups)
    {
        if (const AGGJPartyPopulationGroup* ValidGroup = Group.Get())
        {
            OutMembers.Append(ValidGroup->GetMembers());
        }
    }
}
