// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qqmlgotodefinitionsupport_p.h"
#include "qqmllsutils_p.h"
#include <QtQmlDom/private/qqmldomelements_p.h>
#include <QtLanguageServer/private/qlanguageserverspectypes_p.h>
#include <QtQmlDom/private/qqmldomexternalitems_p.h>
#include <QtQmlDom/private/qqmldomtop_p.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;
using namespace QQmlJS::Dom;

static std::optional<QQmlLSUtils::Location> locationAtStartOfFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    return QQmlLSUtils::Location::from(fileName, QString::fromUtf8(file.readAll()), 1, 1, 0);
}

static DomItem importDomItemFrom(const DomItem &item)
{
    if (item.internalKind() == DomType::Import)
        return item;
    if (item.directParent().internalKind() == DomType::Import)
        return item.directParent();
    return {};
}

static std::optional<QQmlLSUtils::Location> findImportDefinitionOf(const DomItem &item)
{
    const DomItem importItem = importDomItemFrom(item);
    if (!importItem)
        return {};

    const auto import = importItem.as<Import>();
    if (!import)
        return {};

    if (import->uri.isDirectory()) {
        const QString importPath = import->uri.absoluteLocalPath(
                QFileInfo(importItem.canonicalFilePath()).absolutePath());
        if (importPath.isEmpty())
            return {};

        const QFileInfo importInfo(importPath);
        if (importInfo.isFile())
            return locationAtStartOfFile(importInfo.canonicalFilePath());

        return locationAtStartOfFile(importInfo.filePath() + u"/qmldir"_s);
    }

    const auto env = importItem.environment().ownerAs<DomEnvironment>();
    if (!env)
        return {};

    const auto moduleIndex = env->moduleIndexWithUri(importItem.environment(), import->uri.moduleUri(),
                                                     import->version.majorVersion,
                                                     EnvLookup::Normal);
    if (!moduleIndex)
        return {};

    for (const auto &qmldirPath : moduleIndex->qmldirPaths()) {
        const DomItem qmldirFile = importItem.environment().path(qmldirPath);
        const QString fileName = qmldirFile.canonicalFilePath();
        if (fileName.isEmpty())
            continue;
        if (const auto location = locationAtStartOfFile(fileName))
            return location;
    }

    return {};
}

QmlGoToDefinitionSupport::QmlGoToDefinitionSupport(QmlLsp::QQmlCodeModel *codeModel)
    : BaseT(codeModel)
{
}

QString QmlGoToDefinitionSupport::name() const
{
    return u"QmlDefinitionSupport"_s;
}

void QmlGoToDefinitionSupport::setupCapabilities(
        const QLspSpecification::InitializeParams &,
        QLspSpecification::InitializeResult &serverCapabilities)
{
    // just assume serverCapabilities.capabilities.typeDefinitionProvider is a bool for now
    // handle the TypeDefinitionOptions and TypeDefinitionRegistrationOptions cases later on, if
    // needed (as they just allow more fancy go-to-type-definition action).
    serverCapabilities.capabilities.definitionProvider = true;
}

void QmlGoToDefinitionSupport::registerHandlers(QLanguageServer *,
                                                QLanguageServerProtocol *protocol)
{
    protocol->registerDefinitionRequestHandler(getRequestHandler());
}

void QmlGoToDefinitionSupport::process(RequestPointerArgument request)
{
    QList<QLspSpecification::Location> results;
    ResponseScopeGuard guard(results, request->m_response);

    auto itemsFound = itemsForRequest(request);

    if (guard.setErrorFrom(itemsFound))
        return;

    const auto &items = std::get<QList<QQmlLSUtils::ItemLocation>>(itemsFound);

    std::optional<QQmlLSUtils::Location> location;
    for (const auto &item : items) {
        location = QQmlLSUtils::findDefinitionOf(item.domItem);
        if (location)
            break;
    }
    if (!location) {
        for (const auto &item : items) {
            location = QQmlLSUtils::findTypeDefinitionOf(item.domItem);
            if (location)
                break;
        }
    }
    if (!location) {
        for (const auto &item : items) {
            location = findImportDefinitionOf(item.domItem);
            if (location)
                break;
        }
    }
    if (!location)
        return;

    QLspSpecification::Location l;
    l.uri = QUrl::fromLocalFile(location->filename()).toEncoded();
    l.range = QQmlLSUtils::qmlLocationToLspLocation(*location);

    results.append(l);
}
QT_END_NAMESPACE
