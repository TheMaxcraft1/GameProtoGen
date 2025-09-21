using OpenAI.Images;
using System.Text.Json;

namespace GameProtogenAPI.AI.AgentPlugins
{
    public class AssetGenPlugin
    {
        private readonly ImageClient _images;
        private readonly ILogger<AssetGenPlugin> _logger;

        public AssetGenPlugin(ImageClient images, ILogger<AssetGenPlugin> logger)
        {
            _images = images;
            _logger = logger;
        }

        /// Genera una textura PNG desde una descripción y la guarda en Assets/Generated.
        /// Devuelve JSON: { "kind":"asset", "path":"Assets/Generated/xxx.png", "w":512, "h":512 }
        public async Task<string> GenerateTextureAsync(
            string description,
            int width = 512,
            int height = 512,
            CancellationToken ct = default)
        {
            if (string.IsNullOrWhiteSpace(description))
                return JsonSerializer.Serialize(new { kind = "text", message = "Descripción vacía." });

            // 1) La SDK 2.4.0 usa tamaños discretos (256/512/1024) y cuadrados:
            var sizeEnum = MapToGeneratedImageSize(width, height, out int actual);

            var options = new ImageGenerationOptions
            {
                // No uses string "WxH": el tipo aquí es GeneratedImageSize?
                Size = sizeEnum
                // Consejo: si tu build expone un "ResponseFormat/Format"
                // podés forzar bytes; si no, igual abajo cubrimos base64/URL.
            };

            GeneratedImage img;
            try
            {
                var gen = await _images.GenerateImageAsync(description, options, ct);
                img = gen.Value; // Si falla, la SDK lanza excepción antes de esto
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Fallo generando imagen");
                return JsonSerializer.Serialize(new { kind = "text", message = $"No pude generar la imagen: {ex.Message}" });
            }

            // 2) Extraer bytes (cubre bytes/base64/url según qué devuelva el modelo)
            byte[] bytes = await GetBytesAsync(img, ct);
            if (bytes.Length == 0)
                return JsonSerializer.Serialize(new { kind = "text", message = "El modelo no devolvió datos de imagen." });

            // 3) Guardar a disco
            var fileName = MakeSafeFileName($"{DateTime.UtcNow:yyyyMMdd_HHmmssfff}_{TrimForName(description, 32)}.png");
            var dir = Path.Combine("Assets", "Generated");
            Directory.CreateDirectory(dir);
            var path = Path.Combine(dir, fileName);
            await File.WriteAllBytesAsync(path, bytes, ct);

            _logger.LogInformation("Imagen guardada en {Path} ({Bytes} bytes)", path, bytes.Length);

            // 4) Respuesta para tu editor / orquestador
            return JsonSerializer.Serialize(new
            {
                kind = "asset",
                path,
                w = actual,
                h = actual
            });
        }

        // --- helpers ---

        // Mapea (w,h) arbitrario al enum cuadrado más cercano (256/512/1024)
        private static GeneratedImageSize MapToGeneratedImageSize(int w, int h, out int actual)
        {
            int target = Math.Max(w, h);
            int nearest = 512;
            if (target <= 256) nearest = 256;
            else if (target <= 512) nearest = 512;
            else nearest = 1024;

            actual = nearest;

            // Enum típicos en 2.4.0: GeneratedImageSize.Size256x256 / Size512x512 / Size1024x1024
            // Si tu build los nombra distinto, ajustá aquí.
            return nearest switch
            {
                256 => GeneratedImageSize.W256xH256,
                1024 => GeneratedImageSize.W1024xH1024,
                _ => GeneratedImageSize.W512xH512
            };
        }

        private static async Task<byte[]> GetBytesAsync(GeneratedImage img, CancellationToken ct)
        {
            // Intentamos propiedades comunes vía reflexión para tolerar variaciones de la SDK
            var bytesProp = typeof(GeneratedImage).GetProperty("ImageBytes");
            if (bytesProp?.GetValue(img) is BinaryData bd)
                return bd.ToArray();

            var b64Prop = typeof(GeneratedImage).GetProperty("Base64Data");
            if (b64Prop?.GetValue(img) is string b64 && !string.IsNullOrWhiteSpace(b64))
            {
                try { return Convert.FromBase64String(b64); } catch { /* ignore */ }
            }

            var urlProp = typeof(GeneratedImage).GetProperty("Url");
            if (urlProp?.GetValue(img) is Uri uri && uri.IsAbsoluteUri)
            {
                using var http = new HttpClient();
                return await http.GetByteArrayAsync(uri, ct);
            }

            return Array.Empty<byte>();
        }

        private static string TrimForName(string s, int max)
        {
            s = new string(s.Where(ch => char.IsLetterOrDigit(ch) || char.IsWhiteSpace(ch)).ToArray());
            s = s.Trim().Replace(' ', '_');
            return s.Length <= max ? s : s[..max];
        }

        private static string MakeSafeFileName(string name)
        {
            foreach (var c in Path.GetInvalidFileNameChars())
                name = name.Replace(c, '_');
            return name;
        }
    }
}
