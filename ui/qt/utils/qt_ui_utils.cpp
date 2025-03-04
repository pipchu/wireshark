/* qt_ui_utils.cpp
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ui/qt/utils/qt_ui_utils.h>

#include <epan/addr_resolv.h>
#include <epan/range.h>
#include <epan/to_str.h>
#include <epan/value_string.h>

#include <ui/recent.h>
#include "ui/ws_ui_util.h"

#include <wsutil/str_util.h>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QProcess>
#include <QUrl>
#include <QUuid>
#include <QScreen>

/* Make the format_size_flags_e enum usable in C++ */
format_size_flags_e operator|(format_size_flags_e lhs, format_size_flags_e rhs) {
    return (format_size_flags_e) ((int)lhs| (int)rhs);
}

/*
 * We might want to create our own "wsstring" class with convenience
 * methods for handling g_malloc()ed strings, GStrings, and a shortcut
 * to .toUtf8().constData().
 */

gchar *qstring_strdup(QString q_string) {
    return g_strdup(q_string.toUtf8().constData());
}

QString gchar_free_to_qstring(gchar *glib_string) {
    return QString(gchar_free_to_qbytearray(glib_string));
}

QByteArray gchar_free_to_qbytearray(gchar *glib_string)
{
    QByteArray qt_bytearray(glib_string);
    g_free(glib_string);
    return qt_bytearray;
}

QByteArray gstring_free_to_qbytearray(GString *glib_gstring)
{
    QByteArray qt_ba(glib_gstring->str);
    g_string_free(glib_gstring, TRUE);
    return qt_ba;
}

const QString int_to_qstring(qint64 value, int field_width, int base)
{
    // Qt deprecated QString::sprintf in Qt 5.0, then added ::asprintf in
    // Qt 5.5. Rather than navigate a maze of QT_VERSION_CHECKs, just use
    // QString::arg.
    QString int_qstr;

    switch (base) {
    case 8:
        int_qstr = "0";
        break;
    case 16:
        int_qstr = "0x";
        break;
    default:
        break;
    }

    int_qstr += QString("%1").arg(value, field_width, base, QChar('0'));
    return int_qstr;
}

const QString address_to_qstring(const _address *address, bool enclose)
{
    QString address_qstr = QString();
    if (address) {
        if (enclose && address->type == AT_IPv6) address_qstr += "[";
        gchar *address_gchar_p = address_to_str(NULL, address);
        address_qstr += address_gchar_p;
        wmem_free(NULL, address_gchar_p);
        if (enclose && address->type == AT_IPv6) address_qstr += "]";
    }
    return address_qstr;
}

const QString address_to_display_qstring(const _address *address)
{
    QString address_qstr = QString();
    if (address) {
        gchar *address_gchar_p = address_to_display(NULL, address);
        address_qstr = address_gchar_p;
        wmem_free(NULL, address_gchar_p);
    }
    return address_qstr;
}

const QString val_to_qstring(const guint32 val, const value_string *vs, const char *fmt)
{
    QString val_qstr;
    gchar* gchar_p = val_to_str_wmem(NULL, val, vs, fmt);
    val_qstr = gchar_p;
    wmem_free(NULL, gchar_p);

    return val_qstr;
}

const QString val_ext_to_qstring(const guint32 val, value_string_ext *vse, const char *fmt)
{
    QString val_qstr;
    gchar* gchar_p = val_to_str_ext_wmem(NULL, val, vse, fmt);
    val_qstr = gchar_p;
    wmem_free(NULL, gchar_p);

    return val_qstr;
}

const QString range_to_qstring(const epan_range *range)
{
    QString range_qstr = QString();
    if (range) {
        gchar *range_gchar_p = range_convert_range(NULL, range);
        range_qstr = range_gchar_p;
        wmem_free(NULL, range_gchar_p);
    }
    return range_qstr;
}

const QString bits_s_to_qstring(const double bits_s)
{
    return gchar_free_to_qstring(
                format_size(bits_s, format_size_unit_none|format_size_prefix_si));
}

const QString file_size_to_qstring(const gint64 size)
{
    return gchar_free_to_qstring(
                format_size(size, format_size_unit_bytes|format_size_prefix_si));
}

const QString time_t_to_qstring(time_t ti_time)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
    QDateTime date_time = QDateTime::fromSecsSinceEpoch(qint64(ti_time));
