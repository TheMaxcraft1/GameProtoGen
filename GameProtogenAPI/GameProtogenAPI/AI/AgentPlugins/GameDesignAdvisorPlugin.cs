using GameProtogenAPI.Services.Contracts;
using Microsoft.SemanticKernel;
using System.ComponentModel;

namespace GameProtogenAPI.AI.AgentPlugins
{
    public class GameDesignAdvisorPlugin
    {
        private readonly ILLMService _llm;

        public GameDesignAdvisorPlugin(ILLMService llm)
        {
            _llm = llm;
        }

        [KernelFunction, Description("Responde preguntas de diseño de videojuegos (2D, prototipado).")]
        public async Task<string> ask(
            [Description("Pregunta del usuario sobre diseño de juegos")] string question,
            CancellationToken ct)
        {
            return await _llm.AskDesignAsync(question, ct);
        }
    }
}
