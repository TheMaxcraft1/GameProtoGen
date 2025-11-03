using FluentAssertions;
using GameProtogenAPI.Tests.Infra;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Http.Json;
using System.Text;
using System.Threading.Tasks;

namespace GameProtogenAPI.Tests
{
    public class ChatControllerTests : IClassFixture<CustomWebApplicationFactory>
    {
        private readonly CustomWebApplicationFactory _factory;

        public ChatControllerTests(CustomWebApplicationFactory factory)
        {
            _factory = factory;
        }

        [Fact]
        public async Task Command_Should_Return_400_When_Prompt_Empty()
        {
            var client = _factory.CreateAuthenticatedClient();

            var resp = await client.PostAsJsonAsync("/api/chat/command", new { prompt = "", scene = (object?)null });
            resp.StatusCode.Should().Be(HttpStatusCode.BadRequest);
        }

        [Fact]
        public async Task Command_With_SceneEdit_Should_Return_Ops()
        {
            var client = _factory.CreateAuthenticatedClient();

            var body = new
            {
                prompt = "agregá una plataforma y mueve al jugador a 640,480",
                scene = new
                {
                    entities = new object[]
                    {
                    new { id=1, PlayerController = new { moveSpeed=200, jumpSpeed=900 }, Sprite = new { size = new[]{32,64} } }
                    }
                }
            };

            var resp = await client.PostAsJsonAsync("/api/chat/command", body);
            resp.EnsureSuccessStatusCode();

            var json = await resp.Content.ReadAsStringAsync();
            json.Should().Contain("\"kind\":\"ops\"");
            json.Should().Contain("\"op\":\"spawn_box\"");
        }

        [Fact]
        public async Task Command_With_DesignQa_Should_Return_Text()
        {
            var client = _factory.CreateAuthenticatedClient();
            var body = new { prompt = "¿cómo balanceo el salto y la gravedad?", scene = (object?)null };

            var resp = await client.PostAsJsonAsync("/api/chat/command", body);
            resp.EnsureSuccessStatusCode();

            var json = await resp.Content.ReadAsStringAsync();
            json.Should().Contain("\"kind\":\"text\"");
        }

        [Fact]
        public async Task Health_Should_Return_Healthy()
        {
            var client = _factory.CreateClient(); // esta ruta no requiere auth
            var resp = await client.GetAsync("/health");
            resp.EnsureSuccessStatusCode();
            var s = await resp.Content.ReadAsStringAsync();
            s.Should().Contain("Healthy");
        }

        [Fact]
        public async Task Chat_Requires_Authorization()
        {
            var client = _factory.CreateClient(); // sin auth
            var resp = await client.PostAsJsonAsync("/api/chat/command", new { prompt = "hola", scene = (object?)null });
            resp.StatusCode.Should().Be(HttpStatusCode.Unauthorized);
        }
    }
}
