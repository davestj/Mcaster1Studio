#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>

namespace M1 {

/// AiTrackIntel — queries AI providers for artist profiles and discographies.
///
/// Supported providers (configured via QSettings):
///   - Ollama (local)
///   - Claude (Anthropic)
///   - ChatGPT (OpenAI)
///   - Grok (xAI)
///   - Gemini (Google)
///   - Venice (Venice.ai)
///
/// Usage:
///   lookupArtist("Pink Floyd")  →  [async]  →  profileReady(...) | lookupFailed(...)
class AiTrackIntel : public QObject {
    Q_OBJECT

public:
    explicit AiTrackIntel(QObject* parent = nullptr);

    /// Look up artist intel using the configured AI provider.
    /// Reads provider settings from QSettings (ai/provider, ai/*/apiKey, ai/*/model).
    void lookupArtist(const QString& artistName);

    /// Check if a lookup is in progress.
    bool isBusy() const { return m_pending > 0; }

    /// Send a custom system+user prompt pair using the configured AI provider.
    /// Uses the same provider/endpoint logic as lookupArtist() but with custom prompts.
    /// Results are emitted via customPromptReady / customPromptFailed with contextName
    /// as the identifier (so the caller can match responses to requests).
    void sendCustomPrompt(const QString& contextName, const QString& systemPrompt,
                          const QString& userPrompt);

signals:
    /// Emitted when AI returns a profile.
    void profileReady(const QString& artistName, const QString& profileText,
                      const QString& discographyJson, const QString& aiBackend,
                      const QString& aiModel);

    /// Emitted on error.
    void lookupFailed(const QString& artistName, const QString& error);

    /// Emitted when a custom prompt returns a response.
    void customPromptReady(const QString& contextName, const QString& text,
                           const QString& rawJson, const QString& aiBackend,
                           const QString& aiModel);

    /// Emitted when a custom prompt fails.
    void customPromptFailed(const QString& contextName, const QString& error);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    void sendOllamaRequest(const QString& artistName, const QString& url,
                           const QString& model);
    void sendOpenAiCompatRequest(const QString& artistName, const QString& apiKey,
                                  const QString& model, const QString& endpoint);
    void sendClaudeRequest(const QString& artistName, const QString& apiKey,
                           const QString& model);
    void sendGeminiRequest(const QString& artistName, const QString& apiKey,
                           const QString& model);

    // Custom prompt variants (system + user prompt instead of artist lookup prompt)
    void sendCustomOllama(const QString& contextName, const QString& systemPrompt,
                          const QString& userPrompt, const QString& url,
                          const QString& model);
    void sendCustomOpenAiCompat(const QString& contextName, const QString& systemPrompt,
                                const QString& userPrompt, const QString& apiKey,
                                const QString& model, const QString& endpoint);
    void sendCustomClaude(const QString& contextName, const QString& systemPrompt,
                          const QString& userPrompt, const QString& apiKey,
                          const QString& model);
    void sendCustomGemini(const QString& contextName, const QString& systemPrompt,
                          const QString& userPrompt, const QString& apiKey,
                          const QString& model);

    static QString buildPrompt(const QString& artistName);

    QNetworkAccessManager* m_nam = nullptr;
    int m_pending = 0;
};

} // namespace M1
