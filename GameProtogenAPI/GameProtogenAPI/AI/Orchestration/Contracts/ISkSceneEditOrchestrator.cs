namespace GameProtogenAPI.AI.Orchestration.Contracts
{
    public interface ISkSceneEditOrchestrator
    {
        Task<string> RunAsync(string prompt, string sceneJson, CancellationToken ct);
    }
}
