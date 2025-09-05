namespace GameProtogenAPI.Services.Contracts
{
    public interface ILLMService
    {
        /// <summary> Paso 1: user + escena → PLAN XML </summary>
        Task<string> BuildEditPlanXmlAsync(string userPrompt, string sceneJson, CancellationToken ct = default);

        /// <summary> Paso 2: PLAN XML → { "ops": [...] } </summary>
        Task<string> PlanToOpsJsonAsync(string planXml, CancellationToken ct = default);
    }
}
