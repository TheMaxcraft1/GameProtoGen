using FluentAssertions;
using GameProtogenAPI.OnlineTests.Infra;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace GameProtogenAPI.OnlineTests
{
    public class RouteTests
    {
        [Fact]
        public async Task Route_Returns_Json_With_Agents()
        {
            var (llm, _) = ServiceFactory.CreateOnlineServices();
            var json = await llm.RouteAsync("Generá un sprite de mago", SceneUtils.DefaultSceneJson, default);

            json.Should().NotBeNullOrWhiteSpace();
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            root.TryGetProperty("agents", out var agents).Should().BeTrue();
            agents.ValueKind.Should().Be(JsonValueKind.Array);
            agents.GetArrayLength().Should().BeGreaterThan(0);

            if (agents.EnumerateArray().Any(e => e.GetString() == "asset_gen"))
            {
                root.TryGetProperty("asset_mode", out var mode).Should().BeTrue();
                mode.GetString().Should().MatchRegex("^(sprite|texture)$");
            }
        }
    }
}
