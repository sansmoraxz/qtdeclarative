// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qqmllshelputils_p.h"

#include <QtQmlLS/private/qqmllsutils_p.h>
#include <QtCore/private/qfactoryloader_p.h>
#include <QtCore/qlibraryinfo.h>
#include <QtCore/qdiriterator.h>
#include <QtCore/qdir.h>
#include <QtQml/private/qqmlsignalnames_p.h>
#include <QtQmlCompiler/private/qqmljstyperesolver_p.h>
#include <optional>

QT_BEGIN_NAMESPACE

Q_STATIC_LOGGING_CATEGORY(QQmlLSHelpUtilsLog, "qt.languageserver.helpUtils")

using namespace QQmlJS::Dom;

static QStringList documentationFiles(const QString &qtInstallationPath)
{
    QStringList result;
    QDirIterator dirIterator(qtInstallationPath, QStringList{ "*.qch"_L1 }, QDir::Files);
    while (dirIterator.hasNext()) {
        const auto fileInfo = dirIterator.nextFileInfo();
        result << fileInfo.absoluteFilePath();
    }
    return result;
}

static QString markdownCodeBlock(const QString &code)
{
    return u"```qml\n%1\n```"_s.arg(code);
}

static QByteArray prependSignatureToDocumentation(const std::optional<QByteArray> &signature,
                                                  const std::optional<QByteArray> &documentation)
{
    if (!signature)
        return documentation.value_or(QByteArray{});
    if (!documentation)
        return *signature;

    QByteArray result = *signature;
    if (!result.endsWith('\n'))
        result.append('\n');
    result.append('\n');
    result.append(*documentation);
    return result;
}

static bool scopeDefinedInQmltypes(const QQmlJSScope::ConstPtr &scope)
{
    return !scope.isNull() && scope->filePath().endsWith(u".qmltypes"_s);
}

static QQmlJSScope::ConstPtr definingScopeForMethod(const QQmlJSScope::ConstPtr &scope,
                                                    const QString &name)
{
    QQmlJSScope::ConstPtr definingScope = scope;
    while (definingScope && !definingScope->hasOwnMethod(name))
        definingScope = definingScope->baseType();
    return definingScope;
}

static QQmlJSScope::ConstPtr definingScopeForProperty(const QQmlJSScope::ConstPtr &scope,
                                                      const QString &name)
{
    QQmlJSScope::ConstPtr definingScope = scope;
    while (definingScope && !definingScope->hasOwnProperty(name))
        definingScope = definingScope->baseType();
    return definingScope;
}

static QString metaParameterSignature(const QQmlJSMetaParameter &parameter)
{
    QString typeName = parameter.typeName();
    if (typeName.isEmpty() && parameter.type())
        typeName = parameter.type()->internalName();

    if (parameter.isList())
        typeName = u"list<%1>"_s.arg(typeName);
    else if (parameter.isPointer() && !typeName.endsWith(u'*'))
        typeName.append(u'*');

    if (parameter.name().isEmpty())
        return typeName;
    if (typeName.isEmpty())
        return parameter.name();
    return u"%1: %2"_s.arg(parameter.name(), typeName);
}

static QString metaMethodSignature(const QQmlJSMetaMethod &method)
{
    QStringList parameters;
    for (const auto &parameter : method.parameters())
        parameters.append(metaParameterSignature(parameter));

    QString signature = u"%1(%2)"_s.arg(method.methodName(), parameters.join(u", "_s));
    QString returnType = method.returnTypeName();
    if (returnType.isEmpty() && method.returnType())
        returnType = method.returnType()->internalName();
    if (!returnType.isEmpty())
        signature.append(u": "_s).append(returnType);

    return signature;
}

static QString metaPropertySignature(const QQmlJSMetaProperty &property)
{
    QString typeName = property.typeName();
    if (typeName.isEmpty() && property.type())
        typeName = property.type()->internalName();

    if (property.isList())
        typeName = u"list<%1>"_s.arg(typeName);
    else if (property.isPointer() && !typeName.endsWith(u'*'))
        typeName.append(u'*');

    QString signature;
    if (!property.isWritable())
        signature.append(u"readonly "_s);
    signature.append(u"property "_s);
    if (!typeName.isEmpty())
        signature.append(typeName).append(u' ');
    signature.append(property.propertyName());
    return signature;
}

static std::optional<QString> propertyNameFromExpression(const QQmlLSUtils::ExpressionType &expr)
{
    if (!expr.name)
        return std::nullopt;

    switch (expr.type) {
    case QQmlLSUtils::PropertyIdentifier:
    case QQmlLSUtils::GroupedPropertyIdentifier:
        return expr.name;
    case QQmlLSUtils::PropertyChangedSignalIdentifier:
        return QQmlSignalNames::changedSignalNameToPropertyName(*expr.name);
    case QQmlLSUtils::PropertyChangedHandlerIdentifier:
        return QQmlSignalNames::changedHandlerNameToPropertyName(*expr.name);
    default:
        return std::nullopt;
    }
}

