/*
 * SPDX-FileCopyrightText: (C) 2020-2023 Daniel Nicoletti <dantti12@gmail.com>
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <adatabase.h>
#include <adriverfactory.h>
#include <asql_export.h>

#include <QObject>
#include <QUrl>

namespace ASql {

using ADatabaseFn = std::function<void(ADatabase db)>;

class AExpectedDatabase;

class ASQL_EXPORT APool
{
public:
    static const QStringView defaultPool;

    /*!
     * \brief create creates a new database pool
     *
     * Creates a new connection Pool that uses the factory to create new connections when they are
     * required.
     *
     * \param factory is a driver factory that creates new connections
     * \param poolName is an identifier for such pools, for example "read-write" or
     * "read-only-replicas"
     */
    static void create(std::shared_ptr<ADriverFactory> factory, QStringView poolName = defaultPool);

    /*!
     * \brief create creates a new database pool
     *
     * Creates a new connection Pool that uses the factory to create new connections when they are
     * required.
     *
     * \param factory is a driver factory that creates new connections
     * \param poolName is an identifier for such pools, for example "read-write" or
     * "read-only-replicas"
     */
    static void create(std::shared_ptr<ADriverFactory> factory, const QString &poolName);

    /*!
     * \brief remove removes the database pool
     *
     * Removes the \p poolName from the connection pool, it doesn't remove or close current
     * connections.
     *
     * \param poolName
     */
    static void remove(QStringView poolName = defaultPool);

    /*!
     * \brief Returns the available pools
     */
    static QStringList pools();

    /*!
     * \brief database
     *
     * This method returns a new database object, unless an idle connection
     * (one that were previously dereferenced) is available on the pool.
     *
     * If the pool was not created or has reached it's maximum limit an invalid
     * database object is returned.
     *
     * \param poolName
     * \return ADatabase
     */
    static ADatabase database(QStringView poolName = defaultPool);

    /*!
     * \brief currentConnections of the pool
     * \param poolName
     * \return the number of active connections on this pool
     */
    static int currentConnections(QStringView poolName = defaultPool);

    /*!
     * \brief retrieves a database object
     *
     * This method is only useful if the pool has a limit of maximum connections allowed,
     * when the limit is reached instead of immediately returning a database object it will
     * queue the request and once a connection is freed the callback is issued.
     *
     * \param receiver
     * \param connectionName
     */
    static void database(QObject *receiver, ADatabaseFn cb, QStringView poolName = defaultPool);

    static AExpectedDatabase coDatabase(QObject *receiver    = nullptr,
                                        QStringView poolName = defaultPool);

    /*!
     * \brief setMaxIdleConnections maximum number of idle connections of the pool
     *
     * The default value is 1, so if 2 connections are created when they are returned the
     * second one will be deleted.
     *
     * \param max
     * \param poolName
     */
    static void setMaxIdleConnections(int max, QStringView poolName = defaultPool);

    /*!
     * \brief Returns maximum number of idle connections of the pool
     */
    static int maxIdleConnections(QStringView poolName = defaultPool);

    /*!
     * \brief setMaxConnections maximum number of connections of the pool
     *
     * The default value is 0, which means ilimited, if a limit is set the \sa database method
     * will start returning invalid objects untill the current number of connections is reduced.
     *
     * Changing this value only affect new connections created.
     *
     * \param max
     * \param poolName
     */
    static void setMaxConnections(int max, QStringView poolName = defaultPool);

    /*!
     * \brief Returns maximum number of connections of the pool
     */
    static int maxConnections(QStringView poolName = defaultPool);

    /*!
     * \brief setSetupCallback setup a connection before being used for the first time
     *
     * Sometimes one might want to increase connection buffer or set a different timezone,
     * any kind of connection setup that would be done as soon as the connection with the
     * database is estabilished.
     *
     * This callback is not called when the connection is reused.
     *
     * Always call \sa ADatabase::exec() at once so that they are queued and executed before
     * the caller of \sa APool::database().
     *
     * Changing this value only affect new connections created.
     *
     * \param max
     * \param poolName
     */
    static void setSetupCallback(ADatabaseFn cb, QStringView poolName = defaultPool);

    /*!
     * \brief setReuseCallback setup a connection before being reused
     *
     * Sometimes one might want to "DISCARD" previous information on the connection,
     * this callback will be called when an existing connection is going to be reused.
     *
     * This callback is not called when the connection is openned.
     *
     * Always call \sa ADatabase::exec() at once so that they are queued and executed before
     * the caller of \sa APool::database().
     *
     * Changing this value only affect new connections created.
     *
     * \param max
     * \param poolName
     */
    static void setReuseCallback(ADatabaseFn cb, QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedResult
        exec(QStringView query, QObject *receiver = nullptr, QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedResult exec(QUtf8StringView query,
                                              QObject *receiver    = nullptr,
                                              QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedMultiResult execMulti(QStringView query,
                                                        QObject *receiver    = nullptr,
                                                        QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedMultiResult execMulti(QUtf8StringView query,
                                                        QObject *receiver    = nullptr,
                                                        QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedResult exec(const APreparedQuery &query,
                                              QObject *receiver    = nullptr,
                                              QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedResult exec(QStringView query,
                                              const QVariantList &params,
                                              QObject *receiver    = nullptr,
                                              QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedResult exec(QUtf8StringView query,
                                              const QVariantList &params,
                                              QObject *receiver    = nullptr,
                                              QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedResult exec(const APreparedQuery &query,
                                              const QVariantList &params,
                                              QObject *receiver    = nullptr,
                                              QStringView poolName = defaultPool);

    [[nodiscard]] static AExpectedTransaction begin(QObject *receiver    = nullptr,
                                                    QStringView poolName = defaultPool);

private:
    inline static void pushDatabaseBack(QStringView connectionName, ADriver *driver);
};

} // namespace ASql
