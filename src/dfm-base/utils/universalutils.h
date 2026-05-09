// SPDX-FileCopyrightText: 2020 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef UNIVERSALUTILS_H
#define UNIVERSALUTILS_H

#include <QString>
#include <QUrl>

namespace dfmbase {

class UniversalUtils
{
public:
    static QString covertUrlToLocalPath(const QString &url)
    {
        if (url.startsWith("/"))
            return url;
        else
            return QUrl(QUrl::fromPercentEncoding(url.toUtf8())).toLocalFile();
    }
};

} // namespace dfmbase

#endif // UNIVERSALUTILS_H
