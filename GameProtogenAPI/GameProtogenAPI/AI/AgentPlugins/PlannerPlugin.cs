using GameProtogenAPI.Services.Contracts;
using Microsoft.SemanticKernel;
using System.ComponentModel;

namespace GameProtogenAPI.AI.AgentPlugins
{
    public class PlannerPlugin
    {
        private readonly ILLMService _llm;
        public PlannerPlugin(ILLMService llm) { _llm = llm; }

        [KernelFunction("build_plan_xml")]
        [Description("Genera un <plan> XML a partir del prompt del usuario y la escena JSON actual")]
        public async Task<string> BuildPlanXmlAsync(
            [Description("Prompt del usuario (español)")] string prompt,
            [Description("Escena JSON serializada")] string sceneJson,
            CancellationToken ct = default)
        {
            // Reusa TU pipeline existente (prompts, validaciones, logging)
            var plan = await _llm.BuildEditPlanXmlAsync(prompt, sceneJson, ct);
            return plan; // <plan>...</plan>
        }
    }
}
