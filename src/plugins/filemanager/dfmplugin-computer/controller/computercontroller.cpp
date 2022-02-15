/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     xushitong<xushitong@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             zhangsheng<zhangsheng@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "computercontroller.h"
#include "events/computereventcaller.h"
#include "fileentity/appentryfileentity.h"
#include "fileentity/stashedprotocolentryfileentity.h"
#include "utils/computerutils.h"
#include "utils/stashmountsutils.h"
#include "watcher/computeritemwatcher.h"

#include "services/common/dialog/dialogservice.h"
#include "services/common/propertydialog/property_defines.h"
#include "dfm-base/base/application/application.h"
#include "dfm-base/base/application/settings.h"
#include "dfm-base/utils/devicemanager.h"
#include "dfm-base/file/entry/entryfileinfo.h"
#include "dfm-base/file/entry/entities/blockentryfileentity.h"
#include "dfm-base/dfm_event_defines.h"
#include "dfm-base/dbusservice/global_server_defines.h"

#include <dfm-framework/framework.h>

#include <QDebug>
#include <QApplication>
#include <QMenu>

DFMBASE_USE_NAMESPACE
DPCOMPUTER_USE_NAMESPACE
using namespace GlobalServerDefines;

ComputerController *ComputerController::instance()
{
    static ComputerController instance;
    return &instance;
}

void ComputerController::onOpenItem(quint64 winId, const QUrl &url)
{
    // TODO(xust) get the info from factory
    DFMEntryFileInfoPointer info(new EntryFileInfo(url));
    if (!info) {
        qDebug() << "cannot create info of " << url;
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        return;
    }

    DFMBASE_USE_NAMESPACE;
    QString suffix = info->suffix();
    if (!ComputerUtils::isPresetSuffix(suffix)) {
        ComputerEventCaller::sendOpenItem(info->url());
        return;
    }

    if (!info->isAccessable()) {
        qDebug() << "cannot access device: " << url;
        bool needAskForFormat = info->suffix() == SuffixInfo::kBlock
                && !info->extraProperty(DeviceProperty::kHasFileSystem).toBool()
                && !info->extraProperty(DeviceProperty::kIsEncrypted).toBool();
        if (needAskForFormat) {
            if (ComputerUtils::dlgServIns()->askForFormat())
                actFormat(winId, info);
        }
        QApplication::setOverrideCursor(Qt::ArrowCursor);
        return;
    }

    auto target = info->targetUrl();
    if (target.isValid()) {
        ComputerEventCaller::cdTo(winId, target);
    } else {
        if (suffix == SuffixInfo::kBlock) {
            mountDevice(winId, info);
        } else if (suffix == SuffixInfo::kProtocol) {
            // TODO(xust)
        } else if (suffix == SuffixInfo::kStashedRemote) {
            actMount(winId, info, true);
        } else if (suffix == SuffixInfo::kAppEntry) {
            QString cmd = info->extraProperty(ExtraPropertyName::kExecuteCommand).toString();
            QProcess::startDetached(cmd);
        }
    }
}

void ComputerController::onMenuRequest(quint64 winId, const QUrl &url, bool triggerFromSidebar)
{
    DFMEntryFileInfoPointer info(new EntryFileInfo(url));
    QMenu *menu = info->createMenu();
    if (menu) {
        connect(menu, &QMenu::triggered, [=](QAction *act) {
            QString actText = act->text();
            actionTriggered(info, winId, actText, triggerFromSidebar);
        });
        menu->exec(QCursor::pos());
        menu->deleteLater();
    }
}

void ComputerController::doRename(quint64 winId, const QUrl &url, const QString &name)
{
    Q_UNUSED(winId);

    DFMEntryFileInfoPointer info(new EntryFileInfo(url));
    bool removable = info->extraProperty(DeviceProperty::kRemovable).toBool();
    if (removable && info->suffix() == SuffixInfo::kBlock) {
        QString devId = ComputerUtils::getBlockDevIdByUrl(url);   // for now only block devices can be renamed.
        DeviceManagerInstance.invokeRenameBlockDevice(devId, name);
        return;
    }

    if (!removable) {
        doSetAlias(info, name);
    }
}

