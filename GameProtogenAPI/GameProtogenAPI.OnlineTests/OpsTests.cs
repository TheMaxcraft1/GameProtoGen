using FluentAssertions;
using GameProtogenAPI.OnlineTests.Infra;
using GameProtogenAPI.Validators;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace GameProtogenAPI.OnlineTests
{
    public class OpsTests
    {
        [Fact]
        public async Task Plan_To_Ops_Produces_Valid_Ops()
        {
            var (llm, _) = ServiceFactory.CreateOnlineServices();
            var plan = await llm.BuildEditPlanXmlAsync(
                "Agregá dos plataformas de color verde (anchas, 32px alineadas)",
                SceneUtils.DefaultSceneJson,
                default);

            var opsJson = await llm.PlanToOpsJsonAsync(plan, default);
            using var doc = JsonDocument.Parse(opsJson);
            var root = doc.RootElement;

            root.TryGetProperty("ops", out var ops).Should().BeTrue();
            ops.ValueKind.Should().Be(JsonValueKind.Array);
            ops.GetArrayLength().Should().BeGreaterThan(0);

            OpsValidator.TryValidateOps(root, out var error).Should().BeTrue($"ops inválidas: {error}");
        }
    }
}
