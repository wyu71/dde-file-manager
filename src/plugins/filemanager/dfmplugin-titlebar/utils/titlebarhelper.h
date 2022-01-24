/*
 * Copyright (C) 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     zhangsheng<zhangsheng@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             xushitong<xushitong@uniontech.com>
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
#ifndef TITLEBARHELPER_H
#define TITLEBARHELPER_H

#include "dfmplugin_titlebar_global.h"
#include "services/filemanager/titlebar/titlebar_defines.h"

#include <QMap>
#include <QMutex>
#include <QWidget>
#include <QMenu>

DPTITLEBAR_BEGIN_NAMESPACE

using DSB_FM_NAMESPACE::TitleBar::CrumbData;

class TitleBarWidget;
class TitleBarHelper
{
public:
    static TitleBarWidget *findTileBarByWindowId(quint64 windowId);
    static void addTileBar(quint64 windowId, TitleBarWidget *titleBar);
    static void removeTitleBar(quint64 windowId);
    static quint64 windowId(QWidget *sender);
    static QMenu *createSettingsMenu(quint64 id);
    static bool crumbSupportedUrl(const QUrl &url);
    static QList<CrumbData> crumbSeprateUrl(const QUrl &url);
    static bool displayIcon();
    static bool tabAddable(quint64 windowId);

private:
    static QMutex &mutex();
    static QString getDisplayName(const QString &name);
    static QMap<quint64, TitleBarWidget *> kTitleBarMap;
};

DPTITLEBAR_END_NAMESPACE

#endif   // TITLEBARHELPER_H