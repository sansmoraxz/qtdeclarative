// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qqmlfindusagessupport_p.h"
#include "qqmllsutils_p.h"
#include <QtLanguageServer/private/qlanguageserverspectypes_p.h>
#include <QtQmlDom/private/qqmldomexternalitems_p.h>
#include <QtQmlDom/private/qqmldomtop_p.h>
#include <variant>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

QQmlFindUsagesSupport::QQmlFindUsagesSupport(QmlLsp::QQmlCodeModel *codeModel)
    : BaseT(codeModel) { }

QString QQmlFindUsagesSupport::name() const
{
    return u"QmlFindUsagesSupport"_s;
}

void QQmlFindUsagesSupport::setupCapabilities(
        const QLspSpecification::InitializeParams &,
        QLspSpecification::InitializeResult &serverCapabilities)
{
    // just assume serverCapabilities.capabilities.typeDefinitionProvider is a bool for now
    // handle the ReferenceOptions later if needed (it adds the possibility to communicate the
    // current progress).
    serverCapabilities.capabilities.referencesProvider = true;
}

void QQmlFindUsagesSupport::registerHandlers(QLanguageServer *, QLanguageServerProtocol *protocol)
{
    protocol->registerReferenceRequestHandler(getRequestHandler());
}

void QQmlFindUsagesSupport::process(QQmlFindUsagesSupport::RequestPointerArgument request)
{
    QList<QLspSpecification::Location> results;
    ResponseScopeGuard guard(results, request->m_response);

    const auto doc = m_codeModel->openDocumentByUrl(
            QQmlLSUtils::lspUriToQmlUrl(request->m_parameters.textDocument.uri));
    if (!doc.snapshot.validDocVersion || doc.snapshot.validDocVersion != doc.snapshot.docVersion) {
        guard.setError({
                int(QLspSpecification::ErrorCodes::RequestCancelled),
                u"Cannot proceed: current QML document is invalid! Fix all the errors in your "
                u"QML code and try again."_s,
        });
        return;
    }

    const QString filePath = doc.snapshot.validDoc.canonicalFilePath();
    QQmlJS::Dom::DomItem file = m_codeModel->validEnv()
                                        .field(QQmlJS::Dom::Fields::qmlFileWithPath)
                                        .key(filePath)
                                        .field(QQmlJS::Dom::Fields::currentItem);
    if (!file)
        file = doc.snapshot.validDoc.fileObject(QQmlJS::Dom::GoTo::MostLikely);
    if (auto envPtr = file.environment().ownerAs<QQmlJS::Dom::DomEnvironment>())
        envPtr->clearReferenceCache();
    if (!file) {
        guard.setError({
                0,
                u"Could not find file %1 in project."_s.arg(doc.snapshot.doc.toString()),
        });
        return;
    }

    auto items = QQmlLSUtils::itemsFromTextLocation(
            file, request->m_parameters.position.line, request->m_parameters.position.character);
    if (items.isEmpty()) {
        guard.setError({
                0,
                u"Could not find any items at given text location."_s,
        });
        return;
    }

    QQmlLSUtils::ItemLocation &front = items.front();

    auto usages = QQmlLSUtils::findUsagesOf(front.domItem);

    // note: ignore usages in filenames here as that is not supported by the protocol.
    for (const auto &usage : usages.usagesInFile()) {
        QLspSpecification::Location location;
        location.uri = QUrl::fromLocalFile(usage.filename()).toEncoded();
        location.range = QQmlLSUtils::qmlLocationToLspLocation(usage);

        results.append(location);
    }
}
QT_END_NAMESPACE
