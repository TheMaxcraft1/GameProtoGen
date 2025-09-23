using Microsoft.SemanticKernel;
using OpenAI.Images;
using System.ComponentModel;
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

        [KernelFunction("generate_asset")]
        [Description("Genera una imagen (textura/sprite) y la devuelve en base64 para que el cliente la guarde localmente.")]
        public async Task<string> GenerateAsync(
        [Description("Descripción de la textura a generar")] string prompt,
        CancellationToken ct = default)
        {
            if (string.IsNullOrWhiteSpace(prompt))
                return JsonSerializer.Serialize(new { kind = "text", message = "Descripción vacía." });

            // Tamaño fijo inicial (512x512). Si más adelante querés parametrizar ancho/alto, lo ampliamos.
#pragma warning disable OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.
            var options = new ImageGenerationOptions { 
                Size = GeneratedImageSize.W1024xH1024, 
                Quality = GeneratedImageQuality.Medium,
                Background = GeneratedImageBackground.Transparent
            };
#pragma warning restore OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.

            GeneratedImage img;
            try
            {
                var gen = await _images.GenerateImageAsync(prompt, options, ct);
                img = gen.Value;
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Fallo generando imagen");
                return JsonSerializer.Serialize(new { kind = "text", message = $"No pude generar la imagen: {ex.Message}" });
            }

            byte[] bytes = await GetBytesAsync(img, ct);
            if (bytes.Length == 0)
                return JsonSerializer.Serialize(new { kind = "text", message = "El modelo no devolvió datos de imagen." });

            var fileName = MakeSafeFileName($"{DateTime.UtcNow:yyyyMMdd_HHmmssfff}.png");
            var path = $"Assets/Generated/{fileName}";
            var b64 = Convert.ToBase64String(bytes);
            return JsonSerializer.Serialize(new
            {
                kind = "asset",
                fileName,
                path, // <--- NUEVO
                data = b64
            });
        }

        // --- helpers ---
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

        private static string MakeSafeFileName(string name)
        {
            foreach (var c in Path.GetInvalidFileNameChars())
                name = name.Replace(c, '_');
            return name;
        }
    }
}
