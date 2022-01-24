/*
 * Copyright (C) 2021 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     huanyu<huanyub@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
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
#include "localfileinfo.h"
#include "private/localfileinfo_p.h"
#include "base/urlroute.h"
#include "base/standardpaths.h"
#include "base/schemefactory.h"
#include "utils/fileutils.h"
#include "utils/systempathutil.h"
#include "dfileiconprovider.h"

#include <dfm-io/local/dlocalfileinfo.h>

#include <QDateTime>
#include <QDir>
#include <QPainter>
#include <QApplication>
#include <QtConcurrent>
#include <qplatformdefs.h>

#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <mntent.h>

/*!
 * \class LocalFileInfo 本地文件信息类
 * \brief 内部实现本地文件的fileinfo，对应url的scheme是file://
 */
DWIDGET_USE_NAMESPACE
DFMBASE_BEGIN_NAMESPACE

LocalFileInfo::LocalFileInfo(const QUrl &url)
    : AbstractFileInfo(url, new LocalFileInfoPrivate(this))
{
    d = static_cast<LocalFileInfoPrivate *>(dptr.data());
    init(url);
}

LocalFileInfo::~LocalFileInfo()
{
    d = nullptr;
}

LocalFileInfo &LocalFileInfo::operator=(const LocalFileInfo &info)
{
    d->lock.lockForRead();
    AbstractFileInfo::operator=(info);
    d->url = info.d->url;
    d->dfmFileInfo = info.d->dfmFileInfo;
    d->inode = info.d->inode;
    d->lock.unlock();
    return *this;
}
/*!
 * \brief == 重载操作符==
 *
 * \param const DAbstractFileInfo & DAbstractFileInfo实例对象的引用
 *
 * \return bool 传入的DAbstractFileInfo实例对象和自己是否相等
 */
bool LocalFileInfo::operator==(const LocalFileInfo &fileinfo) const
{
    return d->dfmFileInfo == fileinfo.d->dfmFileInfo && d->url == fileinfo.d->url;
}
/*!
 * \brief != 重载操作符!=
 *
 * \param const LocalFileInfo & LocalFileInfo实例对象的引用
 *
 * \return bool 传入的LocalFileInfo实例对象和自己是否不相等
 */
bool LocalFileInfo::operator!=(const LocalFileInfo &fileinfo) const
{
    return !(operator==(fileinfo));
}
/*!
 * \brief setFile 设置文件的File，跟新当前的fileinfo
 *
 * \param const QSharedPointer<DFMIO::DFileInfo> &file 新文件的dfm-io的fileinfo
 *
 * \return
 */
void LocalFileInfo::setFile(const QUrl &url)
{
    d->lock.lockForWrite();
    d->url = url;
    init(url);
    d->lock.unlock();
}
/*!
 * \brief exists 文件是否存在
 *
 * \param
 *
 * \return 返回文件是否存在
 */
bool LocalFileInfo::exists() const
{
    bool exists = false;
    d->lock.lockForRead();
    if (d->dfmFileInfo) {
        exists = d->dfmFileInfo->exists();
    } else {
        exists = QFileInfo::exists(d->url.path());
    }
    d->lock.unlock();

    return exists;
}
/*!
 * \brief refresh 跟新文件信息，清理掉缓存的所有的文件信息
 *
 * \param
 *
 * \return
 */
