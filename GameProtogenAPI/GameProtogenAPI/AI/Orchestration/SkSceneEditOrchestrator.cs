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

        public SkSceneEditOrchestrator(Kernel kernel)
        {
            _kernel = kernel;
        }

        public async Task<string> RunAsync(string prompt, string sceneJson, CancellationToken ct)
        {
            // 1) Planner → <plan> XML
            var plan = await _kernel.InvokeAsync<string>(
                pluginName: nameof(PlannerPlugin),
                functionName: "build_plan_xml",
                arguments: new()
                {
                    ["prompt"] = prompt,
                    ["sceneJson"] = sceneJson
                },
                cancellationToken: ct
            );

            if (string.IsNullOrWhiteSpace(plan))
                return """{"kind":"answer","answer":"No se pudo generar el plan."}""";

            // 2) Synthesizer → {"ops":[...]}
            var opsJson = await _kernel.InvokeAsync<string>(
                pluginName: nameof(SynthesizerPlugin),
                functionName: "plan_to_ops",
                arguments: new() { ["planXml"] = plan },
                cancellationToken: ct
            );

            // 3) Normalizamos salida al contrato {kind:"ops",...}
            //    (Validación queda en el Controller, como siempre)
            using var doc = JsonDocument.Parse(opsJson);
            if (!doc.RootElement.TryGetProperty("ops", out _))
                return """{"kind":"answer","answer":"El synthesizer no devolvió 'ops'."}""";

            // Re-serializamos agregando el campo de forma segura
            var root = doc.RootElement;

            using var ms = new MemoryStream();
            using (var writer = new Utf8JsonWriter(ms))
            {
                writer.WriteStartObject();
                writer.WriteString("kind", "ops");

                // Copiar todas las propiedades existentes
                foreach (var prop in root.EnumerateObject())
                    prop.WriteTo(writer);

                writer.WriteEndObject();
            }
            return Encoding.UTF8.GetString(ms.ToArray());
        }
    }
}
