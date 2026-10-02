// TAC-OPS game target (UE 5.8)

using UnrealBuildTool;
using System.Collections.Generic;

public class TacOpsTarget : TargetRules
{
	public TacOpsTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("TacOps");
	}
}
