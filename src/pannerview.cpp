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
#include "pannerview.h"
#include <qpainter.h>
#include <qevent.h>

PannerView::PannerView(QWidget* parent, const char* name)
    : QGraphicsView(parent)
{
    setObjectName(name);
    setWindowFlags(Qt::WNoAutoErase | Qt::WStaticContents);
    m_Moving = false;
    viewport()->setBackgroundMode(Qt::NoBackground);
    setBackgroundMode(Qt::NoBackground);
}

PannerView::~PannerView()
{
}

void PannerView::drawItems(QPainter * p,
                           int numItems,
                           QGraphicsItem * items[],
                           const QStyleOptionGraphicsItem options[])
{
    p->save();
    QGraphicsView::drawItems(p, numItems, items, options);
    p->restore();
    if (m_ZoomRect.isValid()) {
        p->setPen(Qt::darkRed);
        p->drawRect(m_ZoomRect);
        p->setPen( Qt::red);
        p->drawRect(QRect(m_ZoomRect.x()+1, m_ZoomRect.y()+1,
            m_ZoomRect.width()-2, m_ZoomRect.height()-2));
    }
}

void PannerView::drawItems(QPainter* p)
{
    drawItems(p, 0, NULL, NULL);
}


void PannerView::setZoomRect(const QRect& theValue)
{
    QRect oldRect = m_ZoomRect;
    m_ZoomRect = theValue;
    // http://doc.qt.digia.com/4.0/q3scrollview.html#updateContents-2
    //FIXME    updateContents(oldRect);
    //FIXME    updateContents(m_ZoomRect);
}

/*!
    \fn PannerView::contentsMouseMoveEvent(QMouseEvent* e)
 */
void PannerView::contentsMouseMoveEvent(QMouseEvent* e)
{
    if (m_Moving) {
        emit zoomRectMoved(e->pos().x() - m_LastPos.x(), e->pos().y() - m_LastPos.y());
        m_LastPos = e->pos();
    }
}

/*!
    \fn PannerView::contentsMousePressEvent(QMouseEvent* e)
 */
void PannerView::contentsMousePressEvent(QMouseEvent* e)
{
    if (m_ZoomRect.isValid()) {
        if (!m_ZoomRect.contains(e->pos())) {
            emit zoomRectMoved(e->pos().x() - m_ZoomRect.center().x(),
                e->pos().y() - m_ZoomRect.center().y());
        }
        m_Moving = true;
        m_LastPos = e->pos();
    }
}

/*!
    \fn PannerView::contentsMouseReleaseEvent(QMouseEvent*)
 */
void PannerView::contentsMouseReleaseEvent(QMouseEvent*)
{
    m_Moving = false;
    emit zoomRectMoveFinished();
}

/*!
    \fn PannerView::updateCurrentRect()
 */
void PannerView::updateCurrentRect()
{
    if (m_ZoomRect.isValid()) {
        // FIXME updateContents(m_ZoomRect);
    }
}

#include "pannerview.moc"

