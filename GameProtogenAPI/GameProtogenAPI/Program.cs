using GameProtogenAPI.Services;
using GameProtogenAPI.Services.Contracts;
using OpenAI.Chat;

var builder = WebApplication.CreateBuilder(args);

string? apiKey =
    builder.Configuration["OpenAI:ApiKey"] ??
    builder.Configuration["OPENAI_API_KEY"] ??         // fallback
    Environment.GetEnvironmentVariable("OPENAI_API_KEY");

if (string.IsNullOrWhiteSpace(apiKey))
    throw new InvalidOperationException("Falta OpenAI ApiKey. Definí User Secret 'OpenAI:ApiKey' o var de entorno OPENAI__ApiKey / OPENAI_API_KEY.");

builder.Services.AddSingleton(new ChatClient("gpt-5-nano", apiKey));
builder.Services.AddSingleton(new ChatClient("gpt-5-mini", apiKey));


builder.Services.AddControllers();
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

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
