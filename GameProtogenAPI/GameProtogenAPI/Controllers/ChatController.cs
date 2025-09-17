using GameProtogenAPI.AI.Orchestration.Contracts;
using GameProtogenAPI.Services.Contracts;
using GameProtogenAPI.Validators;
using Microsoft.AspNetCore.Mvc;
using System.Text.Json;
using static GameProtogenAPI.Models.ChatModels;

namespace GameProtogenAPI.Controllers
{
    [Route("api/[controller]")]
    [ApiController]
    public class ChatController : ControllerBase
    {
        private readonly ISkSceneEditOrchestrator _orchestrator;

        public ChatController(ISkSceneEditOrchestrator orchestrator)
        {
            _orchestrator = orchestrator;
        }

        [HttpPost("command")]
        public async Task<IActionResult> Command([FromBody] ChatCommandRequest req, CancellationToken ct)
        {
            if (string.IsNullOrWhiteSpace(req.prompt))
                return BadRequest("prompt vacío");

            var sceneJson = req.scene.Value.GetRawText() ?? "{}";

            // --- Nuevo camino: planner+synth via SK
            var result = await _orchestrator.RunAsync(req.prompt, sceneJson, ct);

            // Si vino kind:"ops", validamos con tu OpsValidator (como antes)
            using var doc = JsonDocument.Parse(result);
            var root = doc.RootElement;

            if (root.TryGetProperty("kind", out var kind) && kind.GetString() == "ops")
            {
                if (!OpsValidator.TryValidateOps(root, out string err))
                {
                    // MVP seguro: respuesta textual en vez de romper al editor
                    var safe = $"{{\"kind\":\"answer\",\"answer\":\"Ops inválidas: {err}\"}}";
                    return Content(safe, "application/json");
                }
            }

            // Passthrough del JSON tal cual (contrato estable con el editor)
            return Content(result, "application/json");
        }
    }
}
