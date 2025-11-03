using GameProtogenAPI.Services.Contracts;
using Microsoft.AspNetCore.DataProtection.KeyManagement;
using Microsoft.SemanticKernel;
using Microsoft.SemanticKernel.ChatCompletion;
using OpenAI.Chat;
using OpenAI.Images;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace GameProtogenAPI.Services
{
    public class LLMService : ILLMService
    {
        private readonly Kernel _kernel;
        private readonly ILogger<LLMService> _logger;
        private readonly IImageService _images;

        public LLMService(ILogger<LLMService> logger, IImageService images, IConfiguration cfg)
        { 
            _logger = logger;
            _images = images;

            var azureAIInferenceEndpoint =
                cfg["AzureAI:InferenceEndpoint"] ??
                cfg["AZURE_INFERENCE_ENDPOINT"] ??
                Environment.GetEnvironmentVariable("AZURE_INFERENCE_ENDPOINT");

            var azureAIInferenceApiKey =
                cfg["AzureAI:InferenceApiKey"] ??
                cfg["AZURE_INFERENCE_API_KEY"] ??
                Environment.GetEnvironmentVariable("AZURE_INFERENCE_API_KEY");

            if (string.IsNullOrWhiteSpace(azureAIInferenceEndpoint))
                throw new InvalidOperationException("Falta AzureAI Endpoint (AzureAI:Endpoint o AZURE_OPENAI_ENDPOINT).");
            if (string.IsNullOrWhiteSpace(azureAIInferenceApiKey))
                throw new InvalidOperationException("Falta AzureAI ApiKey (AzureAI:ApiKey o AZURE_OPENAI_API_KEY).");

            _kernel = Kernel.CreateBuilder()
                //.AddAzureAIInferenceChatCompletion("grok-4-fast-reasoning", azureAIInferenceApiKey, new Uri(azureAIInferenceEndpoint))
                //.AddAzureAIInferenceChatCompletion("phi-4-mini-reasoning", azureAIInferenceApiKey, new Uri(azureAIInferenceEndpoint))
                .AddAzureAIInferenceChatCompletion("gpt-5-mini", azureAIInferenceApiKey, new Uri(azureAIInferenceEndpoint))
                .Build();
        }

        private async Task<string> CompleteAsync(string system, string user, CancellationToken ct, double temperature = 0.2)
        {
            try
            {
                var chat = _kernel.GetRequiredService<IChatCompletionService>();

                var history = new ChatHistory();
                history.AddSystemMessage(system);
                history.AddUserMessage(user);

                var result = await chat.GetChatMessageContentAsync(history);
                var content  = result?.Content ?? string.Empty;
                content = Regex.Replace(
                    content,
                    @"<think\b[^>]*>.*?</think>",
                    string.Empty,
                    RegexOptions.IgnoreCase | RegexOptions.Singleline
                ).Trim();

                return content;
            }
            catch(Exception ex)
            {
                throw;
            }
        }

        public async Task<string> RouteAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            var system = """"
                You are a routing agent for a 2D prototyping tool.
                Decide which specialized agents should run for the user's prompt.

                Available agents:
                  - "scene_edit": create/move/remove/edit entities, colors, transforms, level layout, collisions, etc.
                  - "design_qa": conceptual game design (level design, difficulty, pacing, coyote time, input buffering, UX).
                  - "asset_gen": generate an image/texture/sprite/tile (e.g., "imagen", "textura", "sprite", "tile", "png", "jpg", "render", "generate image/texture").
                  - "script_gen": generate Lua scripts for the built-in VM (ecs.*, on_spawn/on_update), behaviors/AI/movement.

                Output JSON keys:
                  - "agents": array (execution order), unique, 1..3 items, subset of the above.
                  - "reason": short explanation (<= 200 chars).
                  - "asset_mode": OPTIONAL string. REQUIRED if "asset_gen" is present.
                enum: ["texture","sprite"]
                Language: same as user.

                Routing rules (IMPORTANT):
                - Return ONLY a JSON object wrapped between the exact markers (DO NOT INCLUDE ANY OTHER COMMENT OR THING LIKE QUOTES EXPRESSING IT'S A JSON):
                    { ... }
                - JSON shape:
                    {
                      "agents": ["scene_edit"|"design_qa"|"asset_gen"|"script_gen", ...], // 1..3 unique, execution order
                      "reason": "short string (<=200 chars)",
                      "asset_mode": "texture"|"sprite"   // OPTIONAL, REQUIRED if "asset_gen" present
                    }
                - If the user asks to generate an image/texture/sprite/tile:
                    * include "asset_gen"
                    * set "asset_mode" to "texture" when they want a MATERIAL / TILE / BACKGROUND that fills the canvas.
                    * set "asset_mode" to "sprite"   when they want a cutout subject/object with transparent background.
                - If the user asks to **generate** an image/texture/sprite/tile **AND** to **apply/use/set** it on existing/new entities,
                  return BOTH agents in this order: ["asset_gen","scene_edit"].
                  Examples: "generá una textura y aplicala al jugador", "quiero un sprite de mago y que se lo pongan al personaje".
                - If the user asks to generate Lua code / script / comportamiento / IA / movimiento automático:
                    * include "script_gen".
                    * If they also ask to APPLY/attach that script to entities (existing or new), return BOTH:
                      ["script_gen","scene_edit"] (in that order).
                - If only code generation (no apply/use intent), return ONLY ["script_gen"].
                - If the user ONLY asks for generation (no apply/use intent), return ONLY ["asset_gen"].
                - If unsure between scene_edit and design_qa and the prompt is mostly conceptual, prefer "design_qa".
                - If the user asks for scene edits without asset generation, return ONLY ["scene_edit"].
                """";

            //var schema = """
            //{
            //  "type": "object",
            //  "required": ["agents","reason"],
            //  "properties": {
            //    "agents": {
            //      "type": "array",
            //      "minItems": 1,
            //      "maxItems": 3,
            //      "uniqueItems": true,
            //      "items": {
            //        "type": "string",
            //        "enum": ["scene_edit","design_qa","asset_gen", "script_gen"]
            //      }
            //    },
            //    "reason": { "type": "string" },
            //    "asset_mode": {
            //      "type": "string",
            //      "enum": ["texture","sprite"]
            //    }
            //  },
            //  "additionalProperties": false
            //}
            //""";

//#pragma warning disable OPENAI001
//            var options = new ChatCompletionOptions
//            {
//                ResponseFormat = ChatResponseFormat.CreateJsonSchemaFormat(
//                    jsonSchemaFormatName: "ops_schema",
//                    jsonSchema: BinaryData.FromString(schema),
//                    jsonSchemaIsStrict: false
//                ),
//                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
//            };
//#pragma warning restore OPENAI001

            var user = $"Prompt:\n{userPrompt}\n\n(For context, here is the current scene JSON):\n{sceneJson}";
            var completion = await CompleteAsync(system, user, ct);
            //var json = completion.Value.Content?[0].Text ?? "{}";
            _logger.LogInformation("ROUTER response: {Json}", completion);
            return completion;
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
                
                REGLAS EXTRA (TEXTURAS):
                - Si el prompt incluye un bloque [ASSETS] con líneas tipo "ASSETk:Assets/.../file.png"
                  y el usuario pidió aplicarlas (ej. "aplicala al player", "a las 5 plataformas azules"),
                  entonces:
                  * Para entidades nuevas en <agregar>, agrega atributo texturePath="Assets/.../file.png" (elige el ASSET correcto).
                  * Para entidades existentes, crea items en <modificar> con:
                      <item id="X" propiedad="texturePath" valor="Assets/.../file.png"/>
                  * En particular, si pide generar un sprite para el jugador, solo debería ser modificar a la entidad del jugador.
                - Usa ids reales si los menciona el usuario. Si no se mencionan, identifica por tipo ("player", "platform") o por color/tamaño cuando sea obvio.
                - Mantén coords en múltiplos de 32. No devuelvas nada fuera de <plan>...

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

//#pragma warning disable OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.
//            var options = new ChatCompletionOptions
//            {
//                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
//            };
//#pragma warning restore OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.



            // 👇 FIX 1: con la sobrecarga async+ct, accedemos a .Value
            var completion = await CompleteAsync(system, user, ct);
            //var text = completion.Content[0].Text?.Trim() ?? "";
            _logger.LogInformation("PLAN response: {Text}", completion);

            var plan = ExtractPlanXml(completion);
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
                Responde SOLO un objeto JSON con raíz "ops" (sin texto adicional) DO NOT INCLUDE ANY OTHER COMMENT OR THING LIKE QUOTES EXPRESSING IT'S A JSON.
                    { "ops": [ ... ] }

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
                
                REGLAS PARA TEXTURAS:
                - Si un ítem en <modificar> tiene propiedad="texturePath", emite:
                  {"op":"set_component","component":"Texture2D","entity":ID,"value":{"path":"<valor>"}}
                - Si una entidad en <agregar> trae atributo texturePath, además del "spawn_box" correspondiente,
                  emite otro:
                  {"op":"set_component","component":"Texture2D","entity":<id_asignado_si_aplica_o_si_el_plan_lo_da>,"value":{"path":"<texturePath>"}}
                  (Si no hay ID asignado en el plan para la nueva entidad, omite este paso.)
                - Si el usuario pidió “aplicar al player” y el plan lo refleja en <modificar> con texturePath, aplica solo a ese ID.
                - Mantén EXACTOS los nombres de campos.

                REGLAS ESTRICTAS DE FORMATO:
                - NUNCA uses "entities". NUNCA agrupes varios IDs en una sola operación.
                - Si el plan menciona múltiples entidades, genera N operaciones separadas (una por entidad).
                - Mantén los nombres de campos EXACTOS.
                - Si hay un bloque [ASSETS] en el prompt, asigna **value.path** a las plataformas nuevas (puede ser la misma ruta para todas si aplica).
                - Convierte cualquier color por nombre o formato RGBA a "colorHex" #RRGGBBAA.
                - Cuando muevas/ubiques plataformas, si aplicara, usa múltiplos de 32.
                - No incluyas comentarios, ni texto fuera del JSON.
                - Si el color no especifica alfa, usa AA=FF (opaco).
                - NUNCA emitas AA=00 a menos que el usuario pida explícitamente transparencia/invisibilidad.
                """;

            var user = $"""
                <plan>
                {planXml}
                </plan>
                """;


            // JSON Schema que incluye set_component (acepta entity o entities)
//            var schema = """
//            {
//              "type": "object",
//              "additionalProperties": false,
//              "properties": {
//                "ops": {
//                  "type": "array",
//                  "minItems": 0,
//                  "items": {
//                    "oneOf": [
//                      { "$ref": "#/$defs/spawn_box" },
//                      { "$ref": "#/$defs/set_transform" },
//                      { "$ref": "#/$defs/remove_entity" },
//                      { "$ref": "#/$defs/set_component" }
//                    ]
//                  }
//                }
//              },
//              "required": ["ops"],
//              "$defs": {
//                "vec2": {
//                  "type": "array",
//                  "items": { "type": "number" },
//                  "minItems": 2,
//                  "maxItems": 2
//                },
//                "colorObj": {
//                  "type": "object",
//                  "additionalProperties": false,
//                  "properties": {
//                    "r": { "type": "integer", "minimum": 0, "maximum": 255 },
//                    "g": { "type": "integer", "minimum": 0, "maximum": 255 },
//                    "b": { "type": "integer", "minimum": 0, "maximum": 255 },
//                    "a": { "type": "integer", "minimum": 0, "maximum": 255 }
//                  },
//                  "required": ["r","g","b"]
//                },
//                "spawn_box": {
//                  "type": "object",
//                  "additionalProperties": false,
//                  "properties": {
//                    "op": { "type": "string", "enum": ["spawn_box"] },
//                    "pos": { "$ref": "#/$defs/vec2" },
//                    "size": { "$ref": "#/$defs/vec2" },
//                    "colorHex": { "type": "string", "pattern": "^#([A-Fa-f0-9]{8}|[A-Fa-f0-9]{6})$" }
//                  },
//                  "required": ["op","pos","size"]
//                },
//                "set_transform": {
//                  "type": "object",
//                  "additionalProperties": false,
//                  "properties": {
//                    "op": { "type": "string", "enum": ["set_transform"] },
//                    "entity": { "type": "integer", "minimum": 1 },
//                    "position": { "$ref": "#/$defs/vec2" },
//                    "scale": { "$ref": "#/$defs/vec2" },
//                    "rotation": { "type": "number" }
//                  },
//                  "required": ["op","entity"]
//                },
//                "remove_entity": {
//                  "type": "object",
//                  "additionalProperties": false,
//                  "properties": {
//                    "op": { "type": "string", "enum": ["remove_entity"] },
//                    "entity": { "type": "integer", "minimum": 1 }
//                  },
//                  "required": ["op","entity"]
//                },
//                "set_component": {
//                  "type": "object",
//                  "additionalProperties": false,
//                  "properties": {
//                    "op": { "type": "string", "enum": ["set_component"] },
//                    "component": { "type": "string", "enum": ["Sprite","Texture2D"] },
//                    "entity": { "type": "integer", "minimum": 1 },
//                    "value": {
//                      "type": "object",
//                      "additionalProperties": false,
//                      "properties": {
//                        "colorHex": { "type": "string", "pattern": "^#([A-Fa-f0-9]{8}|[A-Fa-f0-9]{6})$" },
//                        "color": { "$ref": "#/$defs/colorObj" },
//                        "size": { "$ref": "#/$defs/vec2" },
//                        "path": { "type": "string" }
//                      }
//                    }
//                  },
//                  "required": ["op","component","entity","value"]
//                }
//              }
//            }
//            """;

//#pragma warning disable OPENAI001
//            var options = new ChatCompletionOptions
//            {
//                ResponseFormat = ChatResponseFormat.CreateJsonSchemaFormat(
//                    jsonSchemaFormatName: "ops_schema",
//                    jsonSchema: BinaryData.FromString(schema),
//                    jsonSchemaIsStrict: false // dejar laxa: el modelo puede incluir entity o entities
//                ),
//                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
//            };
//#pragma warning restore OPENAI001

            var json = await CompleteAsync(system, user, ct);

            //var completion = completionResult.Value;
            //var json = completion.Content[0].Text?.Trim() ?? "";
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
                  - Plain text only.
                Language: match the user's language.
            """;

//#pragma warning disable OPENAI001
//            var options = new ChatCompletionOptions
//            {
//                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
//            };
//#pragma warning restore OPENAI001

            var text = await CompleteAsync(system, userQuestion, ct);

            //var text = completionResult.Value.Content?[0].Text?.Trim();
            _logger.LogInformation("DESIGN Q&A response: {Text}", text);
            return string.IsNullOrWhiteSpace(text)
                ? "No tengo una respuesta útil en este momento."
                : text!;
        }

        public async Task<string> GenerateAssetAsync(string prompt, string? assetMode = null, CancellationToken ct = default)
        {
            if (string.IsNullOrWhiteSpace(prompt))
                return System.Text.Json.JsonSerializer.Serialize(new { kind = "text", message = "Descripción vacía." });

            // 1) Reglas cortas según modo (solo prefijo del prompt del usuario)
            string rules = string.Empty;
            if (string.Equals(assetMode, "texture", StringComparison.OrdinalIgnoreCase))
            {
                rules = """
                    MODO TEXTURA:
                    - Genera una TEXTURA 2D que LLENE COMPLETAMENTE el lienzo (sin zonas vacías).
                    - SIN transparencia (alpha 255 en todos los píxeles).
                    - Iluminación uniforme, sin viñeteo, sin texto ni logos.
                    - NEGATIVOS: sin personas, sin personajes, sin objetos, sin escenas o fondos ilustrados. Si en el pedido de usuario ves "plataformas" no generes plataformas dentro de la textura, solo la textura en si.
                    - Si aplica, que sea tileable/seamless.
                    """;
                        }
            else if (string.Equals(assetMode, "sprite", StringComparison.OrdinalIgnoreCase))
            {
                rules = """
                    MODO SPRITE:
                    - Sprite con FONDO TRANSPARENTE (PNG RGBA).
                    - Un único sujeto/objeto, centrado, recorte ajustado (pocos píxeles de margen).
                    - NEGATIVOS: sin fondo/escena completa, sin suelo, sin texto ni logos.
                    """;

            }

            var modelPrompt = string.IsNullOrEmpty(rules)
                ? prompt
                : $"{rules}\n\nPEDIDO DEL USUARIO:\n{prompt}";

            GeneratedImage img;
            try
            {
                var gen = await _images.GenerateImage(modelPrompt, 
                    transparent: string.Equals(assetMode, "sprite", StringComparison.OrdinalIgnoreCase), ct);
                img = gen.Value;
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Fallo generando imagen");
                return System.Text.Json.JsonSerializer.Serialize(new { kind = "text", message = $"No pude generar la imagen: {ex.Message}" });
            }

            byte[] bytes = await GetBytesAsync(img, ct);
            if (bytes.Length == 0)
                return System.Text.Json.JsonSerializer.Serialize(new { kind = "text", message = "El modelo no devolvió datos de imagen." });

            var fileName = MakeSafeFileName($"{DateTime.UtcNow:yyyyMMdd_HHmmssfff}.png");
            var path = $"Assets/Generated/{fileName}"; // mismo contrato que venía usando el cliente
            var b64 = Convert.ToBase64String(bytes);

            return System.Text.Json.JsonSerializer.Serialize(new
            {
                kind = "asset",
                fileName,
                path,
                data = b64
            });
        }

        public async Task<string> GenerateLuaAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            var system = """
                            You are a Lua code generator for a small 2D engine.
                            Output ONLY JSON (DO NOT INCLUDE ANY OTHER COMMENT OR THING LIKE QUOTES EXPRESSING IT'S A JSON):
                                { "kind":"script", "fileName":"<suggested>.lua", "code":"<lua source>" }

                            Target VM (Lua 5.4, sol2). Scripts run in an environment with:
                              - Global: this_id (uint)
                              - Optional callbacks:
                                  function on_spawn() end
                                  function on_update(dt) end   -- dt in seconds (float)

                            Engine API (exposed as global table `ecs`):
                              -- Entity ops
                              ecs.create() -> uint
                              ecs.destroy(id:uint)
                              ecs.first_with(comp:string) -> uint    -- "Transform","Sprite","Collider","Physics2D","PlayerController"
                              -- GET always returns named tables (NOT arrays):
                              ecs.get(id, "Transform") => { position={ x:number, y:number }, scale={ x:number, y:number }, rotation:number }
                              ecs.get(id, "Sprite")    => { size={ x:number, y:number }, color={ r:int, g:int, b:int, a:int } }
                              ecs.get(id, "Collider")  => { halfExtents={ x:number, y:number }, offset={ x:number, y:number } }
                              ecs.get(id, "Physics2D") => { velocity={ x:number, y:number }, gravity:number, gravityEnabled:boolean, onGround:boolean }
                              ecs.get(id, "PlayerController") => { moveSpeed:number, jumpSpeed:number }

                              -- SET also expects named tables (NOT arrays):
                              ecs.set(id, "Transform", { position={ x:number, y:number }?, scale={ x:number, y:number }?, rotation:number? })
                              ecs.set(id, "Sprite",    { size={ x:number, y:number }?, color={ r:int, g:int, b:int, a:int }? })
                              ecs.set(id, "Collider",  { halfExtents={ x:number, y:number }?, offset={ x:number, y:number }? })
                              ecs.set(id, "Physics2D", { velocity={ x:number, y:number }?, gravity:number?, gravityEnabled:boolean?, onGround:boolean? })
                              ecs.set(id, "PlayerController", { moveSpeed:number?, jumpSpeed:number? })

                            Rules:
                              - When reading positions/sizes, ALWAYS use named fields: p.x, p.y (never p[1], p[2]).
                              - When writing positions/sizes, ALWAYS send named fields: { x=..., y=... }.
                              - Always define at least one of: on_spawn, on_update.
                              - No require/io/os/debug/loadstring.
                              - Use multiples of 32 for platform placement.
                              - Prefer ecs.get/ecs.set.
                              - If you need the player: local pid = ecs.first_with("PlayerController")
                              - Comments can match user language; code is Lua.

                            Deliver JSON only (no fences, no extra text).
                        """;

//            var schema = """
//                        {
//                          "type": "object",
//                          "required": ["kind","fileName","code"],
//                          "additionalProperties": false,
//                          "properties": {
//                            "kind": { "type": "string", "const": "script" },
//                            "fileName": { "type": "string" },
//                            "code": { "type": "string" }
//                          }
//                        }
//                        """;

//#pragma warning disable OPENAI001
//            var options = new ChatCompletionOptions
//            {
//                ResponseFormat = ChatResponseFormat.CreateJsonSchemaFormat(
//                    jsonSchemaFormatName: "lua_code",
//                    jsonSchema: BinaryData.FromString(schema),
//                    jsonSchemaIsStrict: false
//                ),
//                ReasoningEffortLevel = ChatReasoningEffortLevel.Low
//            };
//#pragma warning restore OPENAI001

            var user = $"""
                User request:
                {userPrompt}

                Context (scene JSON):
                {sceneJson}
                """;

            var json = await CompleteAsync(system, user, ct);

            //var json = completion.Value.Content?[0].Text ?? "";
            json = StripCodeFences(json);
            json = TrimToOuterJsonObject(json);
            _logger.LogInformation("LUA response: {Json}", json);
            return json;
        }


        #region HELPERS
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
            // quitar // ... al final de línea (modo multilínea)
            s = System.Text.RegularExpressions.Regex.Replace(
                    s, @"(?m)//.*?$", string.Empty);
            // quitar /* ... */ en bloque (modo singleline)
            s = System.Text.RegularExpressions.Regex.Replace(
                    s, @"/\*.*?\*/", string.Empty, System.Text.RegularExpressions.RegexOptions.Singleline);
            return s.Trim();
        }

        private static async Task<byte[]> GetBytesAsync(GeneratedImage img, CancellationToken ct)
        {
            var bytesProp = typeof(GeneratedImage).GetProperty("ImageBytes");
            if (bytesProp?.GetValue(img) is BinaryData bd)
                return bd.ToArray();

            var b64Prop = typeof(GeneratedImage).GetProperty("Base64Data");
            if (b64Prop?.GetValue(img) is string b64 && !string.IsNullOrWhiteSpace(b64))
            {
                try { return Convert.FromBase64String(b64); } catch { }
            }

            var urlProp = typeof(GeneratedImage).GetProperty("Url");
            if (urlProp?.GetValue(img) is Uri uri && uri.IsAbsoluteUri)
            {
                using var http = new HttpClient();
                return await http.GetByteArrayAsync(uri, ct);
            }

            return Array.Empty<byte>();
        }

        private static string MakeSafeFileName(string name)
        {
            foreach (var c in Path.GetInvalidFileNameChars())
                name = name.Replace(c, '_');
            return name;
        }
        #endregion
    }
}
