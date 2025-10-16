using Azure;
using Azure.AI.OpenAI;
using GameProtogenAPI.AI.AgentPlugins;
using GameProtogenAPI.AI.Orchestration;
using GameProtogenAPI.AI.Orchestration.Contracts;
using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using Microsoft.AspNetCore.DataProtection.KeyManagement;
using Microsoft.SemanticKernel;
using OpenAI.Chat;
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

var azureAIEndpoint =
    builder.Configuration["AzureAI:Endpoint"] ??
    builder.Configuration["AZURE_OPENAI_ENDPOINT"] ??
    Environment.GetEnvironmentVariable("AZURE_OPENAI_ENDPOINT");

var azureAIApiKey =
    builder.Configuration["AzureAI:ApiKey"] ??
    builder.Configuration["AZURE_OPENAI_API_KEY"] ??
    Environment.GetEnvironmentVariable("AZURE_OPENAI_API_KEY");

if (string.IsNullOrWhiteSpace(azureAIEndpoint))
    throw new InvalidOperationException("Falta AzureAI Endpoint (AzureAI:Endpoint o AZURE_OPENAI_ENDPOINT).");
if (string.IsNullOrWhiteSpace(azureAIApiKey))
    throw new InvalidOperationException("Falta AzureAI ApiKey (AzureAI:ApiKey o AZURE_OPENAI_API_KEY).");
if (string.IsNullOrWhiteSpace(openAIPlatformApiKey))
    throw new InvalidOperationException("Falta OpenAI ApiKey (OpenAI:ApiKey o OPENAI_API_KEY).");

builder.Services.AddSingleton<ImageClient>(_ =>
    new ImageClient("gpt-image-1", openAIPlatformApiKey));

builder.Services.AddSingleton(new AzureOpenAIClient(new Uri(azureAIEndpoint), new AzureKeyCredential(azureAIApiKey)));

builder.Services.AddSingleton(sp =>
{
    var root = sp.GetRequiredService<AzureOpenAIClient>();
    return root.GetChatClient("grok-4-fast-reasoning");
});

builder.Services.AddSingleton(sp =>
{
    // Podés parametrizar el modelo desde config si querés
    var kernel = Kernel.CreateBuilder()
        .AddAzureOpenAIChatCompletion("grok-4-fast-reasoning", endpoint: azureAIEndpoint, azureAIApiKey)
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
