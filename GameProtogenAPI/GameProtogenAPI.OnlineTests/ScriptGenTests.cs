using FluentAssertions;
using GameProtogenAPI.OnlineTests.Infra;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading.Tasks;

namespace GameProtogenAPI.OnlineTests
{
    public class ScriptGenTests
    {
        [Fact]
        public async Task GenerateLua_Should_Return_Json_With_Lua_Code()
        {
            var (llm, _) = ServiceFactory.CreateOnlineServices();

            var scene = /* contexto mínimo útil */ "{}";
            var json = await llm.GenerateLuaAsync(
                "Hacé que la plataforma se mueva de arriba hacia abajo",
                SceneUtils.DefaultSceneJson, default);

            json.Should().NotBeNullOrWhiteSpace();

            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            root.GetProperty("kind").GetString().Should().Be("script");
            root.GetProperty("fileName").GetString().Should().EndWith(".lua");

            var code = root.GetProperty("code").GetString();
            code.Should().NotBeNullOrWhiteSpace();

            // Contenido mínimo esperado
            code!.Should().Contain("ecs.");
            code.Should().MatchRegex(@"function\s+on_(spawn|update)\s*\(");

            // No APIs prohibidas
            code.Should().NotContain("require(");
            code.Should().NotContain("io.");
            code.Should().NotContain("os.");
            code.Should().NotContain("debug.");
            code.Should().NotContain("loadstring");

        }
    }
}