void LocalFileInfo::refresh()
{
    d->lock.lockForWrite();
    d->dfmFileInfo->flush();
    d->lock.unlock();
}
/*!
 * \brief filePath 获取文件的绝对路径，含文件的名称，相当于文件的全路径
 *
 * url = file:///tmp/archive.tar.gz
 *
 * filePath = /tmp/archive.tar.gz
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::filePath() const
{
    //文件的路径
    d->lock.lockForRead();
    QString filePath;
    bool success = false;
    if (d->dfmFileInfo) {
        filePath = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardFilePath, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardFilePath failed! case : %1 " << d->dfmFileInfo->lastError().errorMsg();
    }

    if (!success)
        filePath = QFileInfo(d->url.path()).filePath();

    d->lock.unlock();
    return filePath;
}

/*!
 * \brief absoluteFilePath 获取文件的绝对路径，含文件的名称，相当于文件的全路径，事例如下：
 *
 * url = file:///tmp/archive.tar.gz
 *
 * absoluteFilePath = /tmp/archive.tar.gz
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::absoluteFilePath() const
{
    return filePath();
}
/*!
 * \brief fileName 文件名称，全名称
 *
 * url = file:///tmp/archive.tar.gz
 *
 * fileName = archive.tar.gz
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::fileName() const
{
    d->lock.lockForRead();
    bool success = false;
    QString fileName;
    if (d->dfmFileInfo) {
        fileName = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardName, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardName failed!";
    }
    if (!success)
        fileName = QFileInfo(d->url.path()).fileName();
    d->lock.unlock();
    return fileName;
}
/*!
 * \brief baseName 文件的基本名称
 *
 * url = file:///tmp/archive.tar.gz
 *
 * baseName = archive
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::baseName() const
{
    d->lock.lockForRead();
    bool success = false;
    QString baseName;
    if (d->dfmFileInfo) {
        baseName = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardBaseName, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardBaseName failed!";
    }
    if (!success)
        baseName = QFileInfo(d->url.path()).baseName();
    d->lock.unlock();
    return baseName;
}
/*!
 * \brief completeBaseName 文件的完整基本名称
 *
 * url = file:///tmp/archive.tar.gz
 *
 * completeBaseName = archive.tar
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::completeBaseName() const
{
    d->lock.lockForRead();
    bool success = false;
    QString completeBaseName;
    if (d->dfmFileInfo) {
        completeBaseName = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardBaseName, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardBaseName failed!";
    }
    if (!success)
        completeBaseName = QFileInfo(d->url.path()).completeBaseName();
    d->lock.unlock();
    return completeBaseName;
}
/*!
 * \brief suffix 文件的suffix
 *
 * url = file:///tmp/archive.tar.gz
 *
 * suffix = gz
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::suffix() const
{
    d->lock.lockForRead();
    bool success = false;
    QString suffix;
    if (d->dfmFileInfo) {
        suffix = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardSuffix, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardSuffix failed!";
    }
    if (!success)
        suffix = QFileInfo(d->url.path()).suffix();
    d->lock.unlock();
    return suffix;
}
/*!
 * \brief suffix 文件的完整suffix
 *
 * url = file:///tmp/archive.tar.gz
 *
 * suffix = tar.gz
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::completeSuffix()
{
    d->lock.lockForRead();
    bool success = false;
    QString completeSuffix;
    if (d->dfmFileInfo) {
        completeSuffix = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardCompleteSuffix, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardCompleteSuffix failed!";
    }
    if (!success)
        completeSuffix = QFileInfo(d->url.path()).completeSuffix();
    d->lock.unlock();
    return completeSuffix;
}
/*!
 * \brief path 获取文件路径，不包含文件的名称，相当于是父目录
 *
 * url = file:///tmp/archive.tar.gz
 *
 * path = /tmp
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::path() const
{
    d->lock.lockForRead();
    bool success = false;
    QString path;
    if (d->dfmFileInfo) {
        path = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardFilePath, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardFilePath failed!";
    }
    if (!success)
        path = QFileInfo(d->url.path()).path();
    d->lock.unlock();
    return path;
}
/*!
 * \brief path 获取文件路径，不包含文件的名称，相当于是父目录
 *
 * url = file:///tmp/archive.tar.gz
 *
 * absolutePath = /tmp
 *
 * \param
 *
 * \return
 */
QString LocalFileInfo::absolutePath() const
{
    d->lock.lockForRead();
    bool success = false;
    QString path;
    if (d->dfmFileInfo) {
        path = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardParentPath, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardFilePath failed!";
    }
    if (!success)
        path = QFileInfo(d->url.path()).absolutePath();
    d->lock.unlock();
    return path;
}
/*!
 * \brief canonicalPath 获取文件canonical路径，包含文件的名称，相当于文件的全路径
 *
 * url = file:///tmp/archive.tar.gz
 *
 * canonicalPath = /tmp/archive.tar.gz
 *
 * \param
 *
 * \return QString 返回没有符号链接或冗余“.”或“..”元素的绝对路径
 */
