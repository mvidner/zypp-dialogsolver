/***************************************************************************
 *   Copyright (C) 2006-2007 by Rajko Albrecht                             *
 *   ral@alwins-world.de                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/
#ifndef RESTREEWIDGET_H
#define RESTREEWIDGET_H


#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>
#include <qtabwidget.h>
#include <q3listview.h>
#include <q3textbrowser.h>
#include "zypp/Resolver.h"
#include <zypp/PoolItem.h>
#include <zypp/Resolvable.h>


class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class ResGraphView;
class QSplitter;
class QTextBrowser;
class Q3VBox;
class Q3HBox;
class QComboBox;
class QLabel;

namespace svn {
    class LogEntry;
    class Client;
}

class ResTreeWidget : public QWidget
{
    Q_OBJECT

public:
    ResTreeWidget(QWidget* parent = 0, zypp::solver::detail::Resolver_Ptr r = NULL, const char* name = 0, Qt::WFlags fl = 0 );
    ~ResTreeWidget();

    QSplitter* m_Splitter;
    ResGraphView* m_RevGraphView;
    zypp::solver::detail::Resolver_Ptr resolver;
    void selectItem(const zypp::PoolItem item);    

    void dumpRevtree();

protected:
    QVBoxLayout* ResTreeWidgetLayout;
    Q3VBox *descriptionBox;
    Q3HBox *searchBox;
    QLabel *searchLabel;
    QTabWidget *tabWidget;
    Q3ListView *installListView;
    Q3ListView *installedListView;    
    Q3TextBrowser* m_Detailstext;
    QComboBox *resolvableList;
    void selectItem(const QString & itemString);        


protected slots:
    virtual void setDetailText(const QString&, const zypp::PoolItem);
    void itemSelected( Q3ListViewItem* item);    
    void slotComboActivated( const QString &s );
    
private:
    QPixmap image0;

};

#endif // RESTREEWIDGET_H
