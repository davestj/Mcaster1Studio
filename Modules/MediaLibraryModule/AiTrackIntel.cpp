#include "AiTrackIntel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>
#include <QDebug>

namespace M1 {

AiTrackIntel::AiTrackIntel(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &AiTrackIntel::onReplyFinished);
}

QString AiTrackIntel::buildPrompt(const QString& artistName) {
    return QString(
        "Provide a brief artist/band profile for '%1'. "
        "Include: biography (2-3 sentences), primary genre, "
        "notable albums (top 5 with year), similar artists (3-5). "
        "Format the response as plain text with sections."
    ).arg(artistName);
}

void AiTrackIntel::lookupArtist(const QString& artistName) {
    if (artistName.trimmed().isEmpty()) {
        emit lookupFailed(artistName, "Artist name is empty");
        return;
    }

    QSettings settings("Mcaster1", "Mcaster1Studio");
    const QString provider = settings.value("ai/provider", "ollama").toString().toLower();

    qInfo() << "[AiTrackIntel] Looking up artist:" << artistName
            << "via provider:" << provider;

    if (provider == "ollama") {
        const QString url   = settings.value("ai/ollama/url", "http://localhost:11434").toString();
        const QString model = settings.value("ai/ollama/model", "llama3").toString();
        sendOllamaRequest(artistName, url, model);
    }
    else if (provider == "claude") {
        const QString apiKey = settings.value("ai/claude/apiKey").toString();
        const QString model  = settings.value("ai/claude/model", "claude-sonnet-4-20250514").toString();
        if (apiKey.isEmpty()) {
            emit lookupFailed(artistName, "Claude API key not configured (ai/claude/apiKey)");
            return;
        }
        sendClaudeRequest(artistName, apiKey, model);
    }
    else if (provider == "chatgpt" || provider == "openai") {
        const QString apiKey = settings.value("ai/chatgpt/apiKey").toString();
        const QString model  = settings.value("ai/chatgpt/model", "gpt-4o").toString();
        if (apiKey.isEmpty()) {
            emit lookupFailed(artistName, "ChatGPT API key not configured (ai/chatgpt/apiKey)");
            return;
        }
        sendOpenAiCompatRequest(artistName, apiKey, model,
                                 "https://api.openai.com/v1/chat/completions");
    }
    else if (provider == "grok") {
        const QString apiKey = settings.value("ai/grok/apiKey").toString();
        const QString model  = settings.value("ai/grok/model", "grok-2").toString();
        if (apiKey.isEmpty()) {
            emit lookupFailed(artistName, "Grok API key not configured (ai/grok/apiKey)");
            return;
        }
        sendOpenAiCompatRequest(artistName, apiKey, model,
                                 "https://api.x.ai/v1/chat/completions");
    }
    else if (provider == "gemini") {
        const QString apiKey = settings.value("ai/gemini/apiKey").toString();
        const QString model  = settings.value("ai/gemini/model", "gemini-pro").toString();
        if (apiKey.isEmpty()) {
            emit lookupFailed(artistName, "Gemini API key not configured (ai/gemini/apiKey)");
            return;
        }
        sendGeminiRequest(artistName, apiKey, model);
    }
    else if (provider == "venice") {
        const QString apiKey = settings.value("ai/venice/apiKey").toString();
        const QString model  = settings.value("ai/venice/model", "llama-3.3-70b").toString();
        if (apiKey.isEmpty()) {
            emit lookupFailed(artistName, "Venice API key not configured (ai/venice/apiKey)");
            return;
        }
        sendOpenAiCompatRequest(artistName, apiKey, model,
                                 "https://api.venice.ai/api/v1/chat/completions");
    }
    else {
        emit lookupFailed(artistName,
            QString("Unknown AI provider: '%1'. "
                    "Supported: ollama, claude, chatgpt, grok, gemini, venice").arg(provider));
    }
}

