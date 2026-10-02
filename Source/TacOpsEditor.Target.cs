// TAC-OPS editor target (UE 5.8)

using UnrealBuildTool;
using System.Collections.Generic;

public class TacOpsEditorTarget : TargetRules
{
	public TacOpsEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("TacOps");
	}
}
