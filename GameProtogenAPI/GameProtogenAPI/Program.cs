using Azure;
using Azure.AI.Inference;
using GameProtogenAPI.AI.AgentPlugins;
using GameProtogenAPI.AI.Orchestration;
using GameProtogenAPI.AI.Orchestration.Contracts;
using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using Microsoft.SemanticKernel;
using OpenAI.Images;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddControllers();
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

var openAIPlatformApiKey =
    builder.Configuration["OpenAI:ApiKey"] ??
    builder.Configuration["OPENAI_API_KEY"] ??
    Environment.GetEnvironmentVariable("OPENAI_API_KEY");

if (string.IsNullOrWhiteSpace(openAIPlatformApiKey))
    throw new InvalidOperationException("Falta OpenAI ApiKey (OpenAI:ApiKey o OPENAI_API_KEY).");

builder.Services.AddSingleton<ImageClient>(_ =>
    new ImageClient("gpt-image-1", openAIPlatformApiKey));

//builder.Services.AddSingleton(new ChatCompletionsClient(new Uri(azureAIInferenceEndpoint), new AzureKeyCredential(azureAIInferenceApiKey)));

builder.Services.AddSingleton(sp =>
{
    // Podés parametrizar el modelo desde config si querés
    var kernel = Kernel.CreateBuilder()
        .Build();

    // 2) Agregar plugins (Planner y Synthesizer)
    kernel.Plugins.AddFromType<PlannerPlugin>(serviceProvider: sp);
    kernel.Plugins.AddFromType<SynthesizerPlugin>(serviceProvider: sp);
    kernel.Plugins.AddFromType<GameDesignAdvisorPlugin>(serviceProvider: sp);
    kernel.Plugins.AddFromType<RouterPlugin>(serviceProvider: sp);
    kernel.Plugins.AddFromType<AssetGenPlugin>(serviceProvider: sp);
    kernel.Plugins.AddFromType<ScriptGenPlugin>(serviceProvider: sp);

    return kernel;
});

// 3) Orquestador SK
builder.Services.AddSingleton<ISkSceneEditOrchestrator, SkSceneEditOrchestrator>();

builder.Services.AddSingleton<IImageService, OpenAIImageGenService>();

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

app.MapGet("/health", () => Results.Ok(new { status = "Healthy" }));

//app.UseHttpsRedirection();

//app.UseAuthorization();

app.MapControllers();

app.Run();