void ComputerController::doSetAlias(DFMEntryFileInfoPointer info, const QString &alias)
{
    QString uuid = info->extraProperty(DeviceProperty::kUUID).toString();
    if (uuid.isEmpty()) {
        qWarning() << "params exception!" << info->url();
        return;
    }

    using namespace AdditionalProperty;
    QString displayAlias = alias.trimmed();
    QString displayName = info->displayName();
    QVariantList list = Application::genericSetting()->value(kAliasGroupName, kAliasItemName).toList();

    // [a] empty alias  -> remove from list
    // [b] exists alias -> cover it
    // [c] not exists   -> append
    bool exists = false;
    for (int i = 0; i < list.count(); i++) {
        QVariantMap map = list.at(i).toMap();
        if (map.value(kAliasItemUUID).toString() == uuid) {
            if (displayAlias.isEmpty()) {   // [a]
                list.removeAt(i);
            } else {   // [b]
                map[kAliasItemName] = displayName;
                map[kAliasItemAlias] = displayAlias;
                list[i] = map;
            }
            exists = true;
            break;
        }
    }

    // [c]
    if (!exists && !displayAlias.isEmpty() && !uuid.isEmpty()) {
        QVariantMap map;
        map[kAliasItemUUID] = uuid;
        map[kAliasItemName] = displayName;
        map[kAliasItemAlias] = displayAlias;
        list.append(map);
        qInfo() << "append setting item: " << map;
    }

    Application::genericSetting()->setValue(kAliasGroupName, kAliasItemName, list);

    // update sidebar and computer display
    QString sidebarName = displayAlias.isEmpty() ? info->displayName() : displayAlias;
    ComputerUtils::sbServIns()->updateItem(info->url(), sidebarName, true);
    Q_EMIT updateItemAlias(info->url());
}

void ComputerController::mountDevice(quint64 winId, const DFMEntryFileInfoPointer info, ActionAfterMount act)
{
    if (!info) {
        qCritical() << "a null info pointer is transfered";
        return;
    }

    bool isEncrypted = info->extraProperty(DeviceProperty::kIsEncrypted).toBool();
    bool isUnlocked = info->extraProperty(DeviceProperty::kCleartextDevice).toString().length() > 1;
    QString shellId = ComputerUtils::getBlockDevIdByUrl(info->url());
    bool hasFileSystem = info->extraProperty(DeviceProperty::kHasFileSystem).toBool();

    bool needAskForFormat = info->suffix() == SuffixInfo::kBlock
            && !hasFileSystem
            && !isEncrypted;
    if (needAskForFormat) {
        if (ComputerUtils::dlgServIns()->askForFormat())
            actFormat(winId, info);
        return;
    }

    if (isEncrypted) {
        if (!isUnlocked) {
            QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
            QString passwd = ComputerUtils::dlgServIns()->askPasswordForLockedDevice();
            if (passwd.isEmpty()) {
                QApplication::setOverrideCursor(QCursor(Qt::ArrowCursor));
                return;
            }
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

            DeviceManagerInstance.unlockAndDo(shellId, passwd, [=](const QString &newID) {
                if (newID.isEmpty())
                    ComputerUtils::dlgServIns()->showErrorDialog(tr("Unlock device failed"), tr("Wrong password is inputed"));
                else
                    mountDevice(winId, newID, act);
            });
        } else {
            auto realDevId = info->extraProperty(DeviceProperty::kCleartextDevice).toString();
            mountDevice(winId, realDevId, act);
        }
    } else {
        mountDevice(winId, shellId, act);
    }
}

