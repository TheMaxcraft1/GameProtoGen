using GameProtogenAPI.Services.Contracts;
using System.Globalization;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using System.Xml.Linq;

namespace GameProtogenAPI.Services
{
    public class LLMServiceMock : ILLMService
    {
        // Paso 1: user + escena → PLAN XML
        public Task<string> BuildEditPlanXmlAsync(string userPrompt, string sceneJson, CancellationToken ct = default)
        {
            var p = (userPrompt ?? string.Empty).ToLowerInvariant();

            var agregar = new List<XElement>();
            var modificar = new List<XElement>();
            var eliminar = new List<XElement>();

            // Agregar plataforma / caja
            if (p.Contains("plataforma") || p.Contains("platform") || p.Contains("box") || p.Contains("caja"))
            {
                agregar.Add(new XElement("item",
                    new XAttribute("tipo", "plataforma"),
                    new XAttribute("pos", "800,700"),
                    new XAttribute("tam", "300,40"),
                    new XAttribute("color", "#3C3C46FF")
                ));
            }

            // Mover jugador
            if (p.Contains("mover jugador") || p.Contains("move player"))
            {
                modificar.Add(new XElement("item",
                    new XAttribute("id", "1"),
                    new XAttribute("propiedad", "posicion"),
                    new XAttribute("valor", "640,480")
                ));
            }

            // Cambiar color (p. ej. "color #FF0000FF")
            var mColor = Regex.Match(p, @"color\s*#([0-9a-f]{6,8})", RegexOptions.IgnoreCase);
            if (mColor.Success)
            {
                modificar.Add(new XElement("item",
                    new XAttribute("id", "1"),
                    new XAttribute("propiedad", "color"),
                    new XAttribute("valor", "#" + mColor.Groups[1].Value)
                ));
            }

            // Eliminar N (p. ej. "eliminar 5" / "delete 5" / "borrar 5")
            var mDel = Regex.Match(p, @"\b(eliminar|delete|borrar)\s+(\d+)\b", RegexOptions.IgnoreCase);
            if (mDel.Success)
            {
                eliminar.Add(new XElement("item", new XAttribute("id", mDel.Groups[2].Value)));
            }

            var plan = new XElement("plan",
                new XElement("agregar", agregar),
                new XElement("modificar", modificar),
                new XElement("eliminar", eliminar)
            );

            // Devolvemos XML compacto (sin saltos), tal como haría el NANO
            return Task.FromResult(plan.ToString(SaveOptions.DisableFormatting));
        }

        // Paso 2: PLAN XML → { "ops": [...] }
        public Task<string> PlanToOpsJsonAsync(string planXml, CancellationToken ct = default)
        {
            var doc = XDocument.Parse(planXml);
            var root = doc.Root ?? throw new InvalidOperationException("plan XML inválido");
            var ops = new List<object>();

            // ---- agregar → spawn_box ----
            foreach (var it in root.Element("agregar")?.Elements("item") ?? Enumerable.Empty<XElement>())
            {
                var tipo = it.Attribute("tipo")?.Value?.ToLowerInvariant();
                if (tipo is "plataforma" or "caja" or "box")
                {
                    var pos = ParseVec2(it.Attribute("pos")?.Value);
                    var tam = ParseVec2(it.Attribute("tam")?.Value);
                    var color = it.Attribute("color")?.Value;

                    if (pos is not null && tam is not null)
                    {
                        ops.Add(new { op = "spawn_box", pos, size = tam, colorHex = color });
                    }
                }
            }

            // ---- modificar → set_transform / set_component ----
            foreach (var it in root.Element("modificar")?.Elements("item") ?? Enumerable.Empty<XElement>())
            {
                if (!uint.TryParse(it.Attribute("id")?.Value, out var id)) continue;
                var prop = it.Attribute("propiedad")?.Value?.ToLowerInvariant();
                var val = it.Attribute("valor")?.Value;

                if (prop == "posicion")
                {
                    var v = ParseVec2(val);
                    if (v is not null) ops.Add(new { op = "set_transform", entity = id, position = v });
                }
                else if (prop == "escala")
                {
                    var v = ParseVec2(val);
                    if (v is not null) ops.Add(new { op = "set_transform", entity = id, scale = v });
                }
                else if (prop == "rotacion")
                {
                    if (TryParseFloat(val, out var rot))
                        ops.Add(new { op = "set_transform", entity = id, rotation = rot });
                }
                else if (prop == "color")
                {
                    if (TryParseHexColor(val ?? string.Empty, out var rgba))
                    {
                        ops.Add(new
                        {
                            op = "set_component",
                            entity = id,
                            component = "Sprite",
                            value = new { color = new { r = rgba.r, g = rgba.g, b = rgba.b, a = rgba.a } }
                        });
                    }
                }
            }

            // ---- eliminar → remove_entity ----
            foreach (var it in root.Element("eliminar")?.Elements("item") ?? Enumerable.Empty<XElement>())
            {
                if (uint.TryParse(it.Attribute("id")?.Value, out var id))
                    ops.Add(new { op = "remove_entity", entity = id });
            }

            var json = JsonSerializer.Serialize(new { ops }, new JsonSerializerOptions
            {
                DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
            });
            return Task.FromResult(json);
        }

        // ---------------- Helpers ----------------

        private static float[]? ParseVec2(string? s)
        {
            if (string.IsNullOrWhiteSpace(s)) return null;
            var parts = s.Split(',', StringSplitOptions.TrimEntries | StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length != 2) return null;

            if (TryParseFloat(parts[0], out var x) && TryParseFloat(parts[1], out var y))
                return new[] { x, y };

            return null;
        }

        private static bool TryParseFloat(string? s, out float value)
        {
            return float.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out value);
        }

        // Acepta #RRGGBB o #RRGGBBAA; devuelve RGBA
        private static bool TryParseHexColor(string hex, out (byte r, byte g, byte b, byte a) rgba)
        {
            rgba = (255, 255, 255, 255);
            var h = hex.Trim();
            if (h.StartsWith("#")) h = h[1..];
            if (h.Length is not (6 or 8)) return false;

            try
            {
                byte r = Convert.ToByte(h.Substring(0, 2), 16);
                byte g = Convert.ToByte(h.Substring(2, 2), 16);
                byte b = Convert.ToByte(h.Substring(4, 2), 16);
                byte a = (h.Length == 8) ? Convert.ToByte(h.Substring(6, 2), 16) : (byte)255;
                rgba = (r, g, b, a);
                return true;
            }
            catch { return false; }
        }
    }
}
