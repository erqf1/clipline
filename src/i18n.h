#pragma once
#include <QString>

// Übersetzung: Schlüssel ist der englische Text. Unterstützt Englisch und Deutsch.
void setLanguage(const QString& code);  // "", "en", "de" – leer = Systemsprache
QString language();
QString L(const char* english);