void ComputerController::mountDevice(quint64 winId, const QString &id, ActionAfterMount act)
{
    QFutureWatcher<QString> *watcher = new QFutureWatcher<QString>();
    connect(watcher, &QFutureWatcher<QString>::finished, [=] {
        QString path = watcher->result();
        QUrl u;
        u.setScheme(SchemeTypes::kFile);
        u.setPath(path);
        if (act == kEnterDirectory)
            ComputerEventCaller::cdTo(winId, path);
        else if (act == kEnterInNewWindow)
            ComputerEventCaller::sendEnterInNewWindow(u);
        else if (act == kEnterInNewTab)
            ComputerEventCaller::sendEnterInNewTab(winId, u);
        watcher->deleteLater();
    });
    QFuture<QString> fu = QtConcurrent::run(&DeviceManagerInstance, &DeviceManager::invokeMountBlockDevice, id);
    watcher->setFuture(fu);
    QApplication::setOverrideCursor(Qt::ArrowCursor);
}

void ComputerController::actionTriggered(DFMEntryFileInfoPointer info, quint64 winId, const QString &actionText, bool triggerFromSidebar)
{
    // if not original supported suffix, publish event to notify subscribers to handle
    QString sfx = info->suffix();
    if (!ComputerUtils::isPresetSuffix(sfx)) {
        ComputerEventCaller::sendContextActionTriggered(info->url(), actionText);
        return;
    }

    if (actionText == ContextMenuActionTrs::trOpenInNewWin())
        actOpenInNewWindow(winId, info);
    else if (actionText == ContextMenuActionTrs::trOpenInNewTab())
        actOpenInNewTab(winId, info);
    else if (actionText == ContextMenuActionTrs::trMount())
        actMount(winId, info);
    else if (actionText == ContextMenuActionTrs::trUnmount())
        actUnmount(info);
    else if (actionText == ContextMenuActionTrs::trRename())
        actRename(winId, info, triggerFromSidebar);
    else if (actionText == ContextMenuActionTrs::trFormat())
        actFormat(winId, info);
    else if (actionText == ContextMenuActionTrs::trSafelyRemove())
        actSafelyRemove(info);
    else if (actionText == ContextMenuActionTrs::trEject())
        actEject(info->url());
    else if (actionText == ContextMenuActionTrs::trProperties())
        actProperties(winId, info);
    else if (actionText == ContextMenuActionTrs::trOpen())
        onOpenItem(0, info->url());
    else if (actionText == ContextMenuActionTrs::trRemove())
        actRemove(info);
    else if (actionText == ContextMenuActionTrs::trLogoutAndClearSavedPasswd())
        actLogoutAndForgetPasswd(info);
}

void ComputerController::actEject(const QUrl &url)
{
    QString id;
    if (url.path().endsWith(SuffixInfo::kBlock)) {
        id = ComputerUtils::getBlockDevIdByUrl(url);
        DeviceManagerInstance.invokeDetachBlockDevice(id);
    } else if (url.path().endsWith(SuffixInfo::kProtocol)) {
        id = ComputerUtils::getProtocolDevIdByUrl(url);
        DeviceManagerInstance.invokeDetachProtocolDevice(id);
    } else {
        qDebug() << url << "is not support " << __FUNCTION__;
    }
}

void ComputerController::actOpenInNewWindow(quint64 winId, DFMEntryFileInfoPointer info)
{
    auto target = info->targetUrl();
    if (target.isValid())
        ComputerEventCaller::sendEnterInNewWindow(target);
    else
        mountDevice(winId, info, kEnterInNewWindow);
}

void ComputerController::actOpenInNewTab(quint64 winId, DFMEntryFileInfoPointer info)
{
    auto target = info->targetUrl();
    if (target.isValid())
        ComputerEventCaller::sendEnterInNewTab(winId, target);
    else
        mountDevice(winId, info, kEnterInNewTab);
}

static void onNetworkDeviceMountFinished(bool ok, dfmmount::DeviceError err, const QString &mntPath, quint64 winId, bool enterAfterMounted)
{
    if (ok) {
        if (enterAfterMounted)
            ComputerEventCaller::cdTo(winId, mntPath);
    } else {
        ComputerUtils::dlgServIns()->showErrorDialogWhenMountNetworkDeviceFailed(err);
    }
}

