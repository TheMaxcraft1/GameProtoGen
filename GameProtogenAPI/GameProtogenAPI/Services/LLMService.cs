using GameProtogenAPI.Services.Contracts;
using OpenAI.Chat;
using System.Xml.Linq;

namespace GameProtogenAPI.Services
{
    public class LLMService : ILLMService
    {
        private readonly ChatClient _nano; // planner
        private readonly ChatClient _mini; // synthesizer

        public LLMService(ChatClient nano, ChatClient mini)
        {
            // Nota: el orden de registro en Program.cs hace que el primer ChatClient
            // inyectado sea el del nanoModel y el segundo el del miniModel.
            _nano = nano;
            _mini = mini;
        }

        public async Task<string> BuildEditPlanXmlAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            // Prompt del "planner" (nano) en XML → devuelve <plan>...</plan>
            var system = """
                Eres un analista de escenas 2D para un editor de niveles.
                Interpreta el texto del usuario + la escena y devuelve SOLO un <plan> con
                <agregar/>, <modificar/>, <eliminar/> (pueden estar vacíos).
                Usa atributos simples (id, tipo, pos, tam, color), separa coords con coma.
                No devuelvas nada fuera de <plan>...</plan>.
                """;

            var user = $"""
                <task>
                  <input>
                    <prompt_usuario><![CDATA[
                    {userPrompt}
                    ]]></prompt_usuario>
                    <escena_json><![CDATA[
                    {sceneJson}
                    ]]></escena_json>
                  </input>
                  <output_format>
                    <plan>
                      <agregar/>
                      <modificar/>
                      <eliminar/>
                    </plan>
                  </output_format>
                  <rules>
                    - Mantén el formato exacto.
                    - Usa "," como separador de coordenadas/tamaños.
                  </rules>
                </task>
                """;

            var completionResult = await _nano.CompleteChatAsync(
                new List<ChatMessage>
                {
                    new SystemChatMessage(system),
                    new UserChatMessage(user)
                },
                new ChatCompletionOptions { Temperature = 0.2f },
                ct
            );

            // 👇 FIX 1: con la sobrecarga async+ct, accedemos a .Value
            var completion = completionResult.Value;
            var text = completion.Content[0].Text?.Trim() ?? "";

            var plan = ExtractPlanXml(text);
            if (string.IsNullOrWhiteSpace(plan))
                throw new InvalidOperationException("El NANO no devolvió un <plan> XML válido.");

            // Validación básica de XML
            _ = XDocument.Parse(plan);

            return plan;
        }

        public async Task<string> PlanToOpsJsonAsync(string planXml, CancellationToken ct = default)
        {
            // Prompt del "synthesizer" (mini): Plan XML → JSON { "ops": [...] }
            var system = """
                Convierte el PLAN en una lista de operaciones JSON para un motor 2D.
                SOLO responde un objeto JSON con raíz "ops".
                Operaciones soportadas:
                  - spawn_box: {"op":"spawn_box","pos":[x,y],"size":[w,h],"colorHex":"#RRGGBBAA"}
                  - set_transform: {"op":"set_transform","entity":id, "position":[x,y]?, "scale":[sx,sy]?, "rotation":deg?}
                  - remove_entity: {"op":"remove_entity","entity":id}
                No incluyas texto adicional.
                """;

            var user = $"""
                <plan>
                {planXml}
                </plan>
                """;

            // 👇 FIX 2: Structured Outputs con JSON Schema
            var schema = """
            {
              "type": "object",
              "properties": {
                "ops": {
                  "type": "array",
                  "items": {
                    "oneOf": [
                      { "$ref": "#/$defs/spawn_box" },
                      { "$ref": "#/$defs/set_transform" },
                      { "$ref": "#/$defs/remove_entity" }
                    ]
                  }
                }
              },
              "required": ["ops"],
              "additionalProperties": false,
              "$defs": {
                "vec2": {
                  "type": "array",
                  "items": { "type": "number" },
                  "minItems": 2,
                  "maxItems": 2
                },
                "spawn_box": {
                  "type": "object",
                  "properties": {
                    "op": { "const": "spawn_box" },
                    "pos": { "$ref": "#/$defs/vec2" },
                    "size": { "$ref": "#/$defs/vec2" },
                    "colorHex": {
                      "type": "string",
                      "pattern": "^#([A-Fa-f0-9]{8}|[A-Fa-f0-9]{6})$"
                    }
                  },
                  "required": ["op","pos","size"],
                  "additionalProperties": false
                },
                "set_transform": {
                  "type": "object",
                  "properties": {
                    "op": { "const": "set_transform" },
                    "entity": { "type": "integer", "minimum": 1 },
                    "position": { "$ref": "#/$defs/vec2" },
                    "scale": { "$ref": "#/$defs/vec2" },
                    "rotation": { "type": "number" }
                  },
                  "required": ["op","entity"],
                  "additionalProperties": false
                },
                "remove_entity": {
                  "type": "object",
                  "properties": {
                    "op": { "const": "remove_entity" },
                    "entity": { "type": "integer", "minimum": 1 }
                  },
                  "required": ["op","entity"],
                  "additionalProperties": false
                }
              }
            }
            """;

#pragma warning disable OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.
            var options = new ChatCompletionOptions
            {
                Temperature = 0f,
                ResponseFormat = ChatResponseFormat.CreateJsonSchemaFormat(
                    jsonSchemaFormatName: "ops_schema",
                    jsonSchema: BinaryData.FromString(schema),
                    jsonSchemaIsStrict: true
                ),
                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
            };
#pragma warning restore OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.

            var completionResult = await _mini.CompleteChatAsync(
                new List<ChatMessage>
                {
                    new SystemChatMessage(system),
                    new UserChatMessage(user)
                },
                options,
                ct
            );

            // 👇 FIX 1 (igual que arriba)
            var completion = completionResult.Value;
            var json = completion.Content[0].Text?.Trim() ?? "";

            if (string.IsNullOrWhiteSpace(json) || !json.Contains("\"ops\""))
                throw new InvalidOperationException("El MINI no devolvió JSON con 'ops'.");

            return json;
        }

        // --- Helpers ---
        private static string ExtractPlanXml(string raw)
        {
            var start = raw.IndexOf("<plan", StringComparison.OrdinalIgnoreCase);
            if (start < 0) return "";
            var end = raw.LastIndexOf("</plan>", StringComparison.OrdinalIgnoreCase);
            if (end < 0) return "";
            end += "</plan>".Length;
            return raw.Substring(start, end - start);
        }
    }
}
