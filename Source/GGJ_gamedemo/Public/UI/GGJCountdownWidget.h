#pragma once

// 默认的左上角毫秒倒计时界面；可直接使用，也可创建蓝图子类替换视觉。

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GGJCountdownWidget.generated.h"

class UTextBlock;

UCLASS(Blueprintable)
class GGJ_GAMEDEMO_API UGGJCountdownWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 更新显示文本。蓝图自定义 Widget 也可以覆写此事件。 */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Countdown UI")
    void SetCountdownText(const FText& NewText);
    virtual void SetCountdownText_Implementation(const FText& NewText);

    /** 名为 CountdownText 的蓝图 TextBlock 会自动绑定；不做蓝图时由 C++ 创建默认控件。 */
    UPROPERTY(BlueprintReadOnly, Category="Countdown UI", meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> CountdownText;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
