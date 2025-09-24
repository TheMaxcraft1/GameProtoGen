namespace GameProtogenAPI.Services.Contracts
{
    public interface ILLMService
    {
        /// <summary> Paso 0: elige a que agents llamar</summary>
        Task<string> RouteAsync(string userPrompt, string sceneJson, CancellationToken ct = default);

        /// <summary> Si Scene Edit -> Paso 1: user + escena → PLAN XML </summary>
        Task<string> BuildEditPlanXmlAsync(string userPrompt, string sceneJson, CancellationToken ct = default);

        /// <summary> Si Scene Edit -> Paso 2: PLAN XML → { "ops": [...] } </summary>
        Task<string> PlanToOpsJsonAsync(string planXml, CancellationToken ct = default);

        /// <summary> Responde preguntas de diseño </summary>
        Task<string> AskDesignAsync(string userQuestion, CancellationToken ct = default);

        /// <summary> Genera assets (imágenes) </summary>
        Task<string> GenerateAssetAsync(string prompt, CancellationToken ct = default);

    }
}
