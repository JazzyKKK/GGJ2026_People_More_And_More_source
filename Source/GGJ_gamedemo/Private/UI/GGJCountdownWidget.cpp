#include "UI/GGJCountdownWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UGGJCountdownWidget::RebuildWidget()
{
    // 直接使用原生类时自动创建一个简洁的左上角 UI。
    // 如果蓝图子类已经在 Designer 中搭好控件，则保留蓝图的布局。
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), TEXT("CountdownRoot"));
        WidgetTree->RootWidget = RootCanvas;

        UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
            UBorder::StaticClass(), TEXT("CountdownBackground"));
        Background->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.05f, 0.86f));
        Background->SetPadding(FMargin(18.f, 10.f));

        CountdownText = WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(), TEXT("CountdownText"));
        CountdownText->SetText(FText::FromString(TEXT("00:00.000")));
        CountdownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.98f, 1.f, 1.f)));
        CountdownText->SetJustification(ETextJustify::Center);
        CountdownText->SetShadowOffset(FVector2D(1.5f, 1.5f));
        CountdownText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f));
        FSlateFontInfo Font = CountdownText->GetFont();
        Font.Size = 34;
        CountdownText->SetFont(Font);
        Background->SetContent(CountdownText);

        if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(Background))
        {
            PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
            PanelSlot->SetAlignment(FVector2D(0.f, 0.f));
            PanelSlot->SetPosition(FVector2D(28.f, 28.f));
            PanelSlot->SetAutoSize(true);
        }
    }
    return Super::RebuildWidget();
}

void UGGJCountdownWidget::SetCountdownText_Implementation(const FText& NewText)
{
    if (CountdownText)
    {
        CountdownText->SetText(NewText);
    }
}
