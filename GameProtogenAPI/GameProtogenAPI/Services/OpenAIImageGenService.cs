using GameProtogenAPI.Services.Contracts;
using Microsoft.Extensions.Options;
using OpenAI.Images;
using System.ClientModel;

namespace GameProtogenAPI.Services
{
    public class OpenAIImageGenService : IImageService
    {
        private readonly ImageClient _client;
        private readonly ILogger<OpenAIImageGenService> _logger;
        public OpenAIImageGenService(ImageClient client, ILogger<OpenAIImageGenService> logger) {
            _client = client;
            _logger = logger;
        }

        public async Task<ClientResult<GeneratedImage>> GenerateImage(string prompt, bool transparent, CancellationToken ct = default)
        {
#pragma warning disable OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.
            var opts = new ImageGenerationOptions
            {
                Size = GeneratedImageSize.W1024xH1024,
                Quality = GeneratedImageQuality.Medium,
            };

            if (transparent)
            {
                opts.Background = GeneratedImageBackground.Transparent;
            }
#pragma warning restore OPENAI001 // Type is for evaluation purposes only and is subject to change or removal in future updates. Suppress this diagnostic to proceed.

            try
            {
                var res = await _client.GenerateImageAsync(prompt, opts, ct);

                return res;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error generating image with prompt: {Prompt}", prompt);
                throw;
            }
        }
    }
}
