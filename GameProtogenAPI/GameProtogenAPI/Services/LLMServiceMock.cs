using GameProtogenAPI.Services.Contracts;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace GameProtogenAPI.Services
{
    public class LLMServiceMock : ILLMService
    {
        public Task<string> BuildEditPlanXmlAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            var p = (userPrompt ?? string.Empty).ToLowerInvariant();

            bool wantsPlatform = p.Contains("plataforma") || p.Contains("platform") || p.Contains("box");
            bool wantsMovePlayer = p.Contains("mover jugador") || p.Contains("move player");
            uint? deleteId = TryMatchDeleteId(userPrompt);
            string? colorHexInPrompt = TryMatchHexColor(userPrompt);

            bool hasAnyPlatform = SceneHasAnyWideFlatPlatform(sceneJson);

            var plan = new XElement("plan",
                new XElement("agregar"),
                new XElement("modificar"),
                new XElement("eliminar")
            );

            // Agregar plataforma por defecto si se pidió y no hay ninguna
            if (wantsPlatform && !hasAnyPlatform)
            {
                var item = new XElement("item",
                    new XAttribute("tipo", "plataforma"),
                    new XAttribute("pos", "800,700"),
                    new XAttribute("tam", "300,40")
                );
                if (!string.IsNullOrWhiteSpace(colorHexInPrompt))
                    item.SetAttributeValue("color", colorHexInPrompt);
                plan.Element("agregar")!.Add(item);
            }

            // Mover jugador (id=1 en MVP)
            if (wantsMovePlayer)
            {
                plan.Element("modificar")!.Add(
                    new XElement("item",
                        new XAttribute("id", "1"),
                        new XAttribute("propiedad", "posicion"),
                        new XAttribute("valor", "640,480"))
                );
            }

            // Cambiar color del jugador si se dio un #hex
            if (!string.IsNullOrWhiteSpace(colorHexInPrompt))
            {
                plan.Element("modificar")!.Add(
                    new XElement("item",
                        new XAttribute("id", "1"),
                        new XAttribute("propiedad", "color"),
                        new XAttribute("valor", colorHexInPrompt))
                );
            }

            // Eliminar entidad
            if (deleteId.HasValue)
            {
                plan.Element("eliminar")!.Add(
                    new XElement("item",
                        new XAttribute("id", deleteId.Value.ToString()))
                );
            }

            var xml = plan.ToString(SaveOptions.DisableFormatting);
            return Task.FromResult(xml);
        }

        public Task<string> PlanToOpsJsonAsync(string planXml, CancellationToken ct = default)
        {
            var ops = new List<object>();
            var doc = XDocument.Parse(planXml);

            // agregar → spawn_box
            foreach (var item in doc.Descendants("agregar").Descendants("item"))
            {
                string tipo = (string?)item.Attribute("tipo") ?? "";
                if (tipo is "plataforma" or "caja" or "box")
                {
                    if (TryParseVec2Attr(item, "pos", out var pos) && TryParseVec2Attr(item, "tam", out var tam))
                    {
                        var obj = new Dictionary<string, object?>
                        {
                            ["op"] = "spawn_box",
                            ["pos"] = pos,
                            ["size"] = tam
                        };
                        var col = (string?)item.Attribute("color");
                        if (!string.IsNullOrWhiteSpace(col)) obj["colorHex"] = col;
                        ops.Add(obj);
                    }
                }
            }

            // modificar → set_transform / set_component(Sprite.color)
            foreach (var item in doc.Descendants("modificar").Descendants("item"))
            {
                if (!uint.TryParse((string?)item.Attribute("id"), out var id)) continue;
                string prop = (string?)item.Attribute("propiedad") ?? "";
                string? val = (string?)item.Attribute("valor");

                if (prop == "posicion" && TryParseVec2Csv(val, out var pos))
                {
                    ops.Add(new { op = "set_transform", entity = id, position = pos });
                }
                else if (prop == "escala" && TryParseVec2Csv(val, out var sc))
                {
                    ops.Add(new { op = "set_transform", entity = id, scale = sc });
                }
                else if (prop == "rotacion" && float.TryParse(val, out var rot))
                {
                    ops.Add(new { op = "set_transform", entity = id, rotation = rot });
                }
                else if (prop == "color" && TryParseHexColor(val, out var rgba))
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

            // eliminar → remove_entity
            foreach (var item in doc.Descendants("eliminar").Descendants("item"))
            {
                if (uint.TryParse((string?)item.Attribute("id"), out var id))
                {
                    ops.Add(new { op = "remove_entity", entity = id });
                }
            }

            var json = JsonSerializer.Serialize(new { ops });
            return Task.FromResult(json);
        }

        public Task<string> RouteAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            var p = (userPrompt ?? "").ToLowerInvariant();
            var design = p.Contains("diseñ") || p.Contains("design") || p.Contains("cómo") || p.Contains("why");
            var agents = design ? new[] { "design_qa" } : new[] { "scene_edit" };
            var json = JsonSerializer.Serialize(new { agents, reason = "mock" });
            return Task.FromResult(json);
        }

        public Task<string> AskDesignAsync(string userQuestion, CancellationToken ct = default)
        {
            return Task.FromResult(
                "Mock (design_qa): Para un salto responsivo probá gravedad 1800–2400 px/s², " +
                "velocidad de salto 800–1000 px/s, coyote time 80–120 ms e input buffer 80–120 ms."
            );
        }

        // Helpers -----------------------------------------------------------

        private static bool SceneHasAnyWideFlatPlatform(string sceneJson)
        {
            try
            {
                using var doc = JsonDocument.Parse(string.IsNullOrWhiteSpace(sceneJson) ? "{}" : sceneJson);
                if (!doc.RootElement.TryGetProperty("entities", out var ents) || ents.ValueKind != JsonValueKind.Array)
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

        private static uint? TryMatchDeleteId(string text)
        {
            if (string.IsNullOrWhiteSpace(text)) return null;
            var m = Regex.Match(text, @"\b(eliminar|remove)\s+(\d+)\b", RegexOptions.IgnoreCase);
            return m.Success && uint.TryParse(m.Groups[2].Value, out var id) ? id : null;
        }

        private static string? TryMatchHexColor(string text)
        {
            if (string.IsNullOrWhiteSpace(text)) return null;
            var m = Regex.Match(text, @"#([A-Fa-f0-9]{8}|[A-Fa-f0-9]{6})");
            return m.Success ? m.Value : null;
        }

        private static bool TryParseVec2Attr(XElement el, string attr, out float[] vec2)
            => TryParseVec2Csv((string?)el.Attribute(attr), out vec2);

        private static bool TryParseVec2Csv(string? csv, out float[] vec2)
        {
            vec2 = Array.Empty<float>();
            if (string.IsNullOrWhiteSpace(csv)) return false;
            var parts = csv.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 2) return false;
            if (float.TryParse(parts[0], out var x) && float.TryParse(parts[1], out var y))
            {
                vec2 = new[] { x, y };
                return true;
            }
            return false;
        }

        private static bool TryParseHexColor(string? hex, out (byte r, byte g, byte b, byte a) rgba)
        {
            rgba = (255, 255, 255, 255);
            if (string.IsNullOrWhiteSpace(hex)) return false;
            string h = hex.Trim();
            if (h.StartsWith("#")) h = h[1..];
            if (h.Length == 6) h += "FF"; // añadir alfa
            if (h.Length != 8) return false;
            try
            {
                byte r = Convert.ToByte(h.Substring(0, 2), 16);
                byte g = Convert.ToByte(h.Substring(2, 2), 16);
                byte b = Convert.ToByte(h.Substring(4, 2), 16);
                byte a = Convert.ToByte(h.Substring(6, 2), 16);
                rgba = (r, g, b, a);
                return true;
            }
            catch { return false; }
        }
    }
}
