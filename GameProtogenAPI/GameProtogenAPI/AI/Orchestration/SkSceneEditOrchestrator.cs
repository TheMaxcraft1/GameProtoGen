using GameProtogenAPI.AI.AgentPlugins;
using GameProtogenAPI.AI.Orchestration.Contracts;
using Microsoft.SemanticKernel;
using System.Text;
using System.Text.Json;

namespace GameProtogenAPI.AI.Orchestration
{
    public class SkSceneEditOrchestrator : ISkSceneEditOrchestrator
    {
        private readonly Kernel _kernel;
        private const string KIND_TEXT = "text";
        private const string KIND_OPS = "ops";
        private const string KIND_BUNDLE = "bundle";

        public SkSceneEditOrchestrator(Kernel kernel)
        {
            _kernel = kernel;
        }

        public async Task<string> RunAsync(string prompt, string sceneJson, CancellationToken ct)
        {
            // Step 0: Router (LLM)
            var routeJson = await InvokeRouterAsync(prompt, sceneJson, ct);

            var agents = ParseAgentsOrDefault(routeJson);
            if (agents.Length <= 1)
                return await ExecuteSingleAgentAsync(agents.First(), prompt, sceneJson, ct);

            // multi-agente → bundle
            return await ExecuteAgentsBundleAsync(agents, prompt, sceneJson, ct);
        }

        // ───────────────────────── Router ─────────────────────────
        private async Task<string> InvokeRouterAsync(string prompt, string sceneJson, CancellationToken ct)
        {
            try
            {
                return await _kernel.InvokeAsync<string>(
                    pluginName: nameof(RouterPlugin),
                    functionName: "route",
                    arguments: new()
                    {
                        ["prompt"] = prompt,
                        ["sceneJson"] = sceneJson
                    },
                    cancellationToken: ct
                );
            }
            catch (Exception ex)
            {
                // Fallback seguro: si el router falla, asumimos design
                return "{\"agents\":[\"design_qa\"],\"reason\":\"router-fallback: " + JsonSerializer.Serialize(ex.Message) + "\"}";
            }
        }

        private static string[] ParseAgentsOrDefault(string routeJson)
        {
            try
            {
                using var rdoc = JsonDocument.Parse(string.IsNullOrWhiteSpace(routeJson) ? "{}" : routeJson);
                if (!rdoc.RootElement.TryGetProperty("agents", out var arr) ||
                    arr.ValueKind != JsonValueKind.Array || arr.GetArrayLength() == 0)
                    return new[] { "scene_edit" };

                var xs = arr.EnumerateArray()
                            .Where(e => e.ValueKind == JsonValueKind.String)
                            .Select(e => e.GetString()!)
                            .Where(s => !string.IsNullOrWhiteSpace(s))
                            .ToArray();
                return xs.Length == 0 ? new[] { "scene_edit" } : xs;
            }
            catch
            {
                return new[] { "scene_edit" };
            }
        }

        // ───────────────────────── Ejecución single ─────────────────────────
        private async Task<string> ExecuteSingleAgentAsync(string agent, string prompt, string sceneJson, CancellationToken ct)
        {
            if (string.Equals(agent, "design_qa", StringComparison.OrdinalIgnoreCase))
                return await RunDesignQaAsync(prompt, ct);

            // default: scene_edit
            return await RunSceneEditAsync(prompt, sceneJson, ct);
        }

        // ───────────────────────── Ejecución bundle ─────────────────────────
        private async Task<string> ExecuteAgentsBundleAsync(string[] agents, string prompt, string sceneJson, CancellationToken ct)
        {
            var items = new List<JsonElement>();

            foreach (var a in agents)
            {
                if (string.Equals(a, "design_qa", StringComparison.OrdinalIgnoreCase))
                {
                    var item = await TryRunDesignQaAsItemAsync(prompt, ct);
                    items.Add(item);
                }
                else if (string.Equals(a, "scene_edit", StringComparison.OrdinalIgnoreCase))
                {
                    var item = await TryRunSceneEditAsItemAsync(prompt, sceneJson, ct);
                    items.Add(item);
                }
                // (extensible: code_gen / image_gen aquí)
            }

            // { kind:"bundle", items:[...] }
            using var msb = new MemoryStream();
            using (var wb = new Utf8JsonWriter(msb))
            {
                wb.WriteStartObject();
                wb.WriteString("kind", KIND_BUNDLE);
                wb.WritePropertyName("items");
                wb.WriteStartArray();
                foreach (var it in items) it.WriteTo(wb);
                wb.WriteEndArray();
                wb.WriteEndObject();
            }
            return Encoding.UTF8.GetString(msb.ToArray());
        }