QString LocalFileInfo::canonicalPath() const
{
    return filePath();
}
/*!
 * \brief dir 获取文件的父母目录的QDir
 *
 * Returns the path of the object's parent directory as a QDir object.
 *
 * url = file:///tmp/archive.tar.gz
 *
 * dirpath = /tmp
 *
 * \param
 *
 * \return QDir 父母目录的QDir实例对象
 */
QDir LocalFileInfo::dir() const
{
    return QDir(path());
}
/*!
 * \brief absoluteDir 获取文件的父母目录的QDir
 *
 * Returns the file's absolute path as a QDir object.
 *
 * url = file:///tmp/archive.tar.gz
 *
 * absolute path = /tmp
 *
 * \param
 *
 * \return QDir 父母目录的QDir实例对象
 */
QDir LocalFileInfo::absoluteDir() const
{
    return dir();
}
/*!
 * \brief url 获取文件的url，这里的url是转换后的
 *
 * \param
 *
 * \return QUrl 返回真实路径转换的url
 */
QUrl LocalFileInfo::url() const
{
    d->lock.lockForRead();
    QUrl tmp = d->dfmFileInfo->uri();
    d->lock.unlock();
    return tmp;
}

bool LocalFileInfo::canRename() const
{
    if (SystemPathUtil::instance()->isSystemPath(absoluteFilePath()))
        return false;
    return isWritable();
}
/*!
 * \brief isReadable 获取文件是否可读
 *
 * Returns the file can Read
 *
 * url = file:///tmp/archive.tar.gz
 *
 * \param
 *
 * \return bool 返回文件是否可读
 */
bool LocalFileInfo::isReadable() const
{
    d->lock.lockForRead();
    bool success = false;
    bool isReadable = false;
    if (d->dfmFileInfo) {
        isReadable = d->dfmFileInfo->attribute(DFileInfo::AttributeID::AccessCanRead, &success).toBool();
        if (!success)
            qWarning() << "get dfm-io DFileInfo AccessCanRead failed!";
    }
    if (!success)
        isReadable = QFileInfo(d->url.path()).isReadable();
    d->lock.unlock();
    return isReadable;
}
/*!
 * \brief isWritable 获取文件是否可写
 *
 * Returns the file can write
 *
 * url = file:///tmp/archive.tar.gz
 *
 * \param
 *
 * \return bool 返回文件是否可写
 */
bool LocalFileInfo::isWritable() const
{
    d->lock.lockForRead();
    bool success = false;
    bool isWritable = false;
    if (d->dfmFileInfo) {
        isWritable = d->dfmFileInfo->attribute(DFileInfo::AttributeID::AccessCanWrite, &success).toBool();
        if (!success)
            qWarning() << "get dfm-io DFileInfo AccessCanWrite failed!";
    }
    if (!success)
        isWritable = QFileInfo(d->url.path()).isWritable();
    d->lock.unlock();
    return isWritable;
}
/*!
 * \brief isExecutable 获取文件是否可执行
 *
 * \param
 *
 * \return bool 返回文件是否可执行
 */
bool LocalFileInfo::isExecutable() const
{
    d->lock.lockForRead();
    bool success = false;
    bool isExecutable = false;
    if (d->dfmFileInfo) {
        isExecutable = d->dfmFileInfo->attribute(DFileInfo::AttributeID::AccessCanExecute, &success).toBool();
        if (!success)
            qWarning() << "get dfm-io DFileInfo AccessCanExecute failed!";
    }
    if (!success)
        isExecutable = QFileInfo(d->url.path()).isExecutable();
    d->lock.unlock();
    return isExecutable;
}
/*!
 * \brief isHidden 获取文件是否是隐藏
 *
 * \param
 *
 * \return bool 返回文件是否隐藏
 */
bool LocalFileInfo::isHidden() const
{
    d->lock.lockForRead();
    bool isHidden = false;
    if (d->dfmFileInfo) {
        isHidden = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardIsHidden, nullptr).toBool();
    }
    d->lock.unlock();
    return isHidden;
}
/*!
 * \brief isFile 获取文件是否是文件
 *
 * Returns true if this object points to a file or to a symbolic link to a file.
 *
 * Returns false if the object points to something which isn't a file,
 *
 * such as a directory.
 *
 * \param
 *
 * \return bool 返回文件是否文件
 */
