using GameProtogenAPI.Services.Contracts;
using OpenAI.Chat;
using System.Xml.Linq;

namespace GameProtogenAPI.Services
{
    public class LLMService : ILLMService
    {
        private readonly ChatClient _mini;
        private readonly ILogger<LLMService> _logger;

        public LLMService(ChatClient mini, ILogger<LLMService> logger)
        {
            _mini = mini;
            _logger = logger;
        }

        public async Task<string> RouteAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            var system = """
                You are a routing agent for a 2D prototyping tool.
                Decide which specialized agents should run for the user's prompt.

                Available agents:
                  - "scene_edit": create/move/remove/edit entities, colors, transforms, level layout, collisions, etc.
                  - "design_qa": conceptual game design (level design, difficulty, pacing, coyote time, input buffering, UX).
                  - "asset_gen": generate an image/texture/sprite/tile (e.g., "imagen", "textura", "sprite", "tile", "png", "jpg", "render", "generate image/texture").

                Output JSON keys:
                  - "agents": array (execution order), unique, 1..3 items, subset of the above.
                  - "reason": short explanation (<= 200 chars).
                Language: same as user.

                Routing rules (IMPORTANT):
                - If the user asks to **generate** an image/texture/sprite/tile **AND** to **apply/use/set** it on existing/new entities,
                  return BOTH agents in this order: ["asset_gen","scene_edit"].
                  Examples: "generá una textura y aplicala al jugador", "quiero un sprite de mago y que se lo pongan al personaje".
                - If the user ONLY asks for generation (no apply/use intent), return ONLY ["asset_gen"].
                - If unsure between scene_edit and design_qa and the prompt is mostly conceptual, prefer "design_qa".
                - If the user asks for scene edits without asset generation, return ONLY ["scene_edit"].
                """;

            var schema = """
            {
              "type": "object",
              "required": ["agents","reason"],
              "properties": {
                "agents": {
                  "type": "array",
                  "minItems": 1,
                  "maxItems": 3,
                  "uniqueItems": true,
                  "items": {
                    "type": "string",
                    "enum": ["scene_edit","design_qa","asset_gen"]
                  }
                },
                "reason": { "type": "string" }
              },
              "additionalProperties": false
            }
            """;

#pragma warning disable OPENAI001
            var options = new ChatCompletionOptions
            {
                ResponseFormat = ChatResponseFormat.CreateJsonSchemaFormat(
                    jsonSchemaFormatName: "ops_schema",
                    jsonSchema: BinaryData.FromString(schema),
                    jsonSchemaIsStrict: false
                ),
                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
            };
#pragma warning restore OPENAI001

            var user = $"Prompt:\n{userPrompt}\n\n(For context, here is the current scene JSON):\n{sceneJson}";
            var completion = await _mini.CompleteChatAsync(
                [new SystemChatMessage(system), new UserChatMessage(user)],
                options, ct);

            var json = completion.Value.Content?[0].Text ?? "{}";
            _logger.LogInformation("ROUTER response: {Json}", json);
            return json;
        }

        public async Task<string> BuildEditPlanXmlAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            var system = """
                Eres un analista de escenas 2D de plataformas para un editor de niveles.
                Interpreta el texto del usuario + la escena y devuelve SOLO un <plan> con
                <agregar/>, <modificar/>, <eliminar/> (pueden estar vacíos).
                Usa atributos simples (id, tipo, pos, tam, color), separa coords con coma.
                Respecto a la posición, siempre usa múltiplos de 32.
                
                A la hora de crear plataformas ( a menos que el usuario diga lo contrario ):
                - Ten en cuenta el "JumpForce" del jugador (entidad tipo "player").
                - Ten en cuenta el "Gravity" de la escena (escena.gravity).
                - Ten en cuenta la posición y tamaño de otras plataformas.
                - Ten en cuenta la posición y tamaño ( ancho y largo ) del jugador (entidad tipo "player"). Crealas un poco más anchas que el jugador.
                - Crea plataformas horizontales, no inclinadas.
                - Ponlas separadas entre sí por un espacio horizontal de al menos 32 pixeles (no pegadas) y en distintas alturas.


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

#pragma warning disable OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.
            var options = new ChatCompletionOptions
            {
                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
            };
#pragma warning restore OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.


            var completionResult = await _mini.CompleteChatAsync(
                new List<ChatMessage>
                {
                    new SystemChatMessage(system),
                    new UserChatMessage(user)
                },
                options
            );

            // 👇 FIX 1: con la sobrecarga async+ct, accedemos a .Value
            var completion = completionResult.Value;
            var text = completion.Content[0].Text?.Trim() ?? "";
            _logger.LogInformation("PLAN response: {Text}", text);

            var plan = ExtractPlanXml(text);
            if (string.IsNullOrWhiteSpace(plan))
                throw new InvalidOperationException("El modelo no devolvió un <plan> XML válido.");

            // Validación básica de XML
            _ = XDocument.Parse(plan);

            return plan;
        }

        public async Task<string> PlanToOpsJsonAsync(string planXml, CancellationToken ct = default)
        {
            // Prompt del "synthesizer" (mini): Plan XML → JSON { "ops": [...] }
            var system = """
                Convierte el PLAN en una lista de operaciones JSON para un motor 2D.
                Responde SOLO un objeto JSON con raíz "ops" (sin texto adicional).

