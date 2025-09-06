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
        private readonly ILLMService _llm;

        public ChatController(ILLMService llm)
        {
            _llm = llm;
        }

        [HttpPost("command")]
        public async Task<IActionResult> Command([FromBody] ChatCommandRequest req, CancellationToken ct)
        {
            if (string.IsNullOrWhiteSpace(req.prompt))
                return BadRequest(new { error = "prompt vacío" });

            // Si vino objeto, usamos su JSON crudo; sino "{}"
            var sceneJson =
                (req.scene.HasValue &&
                 req.scene.Value.ValueKind != JsonValueKind.Undefined &&
                 req.scene.Value.ValueKind != JsonValueKind.Null)
                ? req.scene.Value.GetRawText()
                : "{}";

            // 1) NANO → plan XML
            string planXml;
            try
            {
                planXml = await _llm.BuildEditPlanXmlAsync(req.prompt, sceneJson, ct);
            }
            catch (Exception ex)
            {
                return StatusCode(502, new { error = "planner_failed" });
            }

            // 2) MINI/4.1/5 → ops JSON
            string opsJson;
            try
            {
                opsJson = await _llm.PlanToOpsJsonAsync(planXml, ct);
            }
            catch (Exception ex)
            {
                return StatusCode(502, new { error = "synth_failed" });
            }

            // 3) Validación de salida antes de responder al editor
            using var doc = JsonDocument.Parse(opsJson);
            var root = doc.RootElement;
            if (!OpsValidator.TryValidateOps(root, out string err))
            {
                // MVP: devolvemos ops vacíos + error (el editor no se cae)
                return Ok(new { ops = Array.Empty<object>(), error = $"ops inválidos: {err}" });
            }

            // 4) Passthrough del JSON tal cual (evita reserializar y cambiar formato)
            return new ContentResult
            {
                Content = opsJson,
                ContentType = "application/json",
                StatusCode = 200
            };
            // Si preferís devolver un objeto ya parseado:
            // return Ok(JsonSerializer.Deserialize<object>(opsJson));
        }
    }
}
