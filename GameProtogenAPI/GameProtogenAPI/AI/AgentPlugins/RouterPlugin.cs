using GameProtogenAPI.Services.Contracts;
using Microsoft.SemanticKernel;
using System.ComponentModel;

namespace GameProtogenAPI.AI.AgentPlugins
{
    public class RouterPlugin
    {
        private readonly ILLMService _llm;
        public RouterPlugin(ILLMService llm) { _llm = llm; }

        [KernelFunction, Description("Clasifica el prompt y devuelve JSON con los agentes a ejecutar")]
        public Task<string> route(
            [Description("Prompt del usuario")] string prompt,
            [Description("Escena JSON actual")] string sceneJson,
            CancellationToken ct)
            => _llm.RouteAsync(prompt, sceneJson, ct);
    }
}
