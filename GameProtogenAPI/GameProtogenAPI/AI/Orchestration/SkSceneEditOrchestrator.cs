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
            var assetMode = ExtractAssetMode(routeJson);
            if (agents.Length <= 1)
                return await ExecuteSingleAgentAsync(agents.First(), prompt, sceneJson, ct, assetMode);

            // multi-agente → bundle
            return await ExecuteAgentsBundleAsync(agents, prompt, sceneJson, ct, assetMode);
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
        private async Task<string> ExecuteSingleAgentAsync(string agent, string prompt, string sceneJson, CancellationToken ct, string? assetMode)
        {
            if (string.Equals(agent, "design_qa", StringComparison.OrdinalIgnoreCase))
                return await RunDesignQaAsync(prompt, ct);

            if (string.Equals(agent, "asset_gen", StringComparison.OrdinalIgnoreCase))
                return await RunAssetGenAsync(prompt, assetMode, ct);

            // default: scene_edit
            return await RunSceneEditAsync(prompt, sceneJson, ct);
        }

        // ───────────────────────── Ejecución bundle ─────────────────────────
        private async Task<string> ExecuteAgentsBundleAsync(string[] agents, string prompt, string sceneJson, CancellationToken ct, string? assetMode)
        {
            var items = new List<JsonElement>();
            var assetPaths = new List<string>();

            // 1) Corre asset_gen primero si está presente (para tener paths)
            foreach (var a in agents)
            {
                if (string.Equals(a, "asset_gen", StringComparison.OrdinalIgnoreCase))
                {
                    var json = await RunAssetGenAsync(prompt, assetMode, ct);
                    var item = JsonDocument.Parse(json).RootElement.Clone();
                    items.Add(item);
                    assetPaths.AddRange(ExtractAssetPathsFromItem(item));
                }
            }

            // 2) Luego corre scene_edit (inyectando assets si existen)
            foreach (var a in agents)
            {
                if (string.Equals(a, "design_qa", StringComparison.OrdinalIgnoreCase))
                {
                    var item = await TryRunDesignQaAsItemAsync(prompt, ct);
                    items.Add(item);
                }
                else if (string.Equals(a, "scene_edit", StringComparison.OrdinalIgnoreCase))
                {
                    var augmentedPrompt = prompt;
                    if (assetPaths.Count > 0)
                    {
                        augmentedPrompt += "\n\n[ASSETS]\n";
                        for (int i = 0; i < assetPaths.Count; i++)
                            augmentedPrompt += $"- ASSET{i}:{assetPaths[i]}\n";
                        augmentedPrompt += "Si el usuario pidió aplicarlas, usa estos ASSETs como texturePath en el plan (para entidades existentes en <modificar> y para nuevas en <agregar>).";
                    }
                    var json = await RunSceneEditAsync(augmentedPrompt, sceneJson, ct);
                    items.Add(JsonDocument.Parse(json).RootElement.Clone());
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
                    pluginName: nameof(GameDesignAdvisorPlugin),
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
            static string TrimToOuter(string s)
            {
                if (string.IsNullOrWhiteSpace(s)) return "{}";
                int i = s.IndexOf('{'); int j = s.LastIndexOf('}');
                return (i >= 0 && j >= i) ? s.Substring(i, j - i + 1) : s;
            }

            opsJson = (opsJson ?? "").Trim();
            // por si viniera con fences o texto extra
            if (opsJson.StartsWith("```"))
            {
                var nl = opsJson.IndexOf('\n');
                if (nl >= 0) opsJson = opsJson[(nl + 1)..];
                var last = opsJson.LastIndexOf("```", StringComparison.Ordinal);
                if (last >= 0) opsJson = opsJson[..last];
            }
            opsJson = TrimToOuter(opsJson);

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

        // ───────────────────────── Wrappers JSON ─────────────────────────
        private async Task<string> RunAssetGenAsync(string prompt, string? assetMode, CancellationToken ct)
        {
            try
            {
                var json = await _kernel.InvokeAsync<string>(
                    pluginName: nameof(AssetGenPlugin),
                    functionName: "generate_asset",
                    arguments: new() { 
                        ["prompt"] = prompt ,
                        ["assetMode"] = assetMode ?? string.Empty
                    },
                    cancellationToken: ct
                );
                return json ?? "{\"kind\":\"text\",\"message\":\"AssetGen devolvió vacío.\"}";
            }
            catch (Exception ex)
            {
                return WrapText($"Error en asset_gen: {ex.Message}");
            }
        }

        private static string[] ExtractAssetPathsFromItem(JsonElement item)
        {
            if (item.ValueKind != JsonValueKind.Object) return Array.Empty<string>();
            if (!item.TryGetProperty("kind", out var k) || k.GetString() != "asset") return Array.Empty<string>();
            if (item.TryGetProperty("path", out var p) && p.ValueKind == JsonValueKind.String)
                return new[] { p.GetString()! };
            return Array.Empty<string>();
        }

        private static string? ExtractAssetMode(string routeJson)
        {
            try
            {
                using var rdoc = JsonDocument.Parse(string.IsNullOrWhiteSpace(routeJson) ? "{}" : routeJson);
                if (rdoc.RootElement.TryGetProperty("asset_mode", out var m) && m.ValueKind == JsonValueKind.String)
                {
                    var s = m.GetString();
                    if (string.Equals(s, "sprite", StringComparison.OrdinalIgnoreCase)) return "sprite";
                    if (string.Equals(s, "texture", StringComparison.OrdinalIgnoreCase)) return "texture";
                }
            }
            catch { }
            return null;
        }
    }
}
