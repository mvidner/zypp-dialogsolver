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
#include "graphtreelabel.h"
#include "graphtree_defines.h"
#include <qpainter.h>

GraphTreeLabel::GraphTreeLabel(const QString&text, const QString&_nodename,const QRect&r,QGraphicsScene*c)
    : QGraphicsRectItem(QRectF(r), NULL /*parent*/, c)
    , StoredDrawParams()
{
    m_Nodename = _nodename;
    m_SourceNode = QString::null;
    setText(0,text);
    m_Label = text;
    setPosition(0, DrawParams::TopCenter);
    drawFrame(true);
}

GraphTreeLabel::~GraphTreeLabel()
{
}

const QString&GraphTreeLabel::nodename()const
{
    return m_Nodename;
}

int GraphTreeLabel::type()const
{
    return GRAPHTREE_LABEL;
}

void GraphTreeLabel::setBgColor(const QColor&c)
{
    _backColor=c;
}

void GraphTreeLabel::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPainter & p = *painter;
    QRectF r = rect();
/*
    p.setPen(blue);
    p.drawRect(r);
*/
    RectDrawing d(r);
    d.drawBack(&p,this);
    d.drawField(&p, 0, this);
    d.drawField(&p, 1, this);
}

void GraphTreeLabel::setSelected(bool s)
{
    QGraphicsRectItem::setSelected(s);
    StoredDrawParams::setSelected(s);
    update();
}

const QString&GraphTreeLabel::source()const
{
    return m_SourceNode;
}

const QString&GraphTreeLabel::label()const
{
    return m_Label;
}


void GraphTreeLabel::setSource(const QString&_s)
{
    m_SourceNode=_s;
}

GraphEdge::GraphEdge(QGraphicsScene*c)
    : QGraphicsPathItem(NULL /*parent*/, c)
{
}

GraphEdge::~GraphEdge()
{
}

void GraphEdge::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QGraphicsPathItem::paint(painter, option, widget);
}

int GraphEdge::type()const
{
    return GRAPHTREE_LINE;
}

GraphEdgeArrow::GraphEdgeArrow(GraphEdge*_parent, QGraphicsScene*c)
    : QGraphicsPolygonItem(NULL /*parent*/, c)
    , _edge(_parent)
{
}

void GraphEdgeArrow::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QGraphicsPolygonItem::paint(painter, option, widget);
}

int GraphEdgeArrow::type()const
{
    return GRAPHTREE_ARROW;
}

GraphEdge*GraphEdgeArrow::edge()
{
    return _edge;
}

/* taken from KCacheGrind project */
QPixmap*GraphMark::_p=0;

GraphMark::GraphMark(GraphTreeLabel*n, QGraphicsScene*c)
    : QGraphicsRectItem(NULL /*parent*/, c)
{
    if (!_p) {

        int d = 5;
        float v1 = 130.0, v2 = 10.0, v = v1, f = 1.03;

        // calculate pix size
        QRect r(0, 0, 30, 30);
        while (v>v2) {
            r.setRect(r.x()-d, r.y()-d, r.width()+2*d, r.height()+2*d);
            v /= f;
        }

        _p = new QPixmap(r.size());
        _p->fill(Qt::white);
        QPainter p(_p);
        p.setPen(Qt::NoPen);

        r.moveBy(-r.x(), -r.y());

        while (v<v1) {
            v *= f;
            p.setBrush(QColor(265-(int)v, 265-(int)v, 265-(int)v));

            p.drawRect(QRect(r.x(), r.y(), r.width(), d));
            p.drawRect(QRect(r.x(), r.bottom()-d, r.width(), d));
            p.drawRect(QRect(r.x(), r.y()+d, d, r.height()-2*d));
            p.drawRect(QRect(r.right()-d, r.y()+d, d, r.height()-2*d));

            r.setRect(r.x()+d, r.y()+d, r.width()-2*d, r.height()-2*d);
        }
    }

    setRect(0, 0, _p->width(), _p->height());
    setPos(n->rect().center().x() - _p->width()/2,
           n->rect().center().y() - _p->height()/2);
}

GraphMark::~ GraphMark()
{
}

bool GraphMark::hit(const QPoint&)const
{
    return false;
}

int GraphMark::type()const
{
    return GRAPHTREE_MARK;
}

void GraphMark::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QPainter & p = *painter;
    p.drawPixmap( int(x()), int(y()), *_p );
}
