using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace GameProtogenAPI.OnlineTests.Infra
{
    public static class SceneUtils
    {
        public static string DefaultSceneJson => @"
            {
              ""entities"": [
                {
                  ""id"": 1,
                  ""Transform"": { ""position"": [320.0, 672.0], ""rotation"": 0.0, ""scale"": [1.0, 1.0] },
                  ""Sprite"": { ""size"": [80.0, 120.0], ""color"": { ""r"": 0, ""g"": 255, ""b"": 0, ""a"": 255 } },
                  ""Collider"": { ""halfExtents"": [40.0, 60.0], ""offset"": [0.0, 0.0] },
                  ""Physics2D"": { ""gravity"": 2000.0, ""gravityEnabled"": true, ""velocity"": [0.0, 0.0] },
                  ""PlayerController"": { ""moveSpeed"": 500.0, ""jumpSpeed"": 900.0 }
                },
                {
                  ""id"": 2,
                  ""Transform"": { ""position"": [800.0, 832.0], ""rotation"": 0.0, ""scale"": [1.0, 1.0] },
                  ""Sprite"": { ""size"": [1024.0, 32.0], ""color"": { ""r"": 120, ""g"": 120, ""b"": 120, ""a"": 255 } },
                  ""Collider"": { ""halfExtents"": [512.0, 16.0], ""offset"": [0.0, 0.0] }
                }
              ]
            }
        ";
    }
}
