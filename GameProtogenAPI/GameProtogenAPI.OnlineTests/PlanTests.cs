using FluentAssertions;
using GameProtogenAPI.OnlineTests.Infra;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GameProtogenAPI.OnlineTests
{
    public class PlanTests
    {
        [Fact]
        public async Task BuildPlan_Returns_Valid_Plan_Xml()
        {
            var (llm, _) = ServiceFactory.CreateOnlineServices();
            var plan = await llm.BuildEditPlanXmlAsync(
                "Agregá una plataforma de color verde",
                SceneUtils.DefaultSceneJson,
                default);

            plan.Should().Contain("<plan").And.Contain("</plan>");
            System.Xml.Linq.XDocument.Parse(plan); // bien formado
        }
    }
}
