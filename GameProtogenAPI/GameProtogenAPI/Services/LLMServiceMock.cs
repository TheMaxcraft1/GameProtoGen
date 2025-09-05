using GameProtogenAPI.Services.Contracts;
using System.Text.Json;

namespace GameProtogenAPI.Services
{
    public class LLMServiceMock
    {
        public Task<object> GetOpsAsync(string prompt, object? scene, CancellationToken ct = default)
        {
            // 1) --- NANO (simulado): construir PLAN A/M/E --------------------
            var plan = BuildPlanFromPromptAndScene(prompt ?? string.Empty, scene);

            // 2) --- GPT-4.1 (simulado): traducir PLAN -> ops -----------------
            var ops = TranslatePlanToOps(plan);

            return Task.FromResult<object>(new { ops });
        }

        // ----- Paso 1: construir PLAN con { Agregar, Modificar, Eliminar } -----
        private static JsonElement BuildPlanFromPromptAndScene(string prompt, object? scene)
        {
            var p = prompt.ToLowerInvariant();
            using var doc = JsonDocument.Parse("{}");
            var plan = new
            {
                Agregar = new List<object>(),
                Modificar = new List<object>(),
                Eliminar = new List<object>()
            };

            // Heurística: “plataforma”/“box”
            bool wantsPlatform = p.Contains("plataforma") || p.Contains("platform") || p.Contains("box");
            bool wantsMovePlayer = p.Contains("mover jugador") || p.Contains("move player");

            bool hasAnyPlatform = SceneHasAnyWideFlatPlatform(scene);

            if (wantsPlatform && !hasAnyPlatform)
            {
                plan.Agregar.Add(new
                {
                    tipo = "plataforma",
                    pos = new[] { 800, 700 },
                    tam = new[] { 300, 40 },
                    color = "#3C3C46FF"
                });
            }

            if (wantsMovePlayer)
            {
                plan.Modificar.Add(new
                {
                    id = 1,
                    propiedad = "posicion",
                    valor = new[] { 640, 480 }
                });
            }

            // Serializamos el plan a JsonElement (igual que haría NANO real)
            var json = JsonSerializer.Serialize(plan);
            return JsonSerializer.Deserialize<JsonElement>(json);
        }

        private static bool SceneHasAnyWideFlatPlatform(object? scene)
        {
            try
            {
                if (scene is null) return false;
                JsonElement se = scene switch
                {
                    JsonElement je => je,
                    _ => JsonSerializer.Deserialize<JsonElement>(JsonSerializer.Serialize(scene))
                };
                if (!se.TryGetProperty("entities", out var ents) || ents.ValueKind != JsonValueKind.Array)
                    return false;

                foreach (var e in ents.EnumerateArray())
                {
                    if (e.TryGetProperty("Sprite", out var sp) &&
                        sp.TryGetProperty("size", out var size) &&
                        size.ValueKind == JsonValueKind.Array && size.GetArrayLength() == 2)
                    {
                        var w = size[0].GetSingle();
                        var h = size[1].GetSingle();
                        if (w >= 250 && h <= 60) return true;
                    }
                }
            }
            catch { }
            return false;
        }

