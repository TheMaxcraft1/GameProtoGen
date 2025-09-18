using GameProtogenAPI.AI.Orchestration.Contracts;
using GameProtogenAPI.Services.Contracts;
using GameProtogenAPI.Validators;
using Microsoft.AspNetCore.Mvc;
using System.Text;
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

            // sceneJson robusto (evita null/undefined)
            string sceneJson = "{}";
            try
            {
                var v = req.scene.Value;
                if (v.ValueKind != JsonValueKind.Undefined && v.ValueKind != JsonValueKind.Null)
                    sceneJson = v.GetRawText() ?? "{}";
            }
            catch { /* si no viene scene, usamos "{}" */ }

            string result;
            try
            {
                // Orquestador ya puede devolver {kind:"ops"|"text"|"bundle"}
                result = await _orchestrator.RunAsync(req.prompt, sceneJson, ct);
            }
            catch (Exception ex)
            {
                var err = JsonSerializer.Serialize(new { kind = "text", message = $"Error en orquestación: {ex.Message}" });
                return Content(err, "application/json");
            }

            // Si no vino JSON válido, lo envuelvo como texto
            JsonDocument doc;
            try { doc = JsonDocument.Parse(string.IsNullOrWhiteSpace(result) ? "{}" : result); }
            catch
            {
                var wrapped = JsonSerializer.Serialize(new { kind = "text", message = result ?? "" });
                return Content(wrapped, "application/json");
            }

            using (doc)
            {
                var root = doc.RootElement;

                // ───────────────────────── caso: bundle ─────────────────────────
                if (root.TryGetProperty("kind", out var kElem) && kElem.GetString() == "bundle")
                {
                    if (!root.TryGetProperty("items", out var arr) || arr.ValueKind != JsonValueKind.Array)
                    {
                        var bad = JsonSerializer.Serialize(new { kind = "text", message = "Bundle sin 'items'." });
                        return Content(bad, "application/json");
                    }

                    var outItems = new List<JsonElement>();
                    foreach (var item in arr.EnumerateArray())
                    {
                        if (item.ValueKind == JsonValueKind.Object &&
                            item.TryGetProperty("kind", out var ik) &&
                            ik.ValueKind == JsonValueKind.String &&
                            ik.GetString() == "ops")
                        {
                            // Validar cada bloque ops. Si falla, lo convierto en texto para no romper toda la respuesta.
                            if (!OpsValidator.TryValidateOps(item, out string err))
                            {
                                var replaced = JsonDocument
                                    .Parse(JsonSerializer.Serialize(new { kind = "text", message = $"Ops inválidas: {err}" }))
                                    .RootElement.Clone();
                                outItems.Add(replaced);
                            }
                            else outItems.Add(item.Clone());
                        }
                        else
                        {
                            // Passthrough de textos u otros tipos soportados
                            outItems.Add(item.Clone());
                        }
                    }

                    // reserializar bundle validado
                    using var msb = new MemoryStream();
                    using (var wb = new Utf8JsonWriter(msb))
                    {
                        wb.WriteStartObject();
                        wb.WriteString("kind", "bundle");
                        wb.WritePropertyName("items");
                        wb.WriteStartArray();
                        foreach (var x in outItems) x.WriteTo(wb);
                        wb.WriteEndArray();
                        wb.WriteEndObject();
                    }
                    return Content(Encoding.UTF8.GetString(msb.ToArray()), "application/json");
                }

                // ───────────────────────── caso: ops ─────────────────────────
                if (root.TryGetProperty("kind", out var kindElem) && kindElem.GetString() == "ops")
                {
                    if (!OpsValidator.TryValidateOps(root, out string err))
                    {
                        var safe = JsonSerializer.Serialize(new { kind = "text", message = $"Ops inválidas: {err}" });
                        return Content(safe, "application/json");
                    }
                    return Content(result, "application/json");
                }

                // ───────────────────────── caso: text ─────────────────────────
                if (root.TryGetProperty("kind", out kindElem) && kindElem.GetString() == "text")
                    return Content(result, "application/json");

                // ───────────────────────── compat: sin 'kind' ─────────────────────────
                if (root.TryGetProperty("ops", out _))
                {
                    // añadir kind:"ops" de forma segura y validar
                    using var ms = new MemoryStream();
                    using (var w = new Utf8JsonWriter(ms))
                    {
                        w.WriteStartObject();
                        w.WriteString("kind", "ops");
                        foreach (var p in root.EnumerateObject()) p.WriteTo(w);
                        w.WriteEndObject();
                    }
                    var withKind = Encoding.UTF8.GetString(ms.ToArray());

                    using var doc2 = JsonDocument.Parse(withKind);
                    if (!OpsValidator.TryValidateOps(doc2.RootElement, out string err))
                    {
                        var safe = JsonSerializer.Serialize(new { kind = "text", message = $"Ops inválidas: {err}" });
                        return Content(safe, "application/json");
                    }
                    return Content(withKind, "application/json");
                }

                // Si llega algo no soportado, texto defensivo
                var asText = JsonSerializer.Serialize(new { kind = "text", message = root.ToString() });
                return Content(asText, "application/json");
            }
        }
    }
}
