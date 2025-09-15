using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using Microsoft.AspNetCore.DataProtection.KeyManagement;
using OpenAI.Chat;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddControllers();
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

string? apiKey;

var useMock = builder.Configuration.GetValue("LLM:USE_MOCK", false);
if (useMock)
    builder.Services.AddSingleton<ILLMService, LLMServiceMock>();
else
{
    apiKey =
        builder.Configuration["OpenAI:ApiKey"] ??
        builder.Configuration["OPENAI_API_KEY"] ??         // fallback
        Environment.GetEnvironmentVariable("OPENAI_API_KEY");

    if (string.IsNullOrWhiteSpace(apiKey))
        throw new InvalidOperationException("Falta OpenAI ApiKey. Definí User Secret 'OpenAI:ApiKey' o var de entorno OPENAI__ApiKey / OPENAI_API_KEY.");

    builder.Services.AddSingleton<ILLMService, LLMService>();
    builder.Services.AddSingleton(new ChatClient("gpt-5-mini", apiKey));

}

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
