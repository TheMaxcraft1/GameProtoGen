using System.Text.Json;

namespace GameProtogenAPI.Validators
{
    public static class OpsValidator
    {
        /// <summary>
        /// Valida que la forma del JSON de ops cumpla el contrato mínimo.
        /// </summary>
        public static bool TryValidateOps(JsonElement root, out string error)
        {
            error = string.Empty;
            if (!root.TryGetProperty("ops", out var opsElem) || opsElem.ValueKind != JsonValueKind.Array)
            {
                error = "Formato inválido: falta 'ops' o no es array.";
                return false;
            }

            foreach (var op in opsElem.EnumerateArray())
            {
                if (!op.TryGetProperty("op", out var opTypeElem) || opTypeElem.ValueKind != JsonValueKind.String)
                {
                    error = "Operación sin 'op' string.";
                    return false;
                }

                var opType = opTypeElem.GetString();
                switch (opType)
                {
                    case "spawn_box":
                        if (!RequireArray(op, "pos", 2, ref error)) return false;
                        if (!RequireArray(op, "size", 2, ref error)) return false;
                        // colorHex opcional
                        break;

                    case "set_transform":
                        if (!RequireUInt(op, "entity", ref error)) return false;
                        // position/scale/rotation opcionales; si vienen, validar formas
                        if (op.TryGetProperty("position", out var pos) && !IsArrayLen(pos, 2))
                        { error = "set_transform.position debe ser array[2]."; return false; }
                        if (op.TryGetProperty("scale", out var sc) && !IsArrayLen(sc, 2))
                        { error = "set_transform.scale debe ser array[2]."; return false; }
                        break;

                    case "remove_entity":
                        if (!RequireUInt(op, "entity", ref error)) return false;
                        break;

                    case "set_component":
                        if (!RequireUInt(op, "entity", ref error)) return false;
                        if (!op.TryGetProperty("component", out var comp) || comp.ValueKind != JsonValueKind.String)
                        { error = "set_component.component string requerido."; return false; }
                        if (!op.TryGetProperty("value", out var val) || val.ValueKind != JsonValueKind.Object)
                        { error = "set_component.value object requerido."; return false; }
                        // Permite "Sprite" (como antes) y "Texture2D" con {path}
                        if (comp.GetString() == "Texture2D")
                        {
                            if (!val.TryGetProperty("path", out var p) || p.ValueKind != JsonValueKind.String)
                            { error = "Texture2D.value.path string requerido."; return false; }
                        }
                        break;

                    default:
                        error = $"Operación desconocida: {opType}";
                        return false;
                }
            }
            return true;
        }

        static bool RequireArray(JsonElement obj, string prop, int len, ref string error)
        {
            if (!obj.TryGetProperty(prop, out var a) || !IsArrayLen(a, len))
            { error = $"'{prop}' debe ser array[{len}]."; return false; }
            return true;
        }

        static bool IsArrayLen(JsonElement e, int len)
            => e.ValueKind == JsonValueKind.Array && e.GetArrayLength() == len;

        static bool RequireUInt(JsonElement obj, string prop, ref string error)
        {
            if (!obj.TryGetProperty(prop, out var v) || (v.ValueKind != JsonValueKind.Number) || !v.TryGetUInt32(out _))
            { error = $"'{prop}' debe ser uint."; return false; }
            return true;
        }
    }
}
