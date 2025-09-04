using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using static GameProtogenAPI.Models.ChatModels;

namespace GameProtogenAPI.Controllers
{
    [Route("api/[controller]")]
    [ApiController]
    public class ChatController : ControllerBase
    {

        // POST /chat/command
        [HttpPost("command")]
        public IActionResult Command([FromBody] ChatCommandRequest req)
        {
            var ops = new List<object>();
            string p = (req?.prompt ?? string.Empty).ToLowerInvariant();

            if (p.Contains("plataforma") || p.Contains("platform") || p.Contains("box"))
            {
                ops.Add(new
                {
                    op = "spawn_box",
                    pos = new[] { 800f, 700f },
                    size = new[] { 300f, 40f },
                    colorHex = "#3C3C46FF"
                });
            }

            if (p.Contains("mover jugador") || p.Contains("move player"))
            {
                ops.Add(new
                {
                    op = "set_transform",
                    entity = 1u,
                    position = new[] { 640f, 480f },
                    scale = (float[]?)null,
                    rotation = (float?)null
                });
            }

            return Ok(new { ops });
        }
    }
}
