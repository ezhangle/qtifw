/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kdjob.h"

#include <QtCore/QDebug>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>


// -- KDJob::Private

class KDJob::Private
{
    KDJob *const q;

public:
    explicit Private(KDJob *qq)
        : q(qq)
        , error(KDJob::NoError)
        , caps(KDJob::NoCapabilities)
        , autoDelete(true)
        , totalAmount(100)
        , processedAmount(0)
        , m_timeout(-1)
    {
        connect(&m_timer, SIGNAL(timeout()), q, SLOT(cancel()));
    }

    ~Private()
    {
        m_timer.stop();
    }

    void delayedStart()
    {
        q->doStart();
        emit q->started(q);
    }

    void waitForSignal(const char *sig)
    {
        QEventLoop loop;
        q->connect(q, sig, &loop, SLOT(quit()));

        if (m_timeout >= 0)
            m_timer.start(m_timeout);
        else
            m_timer.stop();

        loop.exec();
    }

    int error;
    QString errorString;
    KDJob::Capabilities caps;
    bool autoDelete;
    quint64 totalAmount;
    quint64 processedAmount;
    int m_timeout;
    QTimer m_timer;
};


// -- KDJob

KDJob::KDJob(QObject *parent)
    : QObject(parent),
      d(new Private(this))
{
    connect(this, SIGNAL(finished(KDJob*)), this, SLOT(onFinished()));
}

KDJob::~KDJob()
{
    delete d;
}

bool KDJob::autoDelete() const
{
    return d->autoDelete;
}

void KDJob::setAutoDelete(bool autoDelete)
{
    d->autoDelete = autoDelete;
}

int KDJob::error() const
{
    return d->error;
}

QString KDJob::errorString() const
{
    return d->errorString;
}

void KDJob::emitFinished()
{
    emit finished(this);
}

void KDJob::emitFinishedWithError(int error, const QString &errorString)
{
    d->error = error;
    d->errorString = errorString;
    emitFinished();
}

void KDJob::setError(int error)
{
    d->error = error;
}

void KDJob::setErrorString(const QString &errorString)
{
    d->errorString = errorString;
}

void KDJob::waitForStarted()
{
    d->waitForSignal(SIGNAL(started(KDJob*)));
}

void KDJob::waitForFinished()
{
    d->waitForSignal(SIGNAL(finished(KDJob*)));
}

KDJob::Capabilities KDJob::capabilities() const
{
    return d->caps;
}

bool KDJob::hasCapability(Capability c) const
{
    return d->caps.testFlag(c);
}

void KDJob::setCapabilities(Capabilities c)
{
    d->caps = c;
}

void KDJob::start()
{
    QMetaObject::invokeMethod(this, "delayedStart", Qt::QueuedConnection);
}

void KDJob::cancel()
{
    if (d->caps & Cancelable) {
        doCancel();
        if (error() == NoError) {
            setError(Canceled);
            setErrorString(tr("Canceled"));
        }
        emitFinished();
    } else {
        qDebug() << "The current job can not be canceled, missing \"Cancelable\" capability!";
    }
}

quint64 KDJob::totalAmount() const
{
    return d->totalAmount;
}

quint64 KDJob::processedAmount() const
{
    return d->processedAmount;
}

void KDJob::setTotalAmount(quint64 amount)
{
    if (d->totalAmount == amount)
        return;
    d->totalAmount = amount;
    emit progress(this, d->processedAmount, d->totalAmount);
}

/*!
    Returns the timeout in milliseconds before the jobs cancel slot gets triggered. A return value of -1
    means there is currently no timeout used for the job.
*/
int KDJob::timeout() const
{
    return d->m_timeout;
}

/*!
    Sets the timeout in \a milliseconds before the jobs cancel slot gets triggered. \note Only jobs that
    have the \bold KDJob::Cancelable capability can be canceled by an timeout. A value of -1 will stop the
    timeout mechanism.
*/
void KDJob::setTimeout(int milliseconds)
{
    d->m_timeout = milliseconds;
}

void KDJob::setProcessedAmount(quint64 amount)
{
    if (d->processedAmount == amount)
        return;
    d->processedAmount = amount;
    emit progress(this, d->processedAmount, d->totalAmount);
}

void KDJob::onFinished()
{
    d->m_timer.stop();
    if (d->autoDelete)
        deleteLater();
}

#include "moc_kdjob.cpp"
