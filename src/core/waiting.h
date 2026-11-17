#pragma once

#include <QByteArray>
#include <QString>

namespace Kyu
{
// "The update is waiting for you": kyu-update writes what it waits on to
// the state directory's "waiting" file while pacman, sudo or Kyu itself
// asks a question, and removes it when the question is answered; the Kyu
// window says so when it is not in front.
QString waitingPath();
// The text, or none to remove the file.
void setWaiting(const QString &text);
QString waiting();

// The question a program is waiting on, from the end of what it printed
// (raw terminal output): its last line when that is a question ("[Y/n]",
// a trailing "?" or ":"), else nothing. A password prompt becomes
// "Your password is needed."; pacman's "::" goes.
QString waitingPrompt(const QByteArray &tail);
}
