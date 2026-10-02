// TAC-OPS runtime module

using UnrealBuildTool;

public class TacOps : ModuleRules
{
	public TacOps(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Allow includes such as "Core/TOTypes.h" from every source file.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",
			"ProceduralMeshComponent",
			"PhysicsCore",
			"RenderCore",
			"Slate",
			"SlateCore"
		});
	}
}
