#pragma once

// 双人/多人同屏实验共用的数据类型。
// 这里只保存配置，不包含关卡状态，方便以后把键盘方案替换为手柄或网络玩家。

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GGJPartyTypes.generated.h"

class AGGJPhysicalAnimationCharacter;

/** 一名派对玩家对应的人群配置；数组中再添加元素即可扩展第三、第四名玩家。 */
USTRUCT(BlueprintType)
struct FGGJPartyPlayerSetup
{
    GENERATED_BODY()

    /** 稳定的玩家编号。默认 0=P1，1=P2；它也用于匹配出生区域。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Player", meta=(ClampMin="0"))
    int32 PlayerIndex = 0;

    /** 开局立即生成的人数。游戏过程中的增长会改用逐个掉落队列。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Player",
        meta=(ClampMin="0", ClampMax="200"))
    int32 InitialPopulation = 3;

    /** 当前人数与尚在生成队列中的人数之和不能超过该值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Player",
        meta=(ClampMin="1", ClampMax="500"))
    int32 MaxPopulation = 30;

    /** 留空时使用 GameMode 的 DefaultMemberClass。通常选择现有物理动画人物蓝图。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Player")
    TSubclassOf<AGGJPhysicalAnimationCharacter> MemberClass;

    /** 为以后按玩家区分轮廓颜色预留；1~255 会写入角色 Custom Stencil。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Player",
        meta=(ClampMin="1", ClampMax="255", UIMin="1", UIMax="255"))
    int32 OcclusionStencilValue = 1;
};

/**
 * 同一块键盘上的一套方向键和跳跃键。
 * PlayerController 每帧遍历该数组，因此以后增加 IJKL 等方案不需要新增控制代码。
 */
USTRUCT(BlueprintType)
struct FGGJPartyKeyboardControlScheme
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Input")
    bool bEnabled = true;

    /** 必须与 GameMode.PlayerSetups 中的 PlayerIndex 对应。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Input", meta=(ClampMin="0"))
    int32 PlayerIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Input")
    FKey MoveUpKey = EKeys::W;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Input")
    FKey MoveDownKey = EKeys::S;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Input")
    FKey MoveLeftKey = EKeys::A;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Input")
    FKey MoveRightKey = EKeys::D;

    /**
     * 该玩家整组人物的跳跃键。留空时使用兼容默认值：玩家0=Space、玩家1=Right Shift。
     * 这样旧的蓝图数组在新增本字段后也能直接获得正确的双人跳跃键。
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Party Input")
    FKey JumpKey;
};
