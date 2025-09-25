using GameProtogenAPI.Services.Contracts;
using Microsoft.SemanticKernel;
using OpenAI.Images;
using System.ComponentModel;
using System.Text.Json;

namespace GameProtogenAPI.AI.AgentPlugins
{
    public class AssetGenPlugin
    {
        private readonly ILogger<AssetGenPlugin> _logger;
        private readonly ILLMService _llm;

        public AssetGenPlugin(ILLMService llm, ILogger<AssetGenPlugin> logger)
        {
            _logger = logger;
            _llm = llm;
        }

        [KernelFunction("generate_asset")]
        [Description("Genera una imagen (textura/sprite) y la devuelve en base64 para que el cliente la guarde localmente.")]
        public async Task<string> GenerateAsync(
            [Description("Descripción de la textura a generar")] string prompt,
            [Description("Modo explícito: 'sprite' o 'texture' (opcional)")] string? assetMode = null,
            CancellationToken ct = default)
        {
            return await _llm.GenerateAssetAsync(prompt, assetMode, ct);
        }
    }
}
