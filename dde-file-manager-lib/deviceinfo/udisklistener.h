/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
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

#ifndef UDISKLISTENER_H
#define UDISKLISTENER_H

#include "dabstractfilecontroller.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>
#include <QDBusObjectPath>
#include <QList>
#include <QMap>
#include <QDBusArgument>
#include <QTimer>
#include <QXmlStreamReader>
#include <QDBusPendingReply>
#include "udiskdeviceinfo.h"


#define EventTypeVolumeAdded 1
#define EventTypeVolumeRemoved 2
#define EventTypeMountAdded 3
#define EventTypeMountRemoved 4

class UDiskDeviceInfo;
class Subscriber;

class DDiskManager;
class DBlockDevice;

class UDiskListener : public DAbstractFileController
{
    Q_OBJECT

public:
    explicit UDiskListener(QObject *parent = nullptr);
    void initDiskManager();
    void initConnect();
    UDiskDeviceInfoPointer getDevice(const QString &id);
    void addDevice(UDiskDeviceInfoPointer device);
    void removeDevice(UDiskDeviceInfoPointer device);
    bool renameFile(const QSharedPointer<DFMRenameEvent> &event) const override;

    QString lastPart(const QString &path);
    UDiskDeviceInfoPointer hasDeviceInfo(const QString &id);

    void addSubscriber(Subscriber *sub);
    void removeSubscriber(Subscriber *sub);

    QMap<QString, UDiskDeviceInfoPointer> getAllDeviceInfos();
    QList<UDiskDeviceInfoPointer> getDeviceList();
    QList<UDiskDeviceInfoPointer> getMountList();

    bool isDeviceFolder(const QString &path) const;
    bool isInDeviceFolder(const QString &path) const;
    bool isInRemovableDeviceFolder(const QString &path) const;

    const QList<DAbstractFileInfoPointer> getChildren(const QSharedPointer<DFMGetChildrensEvent> &event) const override;
    const DAbstractFileInfoPointer createFileInfo(const QSharedPointer<DFMCreateFileInfoEvent> &event) const override;
    DAbstractFileWatcher *createFileWatcher(const QSharedPointer<DFMCreateFileWatcherEvent> &event) const override;

    UDiskDeviceInfoPointer getDeviceByDevicePath(const QString &deveicePath);
    UDiskDeviceInfoPointer getDeviceByMountPoint(const QString &mountPoint);
    UDiskDeviceInfoPointer getDeviceByMountPointFilePath(const QString &filePath);
    UDiskDeviceInfoPointer getDeviceByPath(const QString &path);
    UDiskDeviceInfoPointer getDeviceByFilePath(const QString &path, const bool bshareuse = false);
    UDiskDeviceInfoPointer getDeviceByDeviceID(const QString &deviceID);

    void loadCustomVolumeLetters();
    QMap<QString, QString> getVolumeLetters();

    QMap<QString, UDiskDeviceInfoPointer> getMountedRemovableDiskDeviceInfos();

    QMap<QString, UDiskDeviceInfoPointer> getCanSendDisksByUrl(QString filepath);

    bool isMountedRemovableDiskExits();

    bool isFileFromDisc(const QString &filePath); // 文件是否来自光盘

signals:
    void volumeAdded(UDiskDeviceInfoPointer device);
    void volumeRemoved(UDiskDeviceInfoPointer device);
    void volumeChanged(UDiskDeviceInfoPointer device);
    void mountAdded(UDiskDeviceInfoPointer device);
    void mountRemoved(UDiskDeviceInfoPointer device);

public slots:
    void update();
    void addMountDiskInfo(const QDiskInfo &diskInfo);
    void removeMountDiskInfo(const QDiskInfo &diskInfo);
    void addVolumeDiskInfo(const QDiskInfo &diskInfo);
    void removeVolumeDiskInfo(const QDiskInfo &diskInfo);
    void changeVolumeDiskInfo(const QDiskInfo &diskInfo);
    void mount(const QString &path);
    bool mountByUDisks(const QString &path);
    void unmount(const QString &path);
    void eject(const QString &path);
    void stopDrive(const QString &path);
    void forceUnmount(const QString &id);

private slots:
    void fileSystemDeviceIdLabelChanged(const QString &path);
    void insertFileSystemDevice(const QString dbusPath);

private:
    DDiskManager *m_diskMgr = nullptr;
    QMap<QString, DBlockDevice *> m_fsDevMap;

    QList<UDiskDeviceInfoPointer> m_list;
    QList<UDiskDeviceInfoPointer> m_mountList;
    QMap<QString, UDiskDeviceInfoPointer> m_map;
    QMap<QString, QString> m_volumeLetters;

    QList<Subscriber *> m_subscribers;

    QTimer *m_diskTimer;
    int m_nCDRomCount = 0; // 光驱接入个数

};

#endif // UDISKLISTENER_H
