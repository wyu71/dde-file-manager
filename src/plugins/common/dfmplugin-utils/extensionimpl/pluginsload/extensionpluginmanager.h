// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef EXTENSIONPLUGINMANAGER_H
#define EXTENSIONPLUGINMANAGER_H

#include "dfmplugin_utils_global.h"

#include "extensionpluginloader.h"

#include <QObject>

DPUTILS_BEGIN_NAMESPACE

class DFMExtMenuImplProxy;
class ExtensionPluginManagerPrivate;
class ExtensionPluginManager : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(ExtensionPluginManager)
    Q_DECLARE_PRIVATE(ExtensionPluginManager)

public:
    enum InitState {
        kReady,
        kScanned,
        kLoaded,
        kInitialized
    };

    static ExtensionPluginManager &instance();
    InitState currentState() const;
    QList<QSharedPointer<DFMEXT::DFMExtMenuPlugin>> menuPlugins() const;
    QList<QSharedPointer<DFMEXT::DFMExtEmblemIconPlugin>> emblemPlugins() const;
    DFMEXT::DFMExtMenuProxy *pluginMenuProxy() const;

Q_SIGNALS:
    void requestInitlaizePlugins();
    void allPluginsInitialized();

public Q_SLOTS:
    void onLoadingPlugins();

private:
    explicit ExtensionPluginManager(QObject *parent = nullptr);
    ~ExtensionPluginManager() override;

private:
    QScopedPointer<ExtensionPluginManagerPrivate> d_ptr;
};

DPUTILS_END_NAMESPACE

#endif   // EXTENSIONPLUGINMANAGER_H