                Operaciones soportadas (usa exactamente estos campos):
                  - spawn_box:
                    {"op":"spawn_box","pos":[x,y],"size":[w,h],"colorHex":"#RRGGBBAA"?}

                  - set_transform:
                    {"op":"set_transform","entity":id,"position":[x,y]?,"scale":[sx,sy]?,"rotation":deg?}

                  - remove_entity:
                    {"op":"remove_entity","entity":id}

                  - set_component (para mutar datos de un componente existente):
                    {"op":"set_component","component":"Sprite","entity":id,"value":{"colorHex":"#RRGGBBAA"|"color":{r,g,b,a}?,"size":[w,h]?}}
                    {"op":"set_component","component":"Texture2D","entity":id,"value":{"path":"Assets/.../file.png"}}

                REGLAS ESTRICTAS DE FORMATO:
                - NUNCA uses "entities". NUNCA agrupes varios IDs en una sola operación.
                - Si el plan menciona múltiples entidades, genera N operaciones separadas (una por entidad).
                - Mantén los nombres de campos EXACTOS.
                - Si hay un bloque [ASSETS] en el prompt, asigna **texturePath** a las plataformas nuevas (puede ser la misma ruta para todas si aplica).
                - Convierte cualquier color por nombre o formato RGBA a "colorHex" #RRGGBBAA.
                - Cuando muevas/ubiques plataformas, si aplicara, usa múltiplos de 32.
                - No incluyas comentarios, ni texto fuera del JSON.
                """;

            var user = $"""
                <plan>
                {planXml}
                </plan>
                """;


            // JSON Schema que incluye set_component (acepta entity o entities)
            var schema = """
            {
                "type": "object",
                "additionalProperties": false,
                "properties": {
                "ops": {
                    "type": "array",
                    "minItems": 0,
                    "items": {
                    "oneOf": [
                        { "$ref": "#/$defs/spawn_box" },
                        { "$ref": "#/$defs/set_transform" },
                        { "$ref": "#/$defs/remove_entity" },
                        { "$ref": "#/$defs/set_component" }
                    ]
                    }
                }
                },
                "required": ["ops"],
                "$defs": {
                "vec2": {
                    "type": "array",
                    "items": { "type": "number" },
                    "minItems": 2,
                    "maxItems": 2
                },
                "colorObj": {
                    "type": "object",
                    "additionalProperties": false,
                    "properties": {
                    "r": { "type": "integer", "minimum": 0, "maximum": 255 },
                    "g": { "type": "integer", "minimum": 0, "maximum": 255 },
                    "b": { "type": "integer", "minimum": 0, "maximum": 255 },
                    "a": { "type": "integer", "minimum": 0, "maximum": 255 }
                    },
                    "required": ["r","g","b"]
                },
                "spawn_box": {
                    "type": "object",
                    "additionalProperties": false,
                    "properties": {
                    "op": { "type": "string", "enum": ["spawn_box"] },
                    "pos": { "$ref": "#/$defs/vec2" },
                    "size": { "$ref": "#/$defs/vec2" },
                    "colorHex": { "type": "string", "pattern": "^#([A-Fa-f0-9]{8}|[A-Fa-f0-9]{6})$" }
                    },
                    "required": ["op","pos","size"]
                },
                "set_transform": {
                    "type": "object",
                    "additionalProperties": false,
                    "properties": {
                    "op": { "type": "string", "enum": ["set_transform"] },
                    "entity": { "type": "integer", "minimum": 1 },
                    "position": { "$ref": "#/$defs/vec2" },
                    "scale": { "$ref": "#/$defs/vec2" },
                    "rotation": { "type": "number" }
                    },
                    "required": ["op","entity"]
                },
                "remove_entity": {
                    "type": "object",
                    "additionalProperties": false,
                    "properties": {
                    "op": { "type": "string", "enum": ["remove_entity"] },
                    "entity": { "type": "integer", "minimum": 1 }
                    },
                    "required": ["op","entity"]
                },
                "set_component": {
                  "type": "object",
                  "additionalProperties": false,
                  "properties": {
                    "op": { "type": "string", "enum": ["set_component"] },
                    "component": { "type": "string", "enum": ["Sprite","Texture2D"] }, // <- agregado Texture2D
                    "entity": { "type": "integer", "minimum": 1 },
                    "value": {
                      "type": "object",
                      "additionalProperties": false,
                      "properties": {
                        "colorHex": { "type": "string", "pattern": "^#([A-Fa-f0-9]{8}|[A-Fa-f0-9]{6})$" },
                        "color": { "$ref": "#/$defs/colorObj" },
                        "size": { "$ref": "#/$defs/vec2" },
                        "path": { "type": "string" } // <- para Texture2D
                      }
                    }
                  },
                  "required": ["op","component","entity","value"]
                }
                }
            }
            """;

#pragma warning disable OPENAI001
            var options = new ChatCompletionOptions
            {
                ResponseFormat = ChatResponseFormat.CreateJsonSchemaFormat(
                    jsonSchemaFormatName: "ops_schema",
                    jsonSchema: BinaryData.FromString(schema),
                    jsonSchemaIsStrict: false // dejar laxa: el modelo puede incluir entity o entities
                ),
                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
            };
#pragma warning restore OPENAI001

            var completionResult = await _mini.CompleteChatAsync(
                new List<ChatMessage>
                {
            new SystemChatMessage(system),
            new UserChatMessage(user)
                },
                options,
                ct
            );

            var completion = completionResult.Value;
            var json = completion.Content[0].Text?.Trim() ?? "";
            _logger.LogInformation("OPS response (raw): {Json}", json);

            // --- Saneo robusto ---
            json = StripCodeFences(json);
            json = TrimToOuterJsonObject(json);
            json = RemoveJsonComments(json);

            // Validación temprana: ¿parsea y tiene "ops"?
            try
            {
                using var doc = System.Text.Json.JsonDocument.Parse(json);
                if (!doc.RootElement.TryGetProperty("ops", out _))
                    throw new InvalidOperationException("El MINI no devolvió JSON con 'ops'.");
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "OPS JSON inválido tras saneo");
                throw; // será capturado arriba y envuelto como "Error en scene_edit: ..."
            }

            _logger.LogInformation("OPS response (clean): {Json}", json);
            return json;
        }

        public async Task<string> AskDesignAsync(string userQuestion, CancellationToken ct = default)
        {
            var system = """
                You are a senior Game Design Advisor specialized in 2D platformers and rapid prototyping.
                Goals:
                  - Give concise, actionable answers (≤ 12 lines).
                  - Use concrete numbers (pixels, seconds), simple formulas, ranges and trade-offs.
                  - Reference common patterns: jump arcs, coyote time, input buffering, acceleration curves, invulnerability frames.
                  - Avoid engine-specific APIs; speak conceptually with practical tips that transfer across engines.
                Output:
                  - Plain text only, no code unless explicitly asked.
                Language: match the user's language.
            """;

#pragma warning disable OPENAI001
            var options = new ChatCompletionOptions
            {
                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
            };
#pragma warning restore OPENAI001

            var completionResult = await _mini.CompleteChatAsync(
                new List<ChatMessage>
                {
            new SystemChatMessage(system),
            new UserChatMessage(userQuestion)
                },
                options,
                ct
            );

            var text = completionResult.Value.Content?[0].Text?.Trim();
            _logger.LogInformation("DESIGN Q&A response: {Text}", text);
            return string.IsNullOrWhiteSpace(text)
                ? "No tengo una respuesta útil en este momento."
                : text!;
        }

        // --- Helpers ---
        private static string ExtractPlanXml(string raw)
        {
            if (string.IsNullOrWhiteSpace(raw)) return string.Empty;
            var start = raw.IndexOf("<plan", StringComparison.OrdinalIgnoreCase);
            if (start < 0) return string.Empty;

            var end = raw.LastIndexOf("</plan>", StringComparison.OrdinalIgnoreCase);
            if (end < 0) return string.Empty;

            // Antes: return raw.Substring(start, end - start);
            // Ahora: incluir el largo de "</plan>" (7 chars)
            const int closingLen = 7; // "</plan>"
            return raw.Substring(start, (end - start) + closingLen);
        }

        private static string StripCodeFences(string s)
        {
            if (string.IsNullOrWhiteSpace(s)) return s ?? "";
            s = s.Trim();
            // ```json ... ```  o  ``` ...
            if (s.StartsWith("```"))
            {
                // quitar la primera línea de fence
                var idx = s.IndexOf('\n');
                if (idx >= 0) s = s[(idx + 1)..];
                // quitar fence de cierre
                var lastFence = s.LastIndexOf("```", StringComparison.Ordinal);
                if (lastFence >= 0) s = s[..lastFence];
            }
            return s.Trim();
        }

        private static string TrimToOuterJsonObject(string s)
        {
            if (string.IsNullOrEmpty(s)) return "{}";
            int first = s.IndexOf('{');
            int last = s.LastIndexOf('}');
            if (first >= 0 && last >= first) return s.Substring(first, last - first + 1);
            return s;
        }

        private static string RemoveJsonComments(string s)
        {
            if (string.IsNullOrEmpty(s)) return s;
            // Quitar // ... (hasta fin de línea)
            s = System.Text.RegularExpressions.Regex.Replace(
                    s, @"(?m)//.*?$", string.Empty);
            // Quitar /* ... */ (multilínea)
            s = System.Text.RegularExpressions.Regex.Replace(
                    s, @"/\*.*?\*/", string.Empty, System.Text.RegularExpressions.RegexOptions.Singleline);
            return s.Trim();
        }
    }
}
