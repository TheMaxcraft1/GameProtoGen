using FluentAssertions;
using GameProtogenAPI.OnlineTests.Infra;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GameProtogenAPI.OnlineTests
{
    public class DesignQATests
    {
        [Fact]
        public async Task AskDesign_Should_Return_Actionable_Text()
        {
            var (llm, _) = ServiceFactory.CreateOnlineServices();
            var text = await llm.AskDesignAsync("¿Rangos típicos de coyote time e input buffering?", default);

            text.Should().NotBeNullOrWhiteSpace();
        }
    }
}