bool LocalFileInfo::isFile() const
{
    d->lock.lockForRead();
    bool success = false;
    bool isFile = false;
    if (d->dfmFileInfo) {
        isFile = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardIsFile, &success).toBool();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardIsFile failed!";
    }
    if (!success)
        isFile = QFileInfo(d->url.path()).isFile();
    d->lock.unlock();
    return isFile;
}
/*!
 * \brief isDir 获取文件是否是目录
 *
 * Returns true if this object points to a directory or to a symbolic link to a directory;
 *
 * otherwise returns false.
 *
 * \param
 *
 * \return bool 返回文件是否目录
 */
bool LocalFileInfo::isDir() const
{
    d->lock.lockForRead();
    bool success = false;
    bool isDir = false;
    if (d->dfmFileInfo) {
        isDir = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardIsDir, &success).toBool();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardIsDir failed!";
    }
    if (!success)
        isDir = QFileInfo(d->url.path()).isDir();
    d->lock.unlock();
    return isDir;
}
/*!
 * \brief isSymLink 获取文件是否是链接文件
 *
 * Returns true if this object points to a symbolic link;
 *
 * otherwise returns false.Symbolic links exist on Unix (including macOS and iOS)
 *
 * and Windows and are typically created by the ln -s or mklink commands, respectively.
 *
 * Opening a symbolic link effectively opens the link's target.
 *
 * In addition, true will be returned for shortcuts (*.lnk files) on Windows.
 *
 * Opening those will open the .lnk file itself.
 *
 * \param
 *
 * \return bool 返回文件是否是链接文件
 */
bool LocalFileInfo::isSymLink() const
{
    d->lock.lockForRead();
    bool success = false;
    bool isSymLink = false;
    if (d->dfmFileInfo) {
        isSymLink = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardIsSymlink, &success).toBool();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardIsSymlink failed!";
    }
    if (!success)
        isSymLink = QFileInfo(d->url.path()).isSymLink();
    d->lock.unlock();
    return isSymLink;
}
/*!
 * \brief isRoot 获取文件是否是根目录
 *
 * Returns true if the object points to a directory or to a symbolic link to a directory,
 *
 * and that directory is the root directory; otherwise returns false.
 *
 * \param
 *
 * \return bool 返回文件是否是根目录
 */
bool LocalFileInfo::isRoot() const
{
    return filePath() == "/";
}
/*!
 * \brief isBundle 获取文件是否是二进制文件
 *
 * Returns true if this object points to a bundle or to a symbolic
 *
 * link to a bundle on macOS and iOS; otherwise returns false.
 *
 * \param
 *
 * \return bool 返回文件是否是二进制文件
 */
bool LocalFileInfo::isBundle() const
{
    d->lock.lockForRead();
    bool isBundle = QFileInfo(d->url.path()).isBundle();
    d->lock.unlock();
    return isBundle;
}
/*!
 * \brief isBundle 获取文件的链接目标文件
 *
 * Returns the absolute path to the file or directory a symbolic link points to,
 *
 * or an empty string if the object isn't a symbolic link.
 *
 * This name may not represent an existing file; it is only a string.
 *
 * QFileInfo::exists() returns true if the symlink points to an existing file.
 *
 * \param
 *
 * \return QString 链接目标文件的路径
 */
QString LocalFileInfo::symLinkTarget() const
{
    d->lock.lockForRead();
    bool success = false;
    QString symLinkTarget;
    if (d->dfmFileInfo) {
        symLinkTarget = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardSymlinkTarget, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardSymlinkTarget failed!";
    }
    if (!success)
        symLinkTarget = QFileInfo(d->url.path()).symLinkTarget();
    d->lock.unlock();
    return symLinkTarget;
}
/*!
 * \brief owner 获取文件的拥有者
 *
 * Returns the owner of the file. On systems where files do not have owners,
 *
 * or if an error occurs, an empty string is returned.
 *
 * This function can be time consuming under Unix (in the order of milliseconds).
 *
 * \param
 *
 * \return QString 文件的拥有者
 */
