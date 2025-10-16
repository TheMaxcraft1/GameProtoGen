using OpenAI.Images;
using System.ClientModel;

namespace GameProtogenAPI.Services.Contracts
{
    public interface IImageService
    {
        Task<ClientResult<GeneratedImage>> GenerateImage(string prompt, bool transparent, CancellationToken ct = default);
    }
}
