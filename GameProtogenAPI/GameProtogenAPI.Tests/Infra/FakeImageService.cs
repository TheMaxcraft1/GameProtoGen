using GameProtogenAPI.Services.Contracts;
using OpenAI.Images;
using System;
using System.ClientModel;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GameProtogenAPI.Tests.Infra
{
    public class FakeImageService : IImageService
    {
        public Task<ClientResult<GeneratedImage>> GenerateImage(string prompt, bool transparent, CancellationToken ct = default)
        {
            // Devolvemos un GeneratedImage mínimo a través de reflexión (o levantamos excepción controlada)
            // Para los tests API, basta con que no explote: mockea una imagen “vacía”.
            var bytes = new byte[] { 137, 80, 78, 71 }; // header PNG mínimo (simbolico)
            var gen = (GeneratedImage)Activator.CreateInstance(typeof(GeneratedImage), nonPublic: true)!;
            // Seteamos propiedades vía reflexión si fuese necesario, o devolvemos un ClientResult que falle controlado.
            // Para simplificar, lanzamos una excepción y verificamos el manejo en la API:
            throw new Exception("Fake image generator not implemented (expected in offline tests).");
        }
    }
}
