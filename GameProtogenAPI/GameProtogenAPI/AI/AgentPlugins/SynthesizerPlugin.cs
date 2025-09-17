using GameProtogenAPI.Services.Contracts;
using Microsoft.SemanticKernel;
using System.ComponentModel;

namespace GameProtogenAPI.AI.AgentPlugins
{
    public class SynthesizerPlugin
    {
        private readonly ILLMService _llm;
        public SynthesizerPlugin(ILLMService llm) { _llm = llm; }

        [KernelFunction("plan_to_ops")]
        [Description("Convierte un <plan> XML en JSON { \"ops\": [...] } válido para el editor")]
        public async Task<string> PlanToOpsAsync(
            [Description("Plan XML")] string planXml,
            CancellationToken ct = default)
        {
            var opsJson = await _llm.PlanToOpsJsonAsync(planXml, ct);
            return opsJson; // {"ops":[...]}
        }
    }
}
