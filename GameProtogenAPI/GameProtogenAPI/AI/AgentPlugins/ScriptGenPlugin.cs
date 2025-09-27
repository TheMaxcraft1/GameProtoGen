using GameProtogenAPI.Services.Contracts;
using Microsoft.SemanticKernel;
using System.ComponentModel;

namespace GameProtogenAPI.AI.AgentPlugins
{
    public class ScriptGenPlugin
    {
        private readonly ILLMService _llm;
        public ScriptGenPlugin(ILLMService llm) { _llm = llm; }

        [KernelFunction("generate_lua")]
        [Description("Genera código Lua para la VM del engine (usa ecs.*, on_spawn, on_update). Devuelve JSON { kind:'lua', fileName, code }.")]
        public Task<string> GenerateAsync(
            [Description("Pedido del usuario (comportamiento, IA simple, movimiento, triggers, etc.)")] string prompt,
            [Description("Escena JSON serializada para dar contexto (opcional)")] string? sceneJson = null,
            CancellationToken ct = default)
            => _llm.GenerateLuaAsync(prompt, sceneJson ?? "{}", ct);
    
    }
}
