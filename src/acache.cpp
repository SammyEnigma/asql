/* 
 * SPDX-FileCopyrightText: (C) 2020 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */

#include "acache.h"
#include "adatabase.h"
#include "apool.h"
#include "aresult.h"

#include <QDateTime>
#include <QPointer>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(ASQL_CACHE, "asql.cache", QtInfoMsg)

struct ACacheReceiverCb {
    AResultFn cb;
    QPointer<QObject> receiver;
    QObject *checkReceiver = nullptr;
};

struct ACacheValue {
    QVariantList args;
    std::shared_ptr<QObject> cancellable;
    AResult result;
    std::vector<ACacheReceiverCb> receivers;
    qint64 hasResultTs = 0;
};

class ACachePrivate
{
public:
    QString poolName;
    ADatabase db;
    QMultiHash<QString, ACacheValue> cache;
    int dbSource = 0; // 0 - unset, 1 - db, 2 - pool
};

ACache::ACache(QObject *parent) : QObject(parent)
  , d_ptr(new ACachePrivate)
{

}

ACache::~ACache() = default;

void ACache::setDatabasePool(const QString &poolName)
{
    Q_D(ACache);
    d->poolName = poolName;
    d->db = ADatabase();
    d->dbSource = 2;
}

void ACache::setDatabase(const ADatabase &db)
{
    Q_D(ACache);
    d->poolName.clear();
    d->db = db;
    d->dbSource = 1;
}

bool ACache::clear(const QString &query, const QVariantList &params)
{
    Q_D(ACache);
    auto it = d->cache.constFind(query);
    while (it != d->cache.constEnd() && it.key() == query) {
        if (it.value().args == params) {
            d->cache.erase(it);
            return true;
        }
        ++it;
    }
//    qDebug(ASQL_CACHE) << "cleared" << ret << "cache entries" << query << params;
    return false;
}

bool ACache::expire(qint64 maxAgeMs, const QString &query, const QVariantList &params)
{
    Q_D(ACache);
    int ret = false;
    const qint64 cutAge = QDateTime::currentMSecsSinceEpoch() - maxAgeMs;
    auto it = d->cache.constFind(query);
    while (it != d->cache.constEnd() && it.key() == query) {
        const ACacheValue &value = *it;
        if (value.args == params) {
            if (value.hasResultTs && value.hasResultTs < cutAge) {
                ret = true;
                qDebug(ASQL_CACHE) << "clearing cache" << query << params;
                d->cache.erase(it);
            }
            break;
        }
        ++it;
    }
    return ret;
}

int ACache::expireAll(qint64 maxAgeMs)
{
    Q_D(ACache);
    int ret = 0;
    const qint64 cutAge = QDateTime::currentMSecsSinceEpoch() - maxAgeMs;
    auto it = d->cache.begin();
    while (it != d->cache.end()) {
        const ACacheValue &value = *it;
        if (value.hasResultTs && value.hasResultTs < cutAge) {
            it = d->cache.erase(it);
            ++ret;
        } else {
            ++it;
        }
    }
    return ret;
}

void ACache::exec(const QString &query, AResultFn cb, QObject *receiver)
{
    execExpiring(query, -1, QVariantList(), cb, receiver);
}

void ACache::exec(const QString &query, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    execExpiring(query, -1, params, cb, receiver);
}

void ACache::execExpiring(const QString &query, qint64 maxAgeMs, AResultFn cb, QObject *receiver)
{
    execExpiring(query, maxAgeMs, QVariantList(), cb, receiver);
}

void ACache::execExpiring(const QString &query, qint64 maxAgeMs, const QVariantList &params, AResultFn cb, QObject *receiver)
{
    Q_D(ACache);
    auto it = d->cache.find(query);
    while (it != d->cache.end() && it.key() == query) {
        auto &value = *it;
        if (value.args == params) {
            if (value.hasResultTs) {
                if (maxAgeMs != -1) {
                    const qint64 cutAge = QDateTime::currentMSecsSinceEpoch() - maxAgeMs;
                    if (value.hasResultTs < cutAge) {
                        d->cache.erase(it);
                        break;
                    } else {
                        qDebug(ASQL_CACHE) << "cached data ready" << query;
                        if (cb) {
                            cb(value.result);
                        }
                    }
                } else {
                    qDebug(ASQL_CACHE) << "cached data ready" << query;
                    if (cb) {
                        cb(value.result);
                    }
                }
            } else {
                qDebug(ASQL_CACHE) << "queuing request" << query;
                // queue another request
                ACacheReceiverCb receiverObj;
                receiverObj.cb = cb;
                receiverObj.receiver = receiver;
                receiverObj.checkReceiver = receiver;
                value.receivers.push_back(receiverObj);
            }

            return;
        }
        ++it;
    }

    qDebug(ASQL_CACHE) << "requesting data" << query;
    ACacheReceiverCb receiverObj;
    receiverObj.cb = cb;
    receiverObj.receiver = receiver;
    receiverObj.checkReceiver = receiver;

    auto cancellable = new QObject;

    ACacheValue _value;
    _value.args = params;
    _value.cancellable = std::make_shared<QObject>(cancellable);
    _value.receivers.push_back(receiverObj);
    d->cache.insert(query, _value);

    auto dbFn = [query, d, params] (AResult &result) {
        auto it = d->cache.find(query);
        while (it != d->cache.end() && it.key() == query) {
            ACacheValue &value = it.value();
            if (value.args == params) {
                value.result = result;
                value.hasResultTs = QDateTime::currentMSecsSinceEpoch();
                qDebug(ASQL_CACHE) << "got request data, dispatching to" << value.receivers.size() << "receivers" << query;
                for (const ACacheReceiverCb &receiverObj : value.receivers) {
                    if (receiverObj.checkReceiver == nullptr || !receiverObj.receiver.isNull()) {
                        qDebug(ASQL_CACHE) << "dispatching to receiver" << receiverObj.checkReceiver << query;
                        receiverObj.cb(result);
                    }
                }
                value.receivers.clear();
            }
            ++it;
        }
    };

    ADatabase db;
    if (d->dbSource == 1) {
        db = d->db;
    } else if (d->dbSource == 2) {
        db = APool::database(d->poolName);
    }

    if (params.isEmpty()) {
        db.exec(query, dbFn, cancellable);
    } else {
        db.exec(query, params, dbFn, cancellable);
    }
}

#include "moc_acache.cpp"