        // ───────────────────────── design_qa ─────────────────────────
        private async Task<string> RunDesignQaAsync(string prompt, CancellationToken ct)
        {
            try
            {
                var answer = await _kernel.InvokeAsync<string>(
                    pluginName: nameof(DesignAdvisorPlugin),
                    functionName: "ask",
                    arguments: new() { ["question"] = prompt },
                    cancellationToken: ct
                );
                return WrapText(answer ?? string.Empty);
            }
            catch (Exception ex)
            {
                return WrapText($"No pude responder la consulta de diseño: {ex.Message}");
            }
        }

        private async Task<JsonElement> TryRunDesignQaAsItemAsync(string prompt, CancellationToken ct)
        {
            var json = await RunDesignQaAsync(prompt, ct); // ya devuelve {kind:"text",...}
            return JsonDocument.Parse(json).RootElement.Clone();
        }

        // ───────────────────────── scene_edit ─────────────────────────
        private async Task<string> RunSceneEditAsync(string prompt, string sceneJson, CancellationToken ct)
        {
            try
            {
                var plan = await BuildPlanAsync(prompt, sceneJson, ct);
                if (string.IsNullOrWhiteSpace(plan))
                    return WrapText("No se pudo generar el plan.");

                var opsJson = await PlanToOpsAsync(plan, ct);
                if (!HasOps(opsJson))
                    return WrapText("El synthesizer no devolvió 'ops'.");

                return WrapOps(opsJson);
            }
            catch (Exception ex)
            {
                return WrapText($"Error en scene_edit: {ex.Message}");
            }
        }

        private async Task<JsonElement> TryRunSceneEditAsItemAsync(string prompt, string sceneJson, CancellationToken ct)
        {
            var json = await RunSceneEditAsync(prompt, sceneJson, ct); // {kind:"ops"| "text",...}
            return JsonDocument.Parse(json).RootElement.Clone();
        }

        private async Task<string> BuildPlanAsync(string prompt, string sceneJson, CancellationToken ct)
        {
            return await _kernel.InvokeAsync<string>(
                pluginName: nameof(PlannerPlugin),
                functionName: "build_plan_xml",
                arguments: new()
                {
                    ["prompt"] = prompt,
                    ["sceneJson"] = sceneJson
                },
                cancellationToken: ct
            );
        }

        private async Task<string> PlanToOpsAsync(string planXml, CancellationToken ct)
        {
            return await _kernel.InvokeAsync<string>(
                pluginName: nameof(SynthesizerPlugin),
                functionName: "plan_to_ops",
                arguments: new() { ["planXml"] = planXml },
                cancellationToken: ct
            );
        }

        private static bool HasOps(string opsJson)
        {
            try
            {
                using var doc = JsonDocument.Parse(opsJson);
                return doc.RootElement.TryGetProperty("ops", out _);
            }
            catch { return false; }
        }

        // ───────────────────────── Wrappers JSON ─────────────────────────
        private static string WrapText(string message)
        {
            using var ms = new MemoryStream();
            using (var w = new Utf8JsonWriter(ms))
            {
                w.WriteStartObject();
                w.WriteString("kind", KIND_TEXT);
                w.WriteString("message", message ?? string.Empty);
                w.WriteEndObject();
            }
            return Encoding.UTF8.GetString(ms.ToArray());
        }

        private static string WrapOps(string opsJson)
        {
            using var input = JsonDocument.Parse(opsJson);
            using var ms = new MemoryStream();
            using (var w = new Utf8JsonWriter(ms))
            {
                w.WriteStartObject();
                w.WriteString("kind", KIND_OPS);
                foreach (var p in input.RootElement.EnumerateObject()) p.WriteTo(w);
                w.WriteEndObject();
            }
            return Encoding.UTF8.GetString(ms.ToArray());
        }
    }
}
