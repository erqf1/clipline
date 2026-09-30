#pragma once
#include <QString>
#include <QStringList>

// Übersetzung: Schlüssel ist der englische Text. 12 Sprachen wie in Cutline.
QStringList languageCodes();   // en de es fr it pt nl pl tr ru ja zh
QStringList languageNames();   // Eigennamen (English, Deutsch, Español, …)
void setLanguage(const QString& code);  // "" = Systemsprache
QString language();
QString L(const char* english);