        // ----- Paso 2: traducir PLAN -> ops (como GPT-4.1) ---------------------
        private static List<object> TranslatePlanToOps(JsonElement plan)
        {
            var ops = new List<object>();

            // Agregar -> spawn_box
            if (plan.TryGetProperty("Agregar", out var agregar) && agregar.ValueKind == JsonValueKind.Array)
            {
                foreach (var item in agregar.EnumerateArray())
                {
                    if (!item.TryGetProperty("tipo", out var tipoEl) || tipoEl.ValueKind != JsonValueKind.String)
                        continue;

                    var tipo = tipoEl.GetString();
                    if (tipo is "plataforma" or "caja" or "box")
                    {
                        if (TryReadVec2(item, "pos", out var pos) && TryReadVec2(item, "tam", out var tam))
                        {
                            string? colorHex = item.TryGetProperty("color", out var col) && col.ValueKind == JsonValueKind.String
                                ? col.GetString() : null;

                            ops.Add(new
                            {
                                op = "spawn_box",
                                pos = pos,
                                size = tam,
                                colorHex = colorHex
                            });
                        }
                    }
                }
            }

            // Modificar -> set_transform / set_component
            if (plan.TryGetProperty("Modificar", out var modificar) && modificar.ValueKind == JsonValueKind.Array)
            {
                foreach (var item in modificar.EnumerateArray())
                {
                    if (!item.TryGetProperty("id", out var idEl) || !idEl.TryGetUInt32(out uint id))
                        continue;
                    if (!item.TryGetProperty("propiedad", out var propEl) || propEl.ValueKind != JsonValueKind.String)
                        continue;

                    var prop = propEl.GetString() ?? string.Empty;

                    if (prop == "posicion" && TryReadVec2(item, "valor", out var pos))
                    {
                        ops.Add(new { op = "set_transform", entity = id, position = pos });
                    }
                    else if (prop == "escala" && TryReadVec2(item, "valor", out var sc))
                    {
                        ops.Add(new { op = "set_transform", entity = id, scale = sc });
                    }
                    else if (prop == "rotacion" && item.TryGetProperty("valor", out var rotEl) && rotEl.ValueKind == JsonValueKind.Number)
                    {
                        ops.Add(new { op = "set_transform", entity = id, rotation = rotEl.GetSingle() });
                    }
                    else if (prop == "color" && item.TryGetProperty("valor", out var colorEl) && colorEl.ValueKind == JsonValueKind.String)
                    {
                        // Convertimos #RRGGBBAA a r,g,b,a (0..255)
                        if (TryParseHexColor(colorEl.GetString()!, out var rgba))
                        {
                            ops.Add(new
                            {
                                op = "set_component",
                                entity = id,
                                component = "Sprite",
                                value = new
                                {
                                    color = new { r = rgba.r, g = rgba.g, b = rgba.b, a = rgba.a }
                                }
                            });
                        }
                    }
                    else if (prop.StartsWith("componente.", StringComparison.OrdinalIgnoreCase) &&
                             item.TryGetProperty("valor", out var valObj) && valObj.ValueKind == JsonValueKind.Object)
                    {
                        var comp = prop.Substring("componente.".Length);
                        ops.Add(new
                        {
                            op = "set_component",
                            entity = id,
                            component = comp,
                            value = valObj
                        });
                    }
                }
            }

            // Eliminar -> remove_entity
            if (plan.TryGetProperty("Eliminar", out var eliminar) && eliminar.ValueKind == JsonValueKind.Array)
            {
                foreach (var item in eliminar.EnumerateArray())
                {
                    if (item.TryGetProperty("id", out var idEl) && idEl.TryGetUInt32(out uint id))
                    {
                        ops.Add(new { op = "remove_entity", entity = id });
                    }
                }
            }

            return ops;
        }

        // ----- helpers ---------------------------------------------------------
        private static bool TryReadVec2(JsonElement obj, string prop, out float[] vec2)
        {
            vec2 = Array.Empty<float>();
            if (!obj.TryGetProperty(prop, out var a) || a.ValueKind != JsonValueKind.Array || a.GetArrayLength() != 2)
                return false;
            vec2 = new[] { a[0].GetSingle(), a[1].GetSingle() };
            return true;
        }

        private static bool TryParseHexColor(string hex, out (byte r, byte g, byte b, byte a) rgba)
        {
            // Acepta #RRGGBBAA o #AARRGGBB (común ver ambos)
            rgba = (255, 255, 255, 255);
            string h = hex.Trim();
            if (h.StartsWith("#")) h = h[1..];
            if (h.Length != 8) return false;

            try
            {
                byte b0 = Convert.ToByte(h.Substring(0, 2), 16);
                byte b1 = Convert.ToByte(h.Substring(2, 2), 16);
                byte b2 = Convert.ToByte(h.Substring(4, 2), 16);
                byte b3 = Convert.ToByte(h.Substring(6, 2), 16);

                // Heurística: si parece #RRGGBBAA (más común en UI), úsalo así; si prefieres AA primero, gira.
                // Aquí asumimos #RRGGBBAA:
                rgba = (b0, b1, b2, b3);
                return true;
            }
            catch { return false; }
        }
    }
}
