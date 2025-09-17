using GameProtogenAPI.AI.AgentPlugins;
using GameProtogenAPI.AI.Orchestration;
using GameProtogenAPI.AI.Orchestration.Contracts;
using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using Microsoft.AspNetCore.DataProtection.KeyManagement;
using Microsoft.SemanticKernel;
using OpenAI.Chat;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddControllers();
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

var apiKey = builder.Configuration["OpenAI:ApiKey"] ??
        builder.Configuration["OPENAI_API_KEY"] ??         // fallback
        Environment.GetEnvironmentVariable("OPENAI_API_KEY");
if (string.IsNullOrWhiteSpace(apiKey))
    throw new InvalidOperationException("Falta OPENAI_API_KEY");

builder.Services.AddSingleton(sp =>
{
    // Podés parametrizar el modelo desde config si querés
    var kernel = Kernel.CreateBuilder()
        .AddOpenAIChatCompletion("gpt-5-mini", apiKey)   // router/LLM base
        .Build();

    // 2) Agregar plugins (Planner y Synthesizer)
    kernel.Plugins.AddFromType<PlannerPlugin>(serviceProvider: sp);
    kernel.Plugins.AddFromType<SynthesizerPlugin>(serviceProvider: sp);
    return kernel;
});

builder.Services.AddSingleton<ChatClient>(_ => new ChatClient("gpt-5-mini", apiKey));

// 3) Orquestador SK
builder.Services.AddSingleton<ISkSceneEditOrchestrator, SkSceneEditOrchestrator>();

// 4) Tu servicio de LLM (se usa adentro de los plugins)
var useMock = builder.Configuration.GetValue("LLM:USE_MOCK", false);
if (useMock)
    builder.Services.AddSingleton<ILLMService, LLMServiceMock>();
else
    builder.Services.AddSingleton<ILLMService, LLMService>();

var app = builder.Build();

// Configure the HTTP request pipeline.
if (app.Environment.IsDevelopment())
{
    app.UseSwagger();
    app.UseSwaggerUI();
}

//app.UseHttpsRedirection();

//app.UseAuthorization();

app.MapControllers();

app.Run("http://localhost:5199");
