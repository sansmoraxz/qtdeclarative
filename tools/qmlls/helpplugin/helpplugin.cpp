// Copyright (C) 2026 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qqmllshelpplugininterface_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qobject.h>
#include <QtHelp/qhelpenginecore.h>
#include <QtHelp/qhelplink.h>

#include <memory>
#include <vector>

QT_BEGIN_NAMESPACE

class LocalQmllsHelpProvider final : public QQmlLSHelpProviderBase
{
public:
    explicit LocalQmllsHelpProvider(const QString &collectionFile, QObject *parent)
        : m_engine(std::make_unique<QHelpEngineCore>(collectionFile, parent))
    {
        const QFileInfo collectionInfo(collectionFile);
        QDir().mkpath(collectionInfo.absolutePath());
        m_engine->setReadOnly(false);
        if (!m_engine->setupData())
            m_error = m_engine->error();
    }

    bool registerDocumentation(const QString &documentationFileName) override
    {
        const QString namespaceName = QHelpEngineCore::namespaceName(documentationFileName);
        if (m_engine->registeredDocumentations().contains(namespaceName))
            return true;

        const bool ok = m_engine->registerDocumentation(documentationFileName);
        if (!ok)
            m_error = m_engine->error();
        return ok;
    }

    [[nodiscard]] QByteArray fileData(const QUrl &url) const override
    {
        return m_engine->fileData(url);
    }

    [[nodiscard]] std::vector<DocumentLink> documentsForIdentifier(const QString &id) const override
    {
        return toDocumentLinks(m_engine->documentsForIdentifier(id));
    }

    [[nodiscard]] std::vector<DocumentLink>
    documentsForIdentifier(const QString &id, const QString &filterName) const override
    {
        return toDocumentLinks(m_engine->documentsForIdentifier(id, filterName));
    }

    [[nodiscard]] std::vector<DocumentLink> documentsForKeyword(const QString &keyword) const override
    {
        return toDocumentLinks(m_engine->documentsForKeyword(keyword));
    }

    [[nodiscard]] std::vector<DocumentLink>
    documentsForKeyword(const QString &keyword, const QString &filterName) const override
    {
        return toDocumentLinks(m_engine->documentsForKeyword(keyword, filterName));
    }

    [[nodiscard]] QStringList registeredNamespaces() const override
    {
        return m_engine->registeredDocumentations();
    }

    [[nodiscard]] QString error() const override
    {
        return m_error.isEmpty() ? m_engine->error() : m_error;
    }

private:
    static std::vector<DocumentLink> toDocumentLinks(const QList<QHelpLink> &helpLinks)
    {
        std::vector<DocumentLink> links;
        links.reserve(helpLinks.size());
        for (const auto &link : helpLinks)
            links.push_back({ link.url, link.title });
        return links;
    }

    std::unique_ptr<QHelpEngineCore> m_engine;
    QString m_error;
};

class LocalQmllsHelpPlugin final : public QObject, public QQmlLSHelpPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlLSHelpPluginInterface_iid)
    Q_INTERFACES(QQmlLSHelpPluginInterface)

public:
    std::unique_ptr<QQmlLSHelpProviderBase> initialize(const QString &collectionFile,
                                                       QObject *parent) override
    {
        return std::make_unique<LocalQmllsHelpProvider>(collectionFile, parent);
    }
};

QT_END_NAMESPACE

#include "helpplugin.moc"
