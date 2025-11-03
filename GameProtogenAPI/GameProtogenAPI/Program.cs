using Azure;
using Azure.AI.Inference;
using GameProtogenAPI.AI.AgentPlugins;
using GameProtogenAPI.AI.Orchestration;
using GameProtogenAPI.AI.Orchestration.Contracts;
using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.IdentityModel.Tokens;
using Microsoft.SemanticKernel;
using OpenAI.Images;

var builder = WebApplication.CreateBuilder(args);

var audience =
    builder.Configuration["Auth:Audience"] ??
    builder.Configuration["AUTH_AUDIENCE"] ??
    Environment.GetEnvironmentVariable("AUTH_AUDIENCE");

builder.Services
    .AddAuthentication(JwtBearerDefaults.AuthenticationScheme)
    .AddJwtBearer(options =>
    {
        options.Authority = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/v2.0";
        options.Audience  = audience;
        // Opcional: validaciones extra
        options.TokenValidationParameters = new TokenValidationParameters
        {
            ValidateIssuer = true,
            ValidIssuer = "https://gameprotogenusers.ciamlogin.com/a9d06d78-e4d2-4909-93a7-e8fa6c09842f/v2.0",
            ValidateAudience = true,
            ValidAudience = audience,
            ValidateLifetime = true
        };
    });

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

app.UseAuthentication();
app.UseAuthorization();

app.MapControllers();

app.Run();
public partial class Program { }
