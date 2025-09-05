namespace GameProtogenAPI.Models
{
    public static class ChatModels
    {
        // Dejamos 'scene' como object para no atarte el backend a un DTO rígido.
        // En producción, podés tiparlo o usar JsonDocument/JsonElement para inspecciones más eficientes.
        public record ChatCommandRequest(string prompt, string? scene);

        // Opcional: estructura de la respuesta
        public record ChatOpsResponse(object ops);
    }
}
