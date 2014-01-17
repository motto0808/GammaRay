/*
  debuggerinjector.h

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GAMMARAY_DEBUGGERINJECTOR_H
#define GAMMARAY_DEBUGGERINJECTOR_H

#include "injector/abstractinjector.h"

#include <QObject>
#include <QProcess>

namespace GammaRay {

/** Base class for debugger-based injectors. */
class DebuggerInjector : public QObject, public AbstractInjector
{
  Q_OBJECT
  public:
    DebuggerInjector();
    ~DebuggerInjector();

    bool selfTest();

    QString errorString();
    int exitCode();
    QProcess::ExitStatus exitStatus();
    QProcess::ProcessError processError();

  protected:
    virtual QString debuggerExecutable() const = 0;

    bool startDebugger(const QStringList &args);

  protected slots:
    virtual void readyReadStandardError();
    virtual void readyReadStandardOutput();

  protected:
    QScopedPointer<QProcess> m_process;
    int mExitCode;
    QProcess::ProcessError mProcessError;
    QProcess::ExitStatus mExitStatus;
    QString mErrorString;
    bool mManualError;
};

}

#endif // GAMMARAY_DEBUGGERINJECTOR_H