#else
    QDateTime date_time = QDateTime::fromTime_t(uint(ti_time));
#endif
    QString time_str = date_time.toLocalTime().toString("yyyy-MM-dd hh:mm:ss");
    return time_str;
}

QString html_escape(const QString plain_string) {
    return plain_string.toHtmlEscaped();
}


void smooth_font_size(QFont &font) {
    QFontDatabase fdb;
    QList<int> size_list = fdb.smoothSizes(font.family(), font.styleName());

    if (size_list.size() < 2) return;

    int last_size = size_list.takeFirst();
    foreach (int cur_size, size_list) {
        if (font.pointSize() > last_size && font.pointSize() <= cur_size) {
            font.setPointSize(cur_size);
            return;
        }
        last_size = cur_size;
    }
}

bool qActionLessThan(const QAction * a1, const QAction * a2) {
    return a1->text().compare(a2->text()) < 0;
}

bool qStringCaseLessThan(const QString &s1, const QString &s2)
{
    return s1.compare(s2, Qt::CaseInsensitive) < 0;
}

// https://stackoverflow.com/questions/3490336/how-to-reveal-in-finder-or-show-in-explorer-with-qt
void desktop_show_in_folder(const QString file_path)
{
    bool success = false;

#if defined(Q_OS_WIN)
    QString path = QDir::toNativeSeparators(file_path);
    QStringList explorer_args = QStringList() << "/select," + path;
    success = QProcess::startDetached("explorer.exe", explorer_args);
#elif defined(Q_OS_MAC)
    QStringList script_args;
    QString escaped_path = file_path;

    escaped_path.replace('"', "\\\"");
    script_args << "-e"
               << QString("tell application \"Finder\" to reveal POSIX file \"%1\"")
                                     .arg(escaped_path);
    if (QProcess::execute("/usr/bin/osascript", script_args) == 0) {
        success = true;
        script_args.clear();
        script_args << "-e"
                   << "tell application \"Finder\" to activate";
        QProcess::execute("/usr/bin/osascript", script_args);
    }
#else
    // Is there a way to highlight the file using xdg-open?
#endif
    if (!success) { // Last resort
        QFileInfo file_info = file_path;
        QDesktopServices::openUrl(QUrl::fromLocalFile(file_info.dir().absolutePath()));
    }
}

bool rect_on_screen(const QRect &rect)
{
    foreach (QScreen *screen, qApp->screens()) {
        if (screen->availableGeometry().contains(rect)) {
            return true;
        }
    }

    return false;
}

void set_action_shortcuts_visible_in_context_menu(QList<QAction *> actions)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0) && QT_VERSION < QT_VERSION_CHECK(5, 13, 0)
    // For QT_VERSION >= 5.13.0 we call styleHints()->setShowShortcutsInContextMenus(true)
    // in WiresharkApplication.
    // QTBUG-71471
    // QTBUG-61181
    foreach (QAction *action, actions) {
        action->setShortcutVisibleInContextMenu(true);
    }
#else
    Q_UNUSED(actions)
#endif
}

QString make_filter_based_on_rtpstream_id(QVector<rtpstream_id_t *> ids)
{
    QStringList stream_filters;
    QString filter;

    foreach(rtpstream_id_t *id, ids) {
        QString ip_proto = id->src_addr.type == AT_IPv6 ? "ipv6" : "ip";
        stream_filters << QString("(%1.src==%2 && udp.srcport==%3 && %1.dst==%4 && udp.dstport==%5 && rtp.ssrc==0x%6)")
                         .arg(ip_proto) // %1
                         .arg(address_to_qstring(&id->src_addr)) // %2
                         .arg(id->src_port) // %3
                         .arg(address_to_qstring(&id->dst_addr)) // %4
                         .arg(id->dst_port) // %5
                         .arg(id->ssrc, 0, 16);
    }
    if (stream_filters.length() > 0) {
        filter = stream_filters.join(" || ");
    }

    return filter;
}

QString make_filter_based_on_rtpstream_info(QVector<rtpstream_info_t *> streams)
{
    QVector<rtpstream_id_t *> ids;

    foreach(rtpstream_info_t *stream_info, streams) {
        ids << &stream_info->id;
    }

    return make_filter_based_on_rtpstream_id(ids);
}