HelpManager::HelpManager()
{
    const QFactoryLoader pluginLoader(QQmlLSHelpPluginInterface_iid, u"/help"_s);
    const auto keys = pluginLoader.metaDataKeys();
    for (qsizetype i = 0; i < keys.size(); ++i) {
        auto instance = qobject_cast<QQmlLSHelpPluginInterface *>(pluginLoader.instance(i));
        if (instance) {
            m_helpPlugin =
                    instance->initialize(QDir::tempPath() + "/collectionFile.qhc"_L1, nullptr);
            break;
        }
    }
}

void HelpManager::setDocumentationRootPath(const QString &path)
{
    if (m_docRootPath == path)
        return;
    m_docRootPath = path;

    const auto foundQchFiles = documentationFiles(path);
    if (foundQchFiles.isEmpty()) {
        qCWarning(QQmlLSHelpUtilsLog)
                << "No documentation files found in the Qt doc installation path: " << path;
        return;
    }

    return registerDocumentations(foundQchFiles);
}

QString HelpManager::documentationRootPath() const
{
    return m_docRootPath;
}

void HelpManager::registerDocumentations(const QStringList &docs) const
{
    if (!m_helpPlugin)
        return;
    std::for_each(docs.cbegin(), docs.cend(),
                  [this](const auto &file) { m_helpPlugin->registerDocumentation(file); });
}

std::optional<QByteArray> HelpManager::extractDocumentation(const DomItem &item) const
{
    if (item.internalKind() == DomType::ScriptIdentifierExpression) {
        const auto resolvedType =
                QQmlLSUtils::resolveExpressionType(item, QQmlLSUtils::ResolveOwnerType);
        if (!resolvedType)
            return std::nullopt;
        return extractDocumentationForIdentifiers(item, resolvedType.value());
    } else {
        return extractDocumentationForDomElements(item);
    }

    Q_UNREACHABLE_RETURN(std::nullopt);
}

std::optional<QByteArray>
HelpManager::extractDocumentationForIdentifiers(const DomItem &item,
                                                QQmlLSUtils::ExpressionType expr) const
{
    const auto links = collectDocumentationLinks(item, expr.semanticScope, expr.name.value_or(item.name()));
    switch (expr.type) {
    case QQmlLSUtils::QmlObjectIdIdentifier:
    case QQmlLSUtils::JavaScriptIdentifier:
    case QQmlLSUtils::GroupedPropertyIdentifier:
    case QQmlLSUtils::PropertyIdentifier: {
        const auto sourceDocumentation = sourceDocumentationForPropertyIdentifier(expr);
        if (!links.empty()) {
            ExtractDocumentation extractor(DomType::PropertyDefinition);
            if (const auto extracted = tryExtract(extractor, links, expr.name.value()))
                return prependSignatureToDocumentation(sourceDocumentation, extracted);
        }
        return sourceDocumentation;
    }
    case QQmlLSUtils::PropertyChangedSignalIdentifier:
    case QQmlLSUtils::PropertyChangedHandlerIdentifier: {
        if (const auto sourceDocumentation = sourceDocumentationForPropertyIdentifier(expr))
            return sourceDocumentation;
        [[fallthrough]];
    }
    case QQmlLSUtils::SignalIdentifier:
    case QQmlLSUtils::SignalHandlerIdentifier:
    case QQmlLSUtils::MethodIdentifier: {
        const auto sourceDocumentation = sourceDocumentationForMethodIdentifier(item, expr);
        if (!links.empty()) {
            ExtractDocumentation extractor(DomType::MethodInfo);
            if (const auto extracted = tryExtract(extractor, links, expr.name.value()))
                return prependSignatureToDocumentation(sourceDocumentation, extracted);
        }
        return sourceDocumentation;
    }
    case QQmlLSUtils::SingletonIdentifier:
    case QQmlLSUtils::AttachedTypeIdentifier:
    case QQmlLSUtils::QmlComponentIdentifier: {
        if (!(m_helpPlugin && !links.empty()))
            return std::nullopt;
        const auto &keyword = item.field(Fields::identifier).value().toString();
        // The keyword is a qmlobject. Keyword search should be sufficient.
        // TODO: Still there can be multiple qmlobject documentation, with
        // different Qt versions. We should pick the best one.
        ExtractDocumentation extractor(DomType::QmlObject);
        return tryExtract(extractor, m_helpPlugin->documentsForKeyword(keyword), keyword);
    }

    // Not implemented yet
    case QQmlLSUtils::EnumeratorIdentifier:
    case QQmlLSUtils::EnumeratorValueIdentifier:
    default:
        qCDebug(QQmlLSHelpUtilsLog)
                << "Documentation extraction for" << expr.name.value() << "was not implemented";
        return std::nullopt;
    }
    Q_UNREACHABLE_RETURN(std::nullopt);
}

