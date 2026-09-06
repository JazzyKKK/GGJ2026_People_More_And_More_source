// 编辑器目标（Editor Target）构建规则。
// 在 Visual Studio 选择“Development Editor”或打开 UE 编辑器时会使用本文件。

using UnrealBuildTool;
using System.Collections.Generic;

public class GGJ_gamedemoEditorTarget : TargetRules
{
	public GGJ_gamedemoEditorTarget( TargetInfo Target) : base(Target)
	{
		// 在游戏运行时代码之外，再链接 Unreal Editor 所需功能。
		Type = TargetType.Editor;
		// 与游戏目标保持相同的编译及头文件包含规则。
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		// 编辑器中同样加载项目的主运行时模块。
		ExtraModuleNames.Add("GGJ_gamedemo");
	}
}
