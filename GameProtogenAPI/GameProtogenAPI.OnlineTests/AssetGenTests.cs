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
    public class AssetGenTests
    {
        [Fact]
        public async Task Generate_Sprite_Returns_Asset_With_Base64()
        {
            var (llm, _) = ServiceFactory.CreateOnlineServices();
            var json = await llm.GenerateAssetAsync(
                "Pixel-art mage character, front-facing, idle pose",
                "sprite",
                default);

            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            root.GetProperty("kind").GetString().Should().Be("asset");
            root.GetProperty("fileName").GetString().Should().EndWith(".png");
            root.GetProperty("path").GetString().Should().StartWith("Assets/Generated/");
            Convert.FromBase64String(root.GetProperty("data").GetString()!).Length.Should().BeGreaterThan(0);
        }

        [Fact]
        public async Task Generate_Texture_Returns_Asset()
        {
            var (llm, _) = ServiceFactory.CreateOnlineServices();
            var json = await llm.GenerateAssetAsync(
                "Stone tiles seamless, flat lighting, top-down",
                "texture",
                default);

            using var doc = JsonDocument.Parse(json);
            doc.RootElement.GetProperty("kind").GetString().Should().Be("asset");
        }
    }
}