std::optional<QByteArray> HelpManager::extractDocumentationForDomElements(const DomItem &item) const
{
    const auto qmlFile = item.containingFile().as<QmlFile>();
    if (!qmlFile)
        return std::nullopt;

    const auto name = item.field(Fields::name).value().toString();
    std::vector<QQmlLSHelpProviderBase::DocumentLink> links;
    switch (item.internalKind()) {
    case DomType::QmlObject: {
        links = collectDocumentationLinks(item, item.nearestSemanticScope(), name);
        break;
    }
    case DomType::PropertyDefinition: {
        links = collectDocumentationLinks(
                item, QQmlLSUtils::findDefiningScopeForProperty(item.nearestSemanticScope(), name),
                name);
        break;
    }
    case DomType::Binding: {
        links = collectDocumentationLinks(
                item, QQmlLSUtils::findDefiningScopeForBinding(item.nearestSemanticScope(), name),
                name);
        break;
    }
    case DomType::MethodInfo: {
        links = collectDocumentationLinks(
                item, QQmlLSUtils::findDefiningScopeForMethod(item.nearestSemanticScope(), name),
                name);
        break;
    }
    default:
        qCDebug(QQmlLSHelpUtilsLog)
                << item.internalKindStr() << "was not implemented for documentation extraction";
        return std::nullopt;
    }

    const auto sourceDocumentation =
            item.internalKind() == DomType::MethodInfo ? sourceDocumentationForMethod(item)
                                                       : std::nullopt;

    if (!links.empty()) {
        ExtractDocumentation extractor(item.internalKind());
        if (const auto extracted = tryExtract(extractor, links, name))
            return prependSignatureToDocumentation(sourceDocumentation, extracted);
    }

    return sourceDocumentation;
}

std::optional<QByteArray>
HelpManager::tryExtract(ExtractDocumentation &extractor,
                        const std::vector<QQmlLSHelpProviderBase::DocumentLink> &links,
                        const QString &name) const
{
    if (!m_helpPlugin)
        return std::nullopt;

    for (auto &&link : links) {
        const auto fileData = m_helpPlugin->fileData(link.url);
        if (fileData.isEmpty()) {
            qCDebug(QQmlLSHelpUtilsLog) << "No documentation found for" << link.url;
            continue;
        }
        const auto &documentation = extractor.execute(QString::fromUtf8(fileData), name,
                                                      HtmlExtractor::ExtractionMode::Simplified);
        if (documentation.isEmpty())
            continue;
        return documentation.toUtf8();
    }

    return std::nullopt;
}

std::optional<QByteArray>
HelpManager::documentationForItem(const DomItem &file, QLspSpecification::Position position)
{
    // Prepare Cpp types to Qml types mapping when documentation data is available.
    const bool hasDocumentationPlugin = m_helpPlugin && !m_helpPlugin->registeredNamespaces().empty();
    if (hasDocumentationPlugin) {
        const auto fileItem = file.containingFile().as<QmlFile>();
        if (fileItem) {
            const auto typeResolver = fileItem->typeResolver();
            if (typeResolver) {
                const auto &names = typeResolver->importedNames();
                for (auto &&[scope, qmlName] : names.asKeyValueRange()) {
                    auto sc = scope;
                    // in some situations, scope->internalName() could be the same
                    // as qmlName. In those cases, the key we are looking for is the
                    // first scope which is non-composite type.
                    // This is mostly the case for templated controls.
                    // Popup <-> Popup
                    // T.Popup <-> Popup
                    // QQuickPopup <-> Popup
                    if (sc && sc->internalName() == qmlName) {
                        while (sc && sc->isComposite())
                            sc = sc->baseType();
                    }
                    if (sc && !m_cppTypesToQmlTypes.contains(sc->internalName()))
                        m_cppTypesToQmlTypes.insert(sc->internalName(), qmlName);
                }
            }
        }
    }

    std::optional<QByteArray> result;
    const auto [line, character] = position;
    const auto itemLocations = QQmlLSUtils::itemsFromTextLocation(file, line, character);
    // Process found item's internalKind and fetch its documentation.
    for (const auto &entry : itemLocations) {
        result = extractDocumentation(entry.domItem);
        if (result.has_value())
            break;
    }

    return result;
}