void ComputerController::actMount(quint64 winId, DFMEntryFileInfoPointer info, bool enterAfterMounted)
{
    QString sfx = info->suffix();
    if (sfx == SuffixInfo::kStashedRemote) {
        QString devId = ComputerUtils::getProtocolDevIdByStashedUrl(info->url());
        ComputerUtils::deviceServIns()->mountNetworkDevice(devId, [devId, enterAfterMounted, winId](bool ok, dfmmount::DeviceError err, const QString &mntPath) {
            onNetworkDeviceMountFinished(ok, err, mntPath, winId, enterAfterMounted);
        });
        return;
    } else if (sfx == SuffixInfo::kBlock) {
        mountDevice(0, info, kNone);
        return;
    } else if (sfx == SuffixInfo::kProtocol) {
        // TODO(xust)
        return;
    }
}

void ComputerController::actUnmount(DFMEntryFileInfoPointer info)
{
    QString devId;
    if (info->suffix() == SuffixInfo::kBlock) {
        devId = ComputerUtils::getBlockDevIdByUrl(info->url());
        DeviceManagerInstance.invokeUnmountBlockDevice(devId);
    } else if (info->suffix() == SuffixInfo::kProtocol) {
        devId = ComputerUtils::getProtocolDevIdByUrl(info->url());
        DeviceManagerInstance.invokeUnmountProtocolDevice(devId);
    } else {
        qDebug() << info->url() << "is not support " << __FUNCTION__;
    }
}

void ComputerController::actSafelyRemove(DFMEntryFileInfoPointer info)
{
    actEject(info->url());
}

void ComputerController::actRename(quint64 winId, DFMEntryFileInfoPointer info, bool triggerFromSidebar)
{
    if (!info) {
        qWarning() << "info is not valid!" << __FUNCTION__;
        return;
    }

    if (info->extraProperty(DeviceProperty::kRemovable).toBool() && info->targetUrl().isValid()) {
        qWarning() << "cannot rename a mounted device! " << __FUNCTION__;
        return;
    }

    if (!triggerFromSidebar)
        Q_EMIT requestRename(winId, info->url());
    else
        ComputerUtils::sbServIns()->triggerItemEdit(winId, info->url());
}

void ComputerController::actFormat(quint64 winId, DFMEntryFileInfoPointer info)
{
    if (info->suffix() != SuffixInfo::kBlock) {
        qWarning() << "non block device is not support format" << info->url();
        return;
    }
    auto url = info->url();
    QString devDesc = "/dev/" + url.path().remove("." + QString(SuffixInfo::kBlock));
    qDebug() << devDesc;

    QString cmd = "dde-device-formatter";
    QStringList args;
    args << "-m=" + QString::number(winId) << devDesc;

    QProcess::startDetached(cmd, args);
}

void ComputerController::actRemove(DFMEntryFileInfoPointer info)
{
    if (info->suffix() != SuffixInfo::kStashedRemote)
        return;
    StashMountsUtils::removeStashedMount(info->url());
    Q_EMIT ComputerItemWatcherInstance->itemRemoved(info->url());
}

void ComputerController::actProperties(quint64 winId, DFMEntryFileInfoPointer info)
{
    Q_UNUSED(winId);
    if (!info)
        return;

    if (info->suffix() == SuffixInfo::kUserDir) {
        ComputerUtils::propertyDlgServIns()->addFileProperty({ info->targetUrl() });
        return;
    }

    DSC_USE_NAMESPACE;
    DeviceInfo devInfo;
    devInfo.icon = info->fileIcon();
    devInfo.deviceUrl = info->url();
    devInfo.deviceName = info->displayName();
    devInfo.deviceType = "test";   // TODO(xust) complete the device type here.
    devInfo.fileSystem = info->extraProperty(DeviceProperty::kFileSystem).toString();
    devInfo.totalCapacity = info->sizeTotal();
    devInfo.availableSpace = info->sizeFree();

    ComputerUtils::propertyDlgServIns()->addDeviceProperty(devInfo);
}

void ComputerController::actLogoutAndForgetPasswd(DFMEntryFileInfoPointer info)
{
    // TODO(xust);
}

ComputerController::ComputerController(QObject *parent)
    : QObject(parent)
{
}