// ---------------------------------------------------------------------------
// Ollama — POST {url}/api/generate
// ---------------------------------------------------------------------------
void AiTrackIntel::sendOllamaRequest(const QString& artistName,
                                      const QString& url,
                                      const QString& model)
{
    ++m_pending;

    QJsonObject body;
    body["model"]  = model;
    body["prompt"] = buildPrompt(artistName);
    body["stream"] = false;

    QNetworkRequest req(QUrl(url + "/api/generate"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName", artistName);
    reply->setProperty("aiBackend",  "ollama");
    reply->setProperty("aiModel",    model);
}

// ---------------------------------------------------------------------------
// Claude — POST https://api.anthropic.com/v1/messages
// ---------------------------------------------------------------------------
void AiTrackIntel::sendClaudeRequest(const QString& artistName,
                                      const QString& apiKey,
                                      const QString& model)
{
    ++m_pending;

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = buildPrompt(artistName);
    messages.append(userMsg);

    QJsonObject body;
    body["model"]      = model;
    body["max_tokens"] = 1024;
    body["messages"]   = messages;

    QNetworkRequest req(QUrl("https://api.anthropic.com/v1/messages"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", apiKey.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName", artistName);
    reply->setProperty("aiBackend",  "claude");
    reply->setProperty("aiModel",    model);
}

// ---------------------------------------------------------------------------
// OpenAI-compatible — ChatGPT, Grok, Venice
// ---------------------------------------------------------------------------
void AiTrackIntel::sendOpenAiCompatRequest(const QString& artistName,
                                            const QString& apiKey,
                                            const QString& model,
                                            const QString& endpoint)
{
    ++m_pending;

    QJsonArray messages;
    QJsonObject systemMsg;
    systemMsg["role"]    = "system";
    systemMsg["content"] = "You are a music encyclopedia. Provide concise, factual artist profiles.";
    messages.append(systemMsg);

    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = buildPrompt(artistName);
    messages.append(userMsg);

    QJsonObject body;
    body["model"]    = model;
    body["messages"] = messages;

    const QUrl epUrl(endpoint);
    QNetworkRequest req(epUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName", artistName);
    reply->setProperty("aiBackend",  epUrl.host());
    reply->setProperty("aiModel",    model);
}

// ---------------------------------------------------------------------------
// Gemini — POST https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={key}
// ---------------------------------------------------------------------------
void AiTrackIntel::sendGeminiRequest(const QString& artistName,
                                      const QString& apiKey,
                                      const QString& model)
{
    ++m_pending;

    QJsonObject textPart;
    textPart["text"] = buildPrompt(artistName);

    QJsonArray parts;
    parts.append(textPart);

    QJsonObject content;
    content["parts"] = parts;

    QJsonArray contents;
    contents.append(content);

    QJsonObject body;
    body["contents"] = contents;

    const QString url = QString(
        "https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2")
        .arg(model, apiKey);

    const QUrl geminiUrl(url);
    QNetworkRequest req(geminiUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName", artistName);
    reply->setProperty("aiBackend",  "gemini");
    reply->setProperty("aiModel",    model);
}

// ---------------------------------------------------------------------------
// Custom prompt — sends system + user prompt using the configured AI provider
// ---------------------------------------------------------------------------
void AiTrackIntel::sendCustomPrompt(const QString& contextName,
                                     const QString& systemPrompt,
                                     const QString& userPrompt)
{
    if (contextName.trimmed().isEmpty()) {
        emit customPromptFailed(contextName, "Context name is empty");
        return;
    }

    QSettings settings("Mcaster1", "Mcaster1Studio");
    const QString provider = settings.value("ai/provider", "ollama").toString().toLower();

    qInfo() << "[AiTrackIntel] Custom prompt:" << contextName
            << "via provider:" << provider;

    if (provider == "ollama") {
        const QString url   = settings.value("ai/ollama/url", "http://localhost:11434").toString();
        const QString model = settings.value("ai/ollama/model", "llama3").toString();
        sendCustomOllama(contextName, systemPrompt, userPrompt, url, model);
    }
    else if (provider == "claude") {
        const QString apiKey = settings.value("ai/claude/apiKey").toString();
        const QString model  = settings.value("ai/claude/model", "claude-sonnet-4-20250514").toString();
        if (apiKey.isEmpty()) {
            emit customPromptFailed(contextName, "Claude API key not configured");
            return;
        }
        sendCustomClaude(contextName, systemPrompt, userPrompt, apiKey, model);
    }
    else if (provider == "chatgpt" || provider == "openai") {
        const QString apiKey = settings.value("ai/chatgpt/apiKey").toString();
        const QString model  = settings.value("ai/chatgpt/model", "gpt-4o").toString();
        if (apiKey.isEmpty()) {
            emit customPromptFailed(contextName, "ChatGPT API key not configured");
            return;
        }
        sendCustomOpenAiCompat(contextName, systemPrompt, userPrompt, apiKey, model,
                                "https://api.openai.com/v1/chat/completions");
    }
    else if (provider == "grok") {
        const QString apiKey = settings.value("ai/grok/apiKey").toString();
        const QString model  = settings.value("ai/grok/model", "grok-2").toString();
        if (apiKey.isEmpty()) {
            emit customPromptFailed(contextName, "Grok API key not configured");
            return;
        }
        sendCustomOpenAiCompat(contextName, systemPrompt, userPrompt, apiKey, model,
                                "https://api.x.ai/v1/chat/completions");
    }
    else if (provider == "gemini") {
        const QString apiKey = settings.value("ai/gemini/apiKey").toString();
        const QString model  = settings.value("ai/gemini/model", "gemini-pro").toString();
        if (apiKey.isEmpty()) {
            emit customPromptFailed(contextName, "Gemini API key not configured");
            return;
        }
        sendCustomGemini(contextName, systemPrompt, userPrompt, apiKey, model);
    }
    else if (provider == "venice") {
        const QString apiKey = settings.value("ai/venice/apiKey").toString();
        const QString model  = settings.value("ai/venice/model", "llama-3.3-70b").toString();
        if (apiKey.isEmpty()) {
            emit customPromptFailed(contextName, "Venice API key not configured");
            return;
        }
        sendCustomOpenAiCompat(contextName, systemPrompt, userPrompt, apiKey, model,
                                "https://api.venice.ai/api/v1/chat/completions");
    }
    else {
        emit customPromptFailed(contextName,
            QString("Unknown AI provider: '%1'").arg(provider));
    }
}

// ---------------------------------------------------------------------------
// Custom prompt — Ollama variant
// ---------------------------------------------------------------------------
void AiTrackIntel::sendCustomOllama(const QString& contextName,
                                     const QString& systemPrompt,
                                     const QString& userPrompt,
                                     const QString& url,
                                     const QString& model)
{
    ++m_pending;

    // Ollama /api/generate uses a single "prompt" field; prepend system as context
    const QString combined = systemPrompt + "\n\n" + userPrompt;

    QJsonObject body;
    body["model"]  = model;
    body["prompt"] = combined;
    body["stream"] = false;

    QNetworkRequest req(QUrl(url + "/api/generate"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName",   contextName);
    reply->setProperty("aiBackend",    "ollama");
    reply->setProperty("aiModel",      model);
    reply->setProperty("customPrompt", true);
}

// ---------------------------------------------------------------------------
// Custom prompt — Claude variant
// ---------------------------------------------------------------------------
void AiTrackIntel::sendCustomClaude(const QString& contextName,
                                     const QString& systemPrompt,
                                     const QString& userPrompt,
                                     const QString& apiKey,
                                     const QString& model)
{
    ++m_pending;

    QJsonArray messages;
    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = userPrompt;
    messages.append(userMsg);

    QJsonObject body;
    body["model"]      = model;
    body["max_tokens"] = 4096;
    body["system"]     = systemPrompt;
    body["messages"]   = messages;

    QNetworkRequest req(QUrl("https://api.anthropic.com/v1/messages"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", apiKey.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName",   contextName);
    reply->setProperty("aiBackend",    "claude");
    reply->setProperty("aiModel",      model);
    reply->setProperty("customPrompt", true);
}

// ---------------------------------------------------------------------------
// Custom prompt — OpenAI-compatible (ChatGPT, Grok, Venice)
// ---------------------------------------------------------------------------
void AiTrackIntel::sendCustomOpenAiCompat(const QString& contextName,
                                            const QString& systemPrompt,
                                            const QString& userPrompt,
                                            const QString& apiKey,
                                            const QString& model,
                                            const QString& endpoint)
{
    ++m_pending;

    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg["role"]    = "system";
    sysMsg["content"] = systemPrompt;
    messages.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = userPrompt;
    messages.append(userMsg);

    QJsonObject body;
    body["model"]    = model;
    body["messages"] = messages;

    const QUrl epUrl(endpoint);
    QNetworkRequest req(epUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName",   contextName);
    reply->setProperty("aiBackend",    epUrl.host());
    reply->setProperty("aiModel",      model);
    reply->setProperty("customPrompt", true);
}

// ---------------------------------------------------------------------------
// Custom prompt — Gemini variant
// ---------------------------------------------------------------------------
void AiTrackIntel::sendCustomGemini(const QString& contextName,
                                     const QString& systemPrompt,
                                     const QString& userPrompt,
                                     const QString& apiKey,
                                     const QString& model)
{
    ++m_pending;

    // Gemini: use systemInstruction + contents
    QJsonObject sysTextPart;
    sysTextPart["text"] = systemPrompt;
    QJsonArray sysParts;
    sysParts.append(sysTextPart);
    QJsonObject systemInstruction;
    systemInstruction["parts"] = sysParts;

    QJsonObject userTextPart;
    userTextPart["text"] = userPrompt;
    QJsonArray userParts;
    userParts.append(userTextPart);
    QJsonObject content;
    content["parts"] = userParts;
    QJsonArray contents;
    contents.append(content);

    QJsonObject body;
    body["system_instruction"] = systemInstruction;
    body["contents"] = contents;

    const QString url = QString(
        "https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent?key=%2")
        .arg(model, apiKey);

    const QUrl gemUrl(url);
    QNetworkRequest req(gemUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "Mcaster1Studio/1.0.0");

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson());
    reply->setProperty("artistName",   contextName);
    reply->setProperty("aiBackend",    "gemini");
    reply->setProperty("aiModel",      model);
    reply->setProperty("customPrompt", true);
}

// ---------------------------------------------------------------------------
// Response handler — parses all provider formats
// ---------------------------------------------------------------------------
void AiTrackIntel::onReplyFinished(QNetworkReply* reply) {
    reply->deleteLater();
    --m_pending;

    const QString artistName   = reply->property("artistName").toString();
    const QString aiBackend    = reply->property("aiBackend").toString();
    const QString aiModel      = reply->property("aiModel").toString();
    const bool    isCustom     = reply->property("customPrompt").toBool();

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = QString("Network error: %1 (HTTP %2)")
            .arg(reply->errorString())
            .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        qWarning() << "[AiTrackIntel]" << err;
        if (isCustom)
            emit customPromptFailed(artistName, err);
        else
            emit lookupFailed(artistName, err);
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull()) {
        if (isCustom)
            emit customPromptFailed(artistName, "Invalid JSON response from AI provider");
        else
            emit lookupFailed(artistName, "Invalid JSON response from AI provider");
        return;
    }

    const QJsonObject root = doc.object();
    QString profileText;

    // --- Ollama response: {"response": "..."} ---
    if (root.contains("response")) {
        profileText = root.value("response").toString();
    }
    // --- Claude response: {"content": [{"type":"text","text":"..."}]} ---
    else if (root.contains("content")) {
        const QJsonArray contentArr = root.value("content").toArray();
        for (const QJsonValue& v : contentArr) {
            const QJsonObject block = v.toObject();
            if (block.value("type").toString() == "text") {
                profileText += block.value("text").toString();
            }
        }
    }
    // --- OpenAI-compatible: {"choices": [{"message":{"content":"..."}}]} ---
    else if (root.contains("choices")) {
        const QJsonArray choices = root.value("choices").toArray();
        if (!choices.isEmpty()) {
            profileText = choices.first().toObject()
                              .value("message").toObject()
                              .value("content").toString();
        }
    }
    // --- Gemini response: {"candidates": [{"content":{"parts":[{"text":"..."}]}}]} ---
    else if (root.contains("candidates")) {
        const QJsonArray candidates = root.value("candidates").toArray();
        if (!candidates.isEmpty()) {
            const QJsonArray parts = candidates.first().toObject()
                                        .value("content").toObject()
                                        .value("parts").toArray();
            for (const QJsonValue& v : parts) {
                profileText += v.toObject().value("text").toString();
            }
        }
    }
    // --- Error response from provider ---
    else if (root.contains("error")) {
        const QJsonObject errObj = root.value("error").toObject();
        const QString errMsg = errObj.value("message").toString(
            errObj.value("type").toString("Unknown provider error"));
        if (isCustom)
            emit customPromptFailed(artistName, errMsg);
        else
            emit lookupFailed(artistName, errMsg);
        return;
    }
    else {
        if (isCustom)
            emit customPromptFailed(artistName, "Unrecognized response format from AI provider");
        else
            emit lookupFailed(artistName, "Unrecognized response format from AI provider");
        return;
    }

    if (profileText.trimmed().isEmpty()) {
        if (isCustom)
            emit customPromptFailed(artistName, "AI returned empty response");
        else
            emit lookupFailed(artistName, "AI returned empty response");
        return;
    }

    const QString rawJson = QJsonDocument(root).toJson(QJsonDocument::Compact);

    qInfo() << "[AiTrackIntel]" << (isCustom ? "Custom prompt" : "Profile")
            << "received for" << artistName
            << "via" << aiBackend << "/" << aiModel
            << "(" << profileText.size() << "chars)";

    if (isCustom)
        emit customPromptReady(artistName, profileText, rawJson, aiBackend, aiModel);
    else
        emit profileReady(artistName, profileText, rawJson, aiBackend, aiModel);
}

} // namespace M1
