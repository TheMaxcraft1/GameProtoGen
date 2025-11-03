using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Logging.Abstractions;
using OpenAI.Images;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GameProtogenAPI.OnlineTests.Infra
{
    public class ServiceFactory
    {
        public static (ILLMService llm, IImageService images) CreateOnlineServices()
        {
            RequireCreds(); // falla con mensaje claro si faltan vars

            // Leemos desde variables de entorno para LLMService
            var cfg = new ConfigurationBuilder()
                .AddUserSecrets(typeof(ServiceFactory).Assembly, optional: true)
                .AddEnvironmentVariables()
                .Build();

            // Cliente de imágenes (OPENAI_API_KEY + modelo fijo como en Program.cs)
            var openAiKey = cfg["OpenAI:ApiKey"] ??
                cfg["OPENAI_API_KEY"] ??
                Environment.GetEnvironmentVariable("OPENAI_API_KEY");

            var imageClient = new ImageClient("gpt-image-1", openAiKey);
            var imageLogger = NullLogger<OpenAIImageGenService>.Instance;
            IImageService imageService = new OpenAIImageGenService(imageClient, imageLogger);

            // LLM real (Azure AI Inference + IImageService)
            var llmLogger = NullLogger<LLMService>.Instance;
            ILLMService llm = new LLMService(llmLogger, imageService, cfg);

            return (llm, imageService);
        }

        private static void RequireCreds()
        {
            var cfg = new ConfigurationBuilder()
                .AddUserSecrets(typeof(ServiceFactory).Assembly, optional: true)
                .AddEnvironmentVariables()
                .Build();

            var azureAIInferenceEndpoint =
                cfg["AzureAI:InferenceEndpoint"] ??
                cfg["AZURE_INFERENCE_ENDPOINT"] ??
                Environment.GetEnvironmentVariable("AZURE_INFERENCE_ENDPOINT");

            var azureAIInferenceApiKey =
                cfg["AzureAI:InferenceApiKey"] ??
                cfg["AZURE_INFERENCE_API_KEY"] ??
                Environment.GetEnvironmentVariable("AZURE_INFERENCE_API_KEY");

            var openAiKey = cfg["OpenAI:ApiKey"] ??
                cfg["OPENAI_API_KEY"] ??
                Environment.GetEnvironmentVariable("OPENAI_API_KEY");

            var ok = !string.IsNullOrWhiteSpace(azureAIInferenceEndpoint)
                  && !string.IsNullOrWhiteSpace(azureAIInferenceApiKey)
                  && !string.IsNullOrWhiteSpace(openAiKey);

            Xunit.Assert.True(ok,
                "Faltan credenciales. Seteá AZURE_INFERENCE_ENDPOINT, AZURE_INFERENCE_API_KEY y OPENAI_API_KEY en tu entorno.");
        }
    }
}