std::optional<QByteArray>
HelpManager::sourceDocumentationForMethodIdentifier(const DomItem &item,
                                                    const QQmlLSUtils::ExpressionType &expr) const
{
    if (!expr.name || !expr.semanticScope)
        return std::nullopt;

    QString name = *expr.name;
    if (expr.type == QQmlLSUtils::SignalHandlerIdentifier) {
        const auto signalName = QQmlSignalNames::handlerNameToSignalName(name);
        if (!signalName)
            return std::nullopt;
        name = *signalName;
    }

    const auto definingScope = definingScopeForMethod(expr.semanticScope, name);
    if (!definingScope)
        return std::nullopt;

    const auto methods = definingScope->methods(name);
    if (scopeDefinedInQmltypes(definingScope) && !methods.isEmpty()) {
        QStringList signatures;
        for (const auto &method : methods)
            signatures.append(metaMethodSignature(method));
        signatures.removeDuplicates();
        return markdownCodeBlock(signatures.join(u'\n')).toUtf8();
    }

    const DomItem ownerFile = QQmlLSUtils::goToFileOfScope(item, definingScope);
    if (ownerFile) {
        const DomItem owner = QQmlLSUtils::sourceLocationToDomItem(
                                      ownerFile, definingScope->sourceLocation())
                                      .qmlObject();
        if (owner) {
            if (const auto sourceDocumentation =
                        sourceDocumentationForMethod(owner.field(Fields::methods).key(name).index(0))) {
                return sourceDocumentation;
            }
        }
    }

    if (methods.isEmpty())
        return std::nullopt;

    QStringList signatures;
    for (const auto &method : methods)
        signatures.append(metaMethodSignature(method));
    signatures.removeDuplicates();
    return markdownCodeBlock(signatures.join(u'\n')).toUtf8();
}

std::optional<QByteArray>
HelpManager::sourceDocumentationForPropertyIdentifier(
        const QQmlLSUtils::ExpressionType &expr) const
{
    if (!expr.semanticScope)
        return std::nullopt;

    const auto propertyName = propertyNameFromExpression(expr);
    if (!propertyName)
        return std::nullopt;

    const auto definingScope = definingScopeForProperty(expr.semanticScope, *propertyName);
    if (!definingScope)
        return std::nullopt;

    const auto property = definingScope->property(*propertyName);
    if (!property.isValid())
        return std::nullopt;

    return markdownCodeBlock(metaPropertySignature(property)).toUtf8();
}

std::optional<QByteArray>
HelpManager::sourceDocumentationForMethod(const DomItem &methodItem) const
{
    const auto *method = methodItem.as<MethodInfo>();
    if (!method)
        return std::nullopt;

    QString signature = method->signature(methodItem);
    if (!methodItem.name().isEmpty() && signature.startsWith(u'('))
        signature.prepend(methodItem.name());

    return markdownCodeBlock(signature).toUtf8();
}

/*
 * Returns the list of potential documentation links for the given item.
 * A keyword is not necessarily a unique name, so we need to find the scope where
 * the keyword is defined. If the item is a property, method or binding, it will
 * search for the defining scope and return the documentation links by looking at
 * the imported names. If the item is a QmlObject, it will return the documentation
 * links for qmlobject name.
 */
std::vector<QQmlLSHelpProviderBase::DocumentLink>
HelpManager::collectDocumentationLinks(const DomItem &item, const QQmlJSScope::ConstPtr &definingScope,
                                       const QString &name) const
{
    if (!(m_helpPlugin && definingScope))
        return {};
    const auto &qmlFile = item.containingFile().as<QmlFile>();
    if (!qmlFile)
        return {};
    const auto typeResolver = qmlFile->typeResolver();
    if (!typeResolver)
        return {};

    std::vector<QQmlLSHelpProviderBase::DocumentLink> links;
    const auto &foundScopeName = definingScope->internalName();
    if (m_cppTypesToQmlTypes.contains(foundScopeName)) {
        const QString id = m_cppTypesToQmlTypes.value(foundScopeName) + u"::"_s + name;
        links = m_helpPlugin->documentsForIdentifier(id);
        if (!links.empty())
            return links;
    }

    const auto &containingObjectName = item.qmlObject().name();
    auto scope = item.nearestSemanticScope();
    while (scope && scope->isComposite()) {
        const QString id = containingObjectName + u"::"_s + name;
        links = m_helpPlugin->documentsForIdentifier(id);
        if (!links.empty())
            return links;
        scope = scope->baseType();
    }

    while (scope && !m_cppTypesToQmlTypes.contains(scope->internalName())) {
        const QString id = m_cppTypesToQmlTypes.value(scope->internalName()) + u"::"_s + name;
        links = m_helpPlugin->documentsForIdentifier(id);
        if (!links.empty())
            return links;
        scope = scope->baseType();
    }

    return m_helpPlugin->documentsForKeyword(name);
}

QT_END_NAMESPACE