QString LocalFileInfo::owner() const
{
    d->lock.lockForRead();
    bool success = false;
    QString owner;
    if (d->dfmFileInfo) {
        owner = d->dfmFileInfo->attribute(DFileInfo::AttributeID::OwnerUser, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo OwnerUser failed!";
    }
    if (!success)
        owner = QFileInfo(d->url.path()).owner();
    d->lock.unlock();
    return owner;
}
/*!
 * \brief ownerId 获取文件的拥有者ID
 *
 * Returns the id of the owner of the file.
 *
 * \param
 *
 * \return uint 文件的拥有者ID
 */
uint LocalFileInfo::ownerId() const
{
    d->lock.lockForRead();
    bool success = false;
    uint ownerId = 0;
    if (d->dfmFileInfo) {
        ownerId = d->dfmFileInfo->attribute(DFileInfo::AttributeID::OwnerUser, &success).toUInt();
        if (!success)
            qWarning() << "get dfm-io DFileInfo OwnerUser failed!";
    }
    if (!success)
        ownerId = QFileInfo(d->url.path()).ownerId();
    d->lock.unlock();
    return ownerId;
}
/*!
 * \brief group 获取文件所属组
 *
 * Returns the group of the file.
 *
 * This function can be time consuming under Unix (in the order of milliseconds).
 *
 * \param
 *
 * \return QString 文件所属组
 */
QString LocalFileInfo::group() const
{
    d->lock.lockForRead();
    bool success = false;
    QString group;
    if (d->dfmFileInfo) {
        group = d->dfmFileInfo->attribute(DFileInfo::AttributeID::OwnerGroup, &success).toString();
        if (!success)
            qWarning() << "get dfm-io DFileInfo OwnerGroup failed!";
    }
    if (!success)
        group = QFileInfo(d->url.path()).group();
    d->lock.unlock();
    return group;
}
/*!
 * \brief groupId 获取文件所属组的ID
 *
 * Returns the id of the group the file belongs to.
 *
 * \param
 *
 * \return uint 文件所属组ID
 */
uint LocalFileInfo::groupId() const
{
    d->lock.lockForRead();
    bool success = false;
    uint groupId = 0;
    if (d->dfmFileInfo) {
        groupId = d->dfmFileInfo->attribute(DFileInfo::AttributeID::OwnerGroup, &success).toUInt();
        if (!success)
            qWarning() << "get dfm-io DFileInfo OwnerGroup failed!";
    }
    if (!success)
        groupId = QFileInfo(d->url.path()).groupId();
    d->lock.unlock();
    return groupId;
}
/*!
 * \brief permission 判断文件是否有传入的权限
 *
 * Tests for file permissions. The permissions argument can be several flags
 *
 * of type QFile::Permissions OR-ed together to check for permission combinations.
 *
 * On systems where files do not have permissions this function always returns true.
 *
 * \param QFile::Permissions permissions 文件的权限
 *
 * \return bool 是否有传入的权限
 */
bool LocalFileInfo::permission(QFileDevice::Permissions permissions) const
{
    return this->permissions() & permissions;
}
/*!
 * \brief permissions 获取文件的全部权限
 *
 * \param
 *
 * \return QFile::Permissions 文件的全部权限
 */
QFileDevice::Permissions LocalFileInfo::permissions() const
{
    d->lock.lockForRead();
    bool success = false;
    QFileDevice::Permissions ps;
    if (d->dfmFileInfo) {
        ps = static_cast<QFileDevice::Permissions>(static_cast<uint16_t>(d->dfmFileInfo->permissions()));
        if (!success)
            qWarning() << "get dfm-io DFileInfo permissions failed!";
    }
    if (!success)
        ps = QFileInfo(d->url.path()).permissions();
    d->lock.unlock();
    return ps;
}
/*!
 * \brief size 获取文件的大小
 *
 * Returns the file size in bytes.
 *
 * If the file does not exist or cannot be fetched, 0 is returned.
 *
 * \param
 *
 * \return qint64 文件的大小
 */
qint64 LocalFileInfo::size() const
{
    d->lock.lockForRead();
    bool success = false;
    qint64 size = 0;
    if (d->dfmFileInfo) {
        size = d->dfmFileInfo->attribute(DFileInfo::AttributeID::StandardSize, &success).value<qint64>();
        if (!success)
            qWarning() << "get dfm-io DFileInfo StandardSize failed!";
    }
    if (!success)
        size = QFileInfo(d->url.path()).size();
    d->lock.unlock();
    return size;
}
/*!
 * \brief created 获取文件的创建时间
 *
 * Returns the date and time when the file was created,
 *
 * the time its metadata was last changed or the time of last modification,
 *
 * whichever one of the three is available (in that order).
 *
 * \param
 *
 * \return QDateTime 文件的创建时间的QDateTime实例
 */
QDateTime LocalFileInfo::created() const
{
    d->lock.lockForRead();
    bool success = false;
    uint64_t created = 0;
    if (d->dfmFileInfo) {
        created = d->dfmFileInfo->attribute(DFileInfo::AttributeID::TimeCreated, &success).value<uint64_t>();
        if (!success)
            qWarning() << "get dfm-io DFileInfo TimeCreated failed!";
    }
    QDateTime time = QDateTime::fromTime_t(static_cast<uint>(created));
    if (!success)
        time = QFileInfo(d->url.path()).created();
    d->lock.unlock();
    return time;
}
/*!
 * \brief birthTime 获取文件的创建时间
 *
 * Returns the date and time when the file was created / born.
 *
 * If the file birth time is not available, this function
 *
 * returns an invalid QDateTime.
 *
 * \param
 *
 * \return QDateTime 文件的创建时间的QDateTime实例
 */
QDateTime LocalFileInfo::birthTime() const
{
    return created();
}
/*!
 * \brief metadataChangeTime 获取文件的改变时间
 *
 * Returns the date and time when the file metadata was changed.
 *
 * A metadata change occurs when the file is created,
 *
 * but it also occurs whenever the user writes or sets
 *
 * inode information (for example, changing the file permissions).
 *
 * \param
 *
 * \return QDateTime 文件的改变时间的QDateTime实例
 */
QDateTime LocalFileInfo::metadataChangeTime() const
{
    d->lock.lockForRead();
    bool success = false;
    uint64_t data = 0;
    if (d->dfmFileInfo) {
        data = d->dfmFileInfo->attribute(DFileInfo::AttributeID::TimeChanged, &success).value<uint64_t>();
        if (!success)
            qWarning() << "get dfm-io DFileInfo TimeChanged failed!";
    }
    QDateTime time = QDateTime::fromTime_t(static_cast<uint>(data));
    if (!success)
        time = QFileInfo(d->url.path()).metadataChangeTime();
    d->lock.unlock();
    return time;
}
/*!
 * \brief lastModified 获取文件的最后修改时间
 *
 * \param
 *
 * \return QDateTime 文件的最后修改时间的QDateTime实例
 */
QDateTime LocalFileInfo::lastModified() const
{
    d->lock.lockForRead();
    bool success = false;
    uint64_t data = 0;
    if (d->dfmFileInfo) {
        data = d->dfmFileInfo->attribute(DFileInfo::AttributeID::TimeModified, &success).value<uint64_t>();
        if (!success)
            qWarning() << "get dfm-io DFileInfo TimeModified failed!";
    }
    QDateTime time = QDateTime::fromTime_t(static_cast<uint>(data));
    if (!success)
        time = QFileInfo(d->url.path()).lastModified();
    d->lock.unlock();
    return time;
}
/*!
 * \brief lastRead 获取文件的最后读取时间
 *
 * \param
 *
 * \return QDateTime 文件的最后读取时间的QDateTime实例
 */
QDateTime LocalFileInfo::lastRead() const
{
    d->lock.lockForRead();
    bool success = false;
    uint64_t data = 0;
    if (d->dfmFileInfo) {
        data = d->dfmFileInfo->attribute(DFileInfo::AttributeID::TimeChanged, &success).value<uint64_t>();
        if (!success)
            qWarning() << "get dfm-io DFileInfo TimelastRead failed!";
    }
    QDateTime time = QDateTime::fromTime_t(static_cast<uint>(data));
    if (!success)
        time = QFileInfo(d->url.path()).lastRead();
    d->lock.unlock();
    return time;
}
/*!
 * \brief fileTime 获取文件的事件通过传入的参数
 *
 * \param QFile::FileTime time 时间类型
 *
 * \return QDateTime 文件的不同时间类型的时间的QDateTime实例
 */
QDateTime LocalFileInfo::fileTime(QFileDevice::FileTime time) const
{
    if (time == QFileDevice::FileAccessTime) {
        return lastRead();
    } else if (time == QFileDevice::FileBirthTime) {
        return created();
    } else if (time == QFileDevice::FileMetadataChangeTime) {
        return metadataChangeTime();
    } else if (time == QFileDevice::FileModificationTime) {
        return lastModified();
    } else {
        return QDateTime();
    }
}
/*!
 * \brief isBlockDev 获取是否是块设备
 *
 * \return bool 是否是块设备
 */
bool LocalFileInfo::isBlockDev() const
{
    return fileType() == kBlockDevice;
}
/*!
 * \brief mountPath 获取挂载路径
 *
 * \return QString 挂载路径
 */
QString LocalFileInfo::mountPath() const
{
    // TODO::
    if (!isBlockDev())
        return "";
    else
        return "";
}
/*!
 * \brief isCharDev 获取是否是字符设备
 *
 * \return bool 是否是字符设备
 */
bool LocalFileInfo::isCharDev() const
{
    return fileType() == kCharDevice;
}
/*!
 * \brief isFifo 获取当前是否为管道文件
 *
 * \return bool 返回当前是否为管道文件
 */
bool LocalFileInfo::isFifo() const
{
    return fileType() == kFIFOFile;
}
/*!
 * \brief isSocket 获取当前是否为套接字文件
 *
 * \return bool 返回是否为套接字文件
 */
bool LocalFileInfo::isSocket() const
{
    return fileType() == kSocketFile;
}
/*!
 * \brief isRegular 获取当前是否是常规文件(与isFile一致)
 *
 * \return bool 返回是否是常规文件(与isFile一致)
 */
bool LocalFileInfo::isRegular() const
{
    return fileType() == kRegularFile;
}

/*!
 * \brief fileType 获取文件类型
 *
 * \return DMimeDatabase::FileType 文件设备类型
 */
LocalFileInfo::Type LocalFileInfo::fileType() const
{
    d->lock.lockForRead();
    Type fileType;
    if (d->fileType != MimeDatabase::FileType::kUnknown) {
        fileType = Type(d->fileType);
        d->lock.unlock();
        return fileType;
    }

    QString absoluteFilePath = filePath();
    if (absoluteFilePath.startsWith(StandardPaths::location(StandardPaths::kTrashFilesPath))
        && isSymLink()) {
        d->fileType = MimeDatabase::FileType::kRegularFile;
        fileType = Type(d->fileType);
        d->lock.unlock();
        return fileType;
    }

    // Cannot access statBuf.st_mode from the filesystem engine, so we have to stat again.
    // In addition we want to follow symlinks.
    const QByteArray &nativeFilePath = QFile::encodeName(absoluteFilePath);
    QT_STATBUF statBuffer;
    if (QT_STAT(nativeFilePath.constData(), &statBuffer) == 0) {
        if (S_ISDIR(statBuffer.st_mode))
            d->fileType = MimeDatabase::FileType::kDirectory;

        else if (S_ISCHR(statBuffer.st_mode))
            d->fileType = MimeDatabase::FileType::kCharDevice;

        else if (S_ISBLK(statBuffer.st_mode))
            d->fileType = MimeDatabase::FileType::kBlockDevice;

        else if (S_ISFIFO(statBuffer.st_mode))
            d->fileType = MimeDatabase::FileType::kFIFOFile;

        else if (S_ISSOCK(statBuffer.st_mode))
            d->fileType = MimeDatabase::FileType::kSocketFile;

        else if (S_ISREG(statBuffer.st_mode))
            d->fileType = MimeDatabase::FileType::kRegularFile;
    }

    fileType = Type(d->fileType);
    d->lock.unlock();
    return fileType;
}
/*!
 * \brief countChildFile 文件夹下子文件的个数，只统计下一层不递归
 *
 * \return int 子文件个数
 */
int LocalFileInfo::countChildFile() const
{
    if (isDir()) {
        d->lock.lockForRead();
        QDir dir(absoluteFilePath());
        QStringList entryList = dir.entryList(QDir::AllEntries | QDir::System
                                              | QDir::NoDotAndDotDot | QDir::Hidden);
        d->lock.unlock();
        return entryList.size();
    }

    return -1;
}
/*!
 * \brief sizeFormat 格式化大小
 * \return QString 大小格式化后的大小
 */
QString LocalFileInfo::sizeFormat() const
{
    if (isDir()) {
        return QStringLiteral("-");
    }

    qlonglong fileSize(size());
    bool withUnitVisible = true;
    int forceUnit = -1;

    if (fileSize < 0) {
        qWarning() << "Negative number passed to formatSize():" << fileSize;
        fileSize = 0;
    }

    bool isForceUnit = false;
    QStringList list { " B", " KB", " MB", " GB", " TB" };

    QStringListIterator i(list);
    QString unit = i.hasNext() ? i.next() : QStringLiteral(" B");

    int index = 0;
    while (i.hasNext()) {
        if (fileSize < 1024 && !isForceUnit) {
            break;
        }

        if (isForceUnit && index == forceUnit) {
            break;
        }

        unit = i.next();
        fileSize /= 1024;
        index++;
    }
    QString unitString = withUnitVisible ? unit : QString();
    return QString("%1%2").arg(d->sizeString(QString::number(fileSize, 'f', 1)), unitString);
}
/*!
 * \brief fileDisplayName 文件的显示名称，一般为文件的名称
 *
 * \return QString 文件的显示名称
 */
QString LocalFileInfo::fileDisplayName() const
{
    return fileName();
}
/*!
 * \brief toQFileInfo 获取他的QFileInfo实例对象
 *
 * \return QFileInfo 文件的QFileInfo实例
 */
QFileInfo LocalFileInfo::toQFileInfo() const
{
    QFileInfo info = QFileInfo(d->url.path());
    return info;
}
/*!
 * \brief extraProperties 获取文件的扩展属性
 *
 * \return QVariantHash 文件的扩展属性Hash
 */
QVariantHash LocalFileInfo::extraProperties() const
{
    if (d->extraProperties.isEmpty()) {
    }
    return d->extraProperties;
}

QIcon LocalFileInfo::fileIcon() const
{
    d->lock.lockForRead();
    QIcon icon = DFileIconProvider::globalProvider()->icon(this->path());
    d->lock.unlock();
    return icon;
}
/*!
 * \brief inode linux系统下的唯一表示符
 *
 * \return quint64 文件的inode
 */
quint64 LocalFileInfo::inode() const
{
    d->lock.lockForRead();
    quint64 inNode = d->inode;
    if (d->inode != 0) {
        d->lock.unlock();
        return inNode;
    }

    struct stat statinfo;
    int filestat = stat(absoluteFilePath().toStdString().c_str(), &statinfo);
    if (filestat != 0) {
        d->lock.unlock();
        return 0;
    }
    d->inode = statinfo.st_ino;
    d->lock.unlock();
    return d->inode;
}

QMimeType LocalFileInfo::fileMimeType() const
{
    return MimeDatabase::mimeTypeForUrl(d->url);
}

void LocalFileInfo::init(const QUrl &url)
{
    if (url.isEmpty()) {
        qWarning("Failed, can't use empty url init fileinfo");
        abort();
    }

    if (UrlRoute::isVirtual(url)) {
        qWarning("Failed, can't use virtual scheme init local fileinfo");
        abort();
    }

    QUrl cvtResultUrl = QUrl::fromLocalFile(UrlRoute::urlToPath(url));

    if (!url.isValid()) {
        qWarning("Failed, can't use valid url init fileinfo");
        abort();
    }

    QSharedPointer<DIOFactory> factory = produceQSharedIOFactory(cvtResultUrl.scheme(), static_cast<QUrl>(cvtResultUrl));
    if (!factory) {
        qWarning("Failed, dfm-io create factory");
        abort();
    }

    d->dfmFileInfo = factory->createFileInfo();
    if (!d->dfmFileInfo) {
        qWarning("Failed, dfm-io use factory create fileinfo");
        abort();
    }
}

DFMBASE_END_NAMESPACE