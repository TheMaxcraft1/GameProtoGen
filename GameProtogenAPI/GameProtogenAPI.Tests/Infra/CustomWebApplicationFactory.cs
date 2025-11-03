using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using Microsoft.AspNetCore.Authentication;
using Microsoft.AspNetCore.Hosting;
using Microsoft.AspNetCore.Mvc.Testing;
using Microsoft.AspNetCore.TestHost;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.VisualStudio.TestPlatform.TestHost;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http.Headers;
using System.Text;
using System.Threading.Tasks;

namespace GameProtogenAPI.Tests.Infra
{
    public class CustomWebApplicationFactory : WebApplicationFactory<Program>
    {
        protected override void ConfigureWebHost(IWebHostBuilder builder)
        {
            builder.UseEnvironment("Development");

            builder.ConfigureTestServices(services =>
            {
                // 1) Autenticación de test (evita JWT real)
                services.AddAuthentication(options =>
                {
                    options.DefaultAuthenticateScheme = "TestAuth";
                    options.DefaultChallengeScheme = "TestAuth";
                }).AddScheme<AuthenticationSchemeOptions, TestAuthHandler>("TestAuth", _ => { });

                // 2) Reemplazar servicios online por mocks
                services.AddSingleton<ILLMService, LLMServiceMock>();
                services.AddSingleton<IImageService, FakeImageService>();
            });
        }

        // Helper para cliente autenticado
        public HttpClient CreateAuthenticatedClient()
        {
            var client = CreateClient(new WebApplicationFactoryClientOptions
            {
                AllowAutoRedirect = false
            });

            // IMPORTANTE: usar el esquema "Test" (no "Bearer")
            client.DefaultRequestHeaders.Authorization =
                new AuthenticationHeaderValue("Test", "faketoken");

            return client;
        }
    }
}
