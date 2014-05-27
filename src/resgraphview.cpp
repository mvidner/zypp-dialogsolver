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
#include "QZyppSolverDialog.h"
#include "resgraphview.h"
#include "graphtreelabel.h"
#include "pannerview.h"
#include "graphtree_defines.h"
#include <QProcess>
#include <qtooltip.h>
#include <qwmatrix.h>
#include <qpainter.h>
#include <qregexp.h>
#include <qmessagebox.h>
#include <qfiledialog.h>
#include <qcursor.h>
#include <qdesktopwidget.h>
#include <qtextstream.h>
#include <qevent.h>

#include <math.h>

#include <zypp/Pathname.h>
#include <zypp/TmpPath.h>
#include "zypp/ZYppFactory.h"
#include "getText.h"

#define LABEL_WIDTH 160
#define LABEL_HEIGHT 90

using namespace zypp;

static int globalDirection = 0;

ResGraphView::ResGraphView(QWidget * parent, const char * name, Qt::WFlags f)
 : Q3CanvasView(parent,name,f)
{
    m_Canvas = 0L;
    dotTmpFile = 0;
    m_Selected = 0;
    renderProcess = 0;
    m_Marker = 0;
    m_CompleteView = new PannerView(this);
    
    m_CompleteView->setVScrollBarMode(Q3ScrollView::AlwaysOff);
    m_CompleteView->setHScrollBarMode(Q3ScrollView::AlwaysOff);
    m_CompleteView->raise();
    m_CompleteView->hide();
    connect(this, SIGNAL(contentsMoving(int,int)),
            this, SLOT(contentsMovingSlot(int,int)));
    connect(m_CompleteView, SIGNAL(zoomRectMoved(int,int)),
            this,           SLOT(zoomRectMoved(int,int)));
    connect(m_CompleteView, SIGNAL(zoomRectMoveFinished()),
            this,           SLOT(zoomRectMoveFinished()));
    m_LastAutoPosition = TopLeft;
    _isMoving = false;
    _noUpdateZoomerPos = false;
    m_LabelMap[""]="";
    _lastSelectedItem = "";
}

ResGraphView::~ResGraphView()
{
    setCanvas(0);
    delete m_Canvas;
    delete dotTmpFile;
    delete m_CompleteView;
    delete renderProcess;
}

void ResGraphView::showText(const QString&s)
{
    clear();
    m_Canvas = new Q3Canvas(QApplication::desktop()->width(),
                        QApplication::desktop()->height());

    Q3CanvasText* t = new Q3CanvasText(s, m_Canvas);
    t->move(5, 5);
    t->show();
    center(0,0);
    setCanvas(m_Canvas);
    m_Canvas->update();
    m_CompleteView->hide();
}

void ResGraphView::clear()
{
    if (m_Selected) {
        m_Selected->setSelected(false);
        m_Selected=0;
    }
    if (m_Marker) {
        m_Marker->hide();
        delete m_Marker;
        m_Marker=0;
    }
    if (!m_Canvas) return;
    delete m_Canvas;
    m_Canvas = 0;
    setCanvas(0);
    m_CompleteView->setCanvas(0);
}

void ResGraphView::beginInsert()
{
    viewport()->setUpdatesEnabled(false);
}

void ResGraphView::endInsert()
{
    if (m_Canvas) {
        _cvZoomH = 0;
        _cvZoomW = 0;	
        updateSizes();
        m_Canvas->update();
    }
    viewport()->setUpdatesEnabled(true);
}

void ResGraphView::dotExit()
{
    dotOutput+=QString::fromLocal8Bit(renderProcess->readAllStandardOutput());

    // remove line breaks when lines to long
    QRegExp endslash("\\\\\\n");
    dotOutput.replace(endslash,"");

#if 0
    QFile *dot = new QFile("dotoutput");
    dot->open(IO_ReadWrite);
    QTextStream stream(dot);
    stream << dotOutput << flush;
    dot->close();
#endif

    double scale = 1.0, scaleX = 1.0, scaleY = 1.0;
    double dotWidth, dotHeight;
    QTextStream* dotStream;
    dotStream = new QTextStream(&dotOutput);
    QString line,cmd;
    int lineno=0;
    clear();
    beginInsert();
    /* mostly taken from kcachegrind */
    // http://www.graphviz.org/content/output-formats#dplain
    // The lower left corner of the drawing is at the origin.
    while (1) {
        line = dotStream->readLine();
        if (line.isNull()) break;
        lineno++;
        if (line.isEmpty()) continue;
        QTextStream lineStream(&line);
        lineStream >> cmd;
        if (cmd == "stop") {break; }

        if (cmd == "graph") {
            lineStream >> scale >> dotWidth >> dotHeight;
            scaleX = scale * 60; scaleY = scale * 100;
            int w = (int)(scaleX * dotWidth);
            int h = (int)(scaleY * dotHeight);

            _xMargin = 50;
            if (w < QApplication::desktop()->width())
                _xMargin += (QApplication::desktop()->width()-w)/2;
            _yMargin = 50;
            if (h < QApplication::desktop()->height())
                _yMargin += (QApplication::desktop()->height()-h)/2;
            m_Canvas = new Q3Canvas(int(w+2*_xMargin), int(h+2*_yMargin));
            continue;
        }
        if ((cmd != "node") && (cmd != "edge")) {
//            kdWarning() << "Ignoring unknown command '" << cmd << "' from dot ("
//                << dotTmpFile->name() << ":" << lineno << ")" << endl;
            continue;
        }
        if (cmd=="node") {
            QString nodeName, label;
            QString _x,_y,_w,_h;
            double x, y, width, height;
            lineStream >> nodeName >> _x >> _y >> _w >> _h;
            x=_x.toDouble();
            y=_y.toDouble();
            width=_w.toDouble();
            height=_h.toDouble();
            // better here 'cause dot may scramble utf8 labels so we regenerate it better
            // and do not read it in.
            label = getLabelstring(nodeName);
            int xx = (int)(scaleX * x + _xMargin);
            int yy = (int)(scaleY * (dotHeight - y) + _yMargin);
            int w = (int)(scaleX * width);
            int h = (int)(scaleY * height);
            QRect r(xx-w/2, yy-h/2, w, h);
            GraphTreeLabel*t=new GraphTreeLabel(label,nodeName,r,m_Canvas);
            if (isStart(nodeName)) {
                ensureVisible(r.x(),r.y());
            }
            t->setBgColor(getBgColor(nodeName));
            t->setZ(1.0);
            t->show();
            m_NodeList[nodeName]=t;
        } else {
            QString node1Name, node2Name, label;
            QString _x,_y;
            double x, y;
            Q3PointArray pa;
            int points, i;
            lineStream >> node1Name >> node2Name;
            lineStream >> points;
            pa.resize(points);
            for (i=0;i<points;++i) {
                if (lineStream.atEnd()) break;
                lineStream >> _x >> _y;
                x=_x.toDouble();
                y=_y.toDouble();
                int xx = (int)(scaleX * x + _xMargin);
                int yy = (int)(scaleY * (dotHeight - y) + _yMargin);

                if (0) qDebug("   P %d: ( %f / %f ) => ( %d / %d)",
                    i, x, y, xx, yy);
                pa.setPoint(i, xx, yy);
            }
            if (i < points) {
                qDebug("CallGraphView: Can't read %d spline points (%d)",
                    points,  lineno);
                continue;
            }

            GraphEdge * n = new GraphEdge(m_Canvas);
            QColor arrowColor = Qt::black;
	    if (isRecommended(node2Name))
		arrowColor = Qt::green;
            n->setPen(QPen(arrowColor,1));
            n->setControlPoints(pa,false);
            n->setZ(0.5);
            n->show();

            /* arrow */
            QPoint arrowDir;
            int indexHead = -1;

            QMap<QString,GraphTreeLabel*>::Iterator it;
            it = m_NodeList.find(node2Name);
            if (it!=m_NodeList.end()) {
                it.data()->setSource(node1Name);
            }
            it = m_NodeList.find(node1Name);
            if (it!=m_NodeList.end()) {
                GraphTreeLabel*tlab = it.data();
                if (tlab) {
                    QPoint toCenter = tlab->rect().center();
                    int dx0 = pa.point(0).x() - toCenter.x();
                    int dy0 = pa.point(0).y() - toCenter.y();
                    int dx1 = pa.point(points-1).x() - toCenter.x();
                    int dy1 = pa.point(points-1).y() - toCenter.y();
                    if (dx0*dx0+dy0*dy0 > dx1*dx1+dy1*dy1) {
                    // start of spline is nearer to call target node
                        indexHead=-1;
                        while(arrowDir.isNull() && (indexHead<points-2)) {
                            indexHead++;
                            arrowDir = pa.point(indexHead) - pa.point(indexHead+1);
                        }
                    }
                }
            }

            if (arrowDir.isNull()) {
                indexHead = points;
                // sometimes the last spline points from dot are the same...
                while(arrowDir.isNull() && (indexHead>1)) {
                    indexHead--;
                    arrowDir = pa.point(indexHead) - pa.point(indexHead-1);
                }
            }
            if (!arrowDir.isNull()) {
                arrowDir *= 10.0/sqrt(double(arrowDir.x()*arrowDir.x() +
                    arrowDir.y()*arrowDir.y()));
                Q3PointArray a(3);
                a.setPoint(0, pa.point(indexHead) + arrowDir);
                a.setPoint(1, pa.point(indexHead) + QPoint(arrowDir.y()/2,
                    -arrowDir.x()/2));
                a.setPoint(2, pa.point(indexHead) + QPoint(-arrowDir.y()/2,
                    arrowDir.x()/2));
                GraphEdgeArrow* aItem = new GraphEdgeArrow(n,m_Canvas);
                aItem->setPoints(a);
                aItem->setBrush(arrowColor);
                aItem->setZ(1.5);
                aItem->show();
//                sItem->setArrow(aItem);
            }
        }
    }
    if (!m_Canvas) {
        QString s = i18n("Error running the graph layouting tool.\n");
        s += i18n("Please check that 'dot' is installed (package GraphViz).");
        showText(s);
    } else {
        setCanvas(m_Canvas);
        m_CompleteView->setCanvas(m_Canvas);
    }
    endInsert();
    renderProcess=0;
    selectItem(_lastSelectedItem); // Show the selected item (Could be set via API meanwhile)
}

bool ResGraphView::isStart(const QString&nodeName)const
{
    bool res = false;
    trevTree::ConstIterator it;
    it = m_Tree.find(nodeName);
    if (it==m_Tree.end()) {
        return res;
    }

    return res;
}


QColor ResGraphView::getBgColor(const QString&nodeName)const
{
    trevTree::ConstIterator it;
    it = m_Tree.find(nodeName);
    if (it==m_Tree.end())
        return Qt::white;

    if (it.data().item.status().staysInstalled())
	return QColor(Qt::green).lighter(190);	
	
    if (it.data().item->isKind( ResKind::product )) 
	return QColor(Qt::magenta).lighter(190);
    if (it.data().item->isKind( ResKind::pattern )) 
	return QColor(Qt::blue).lighter(190);
    if (it.data().item->isKind( ResKind::patch )) 
	return QColor(Qt::yellow).lighter(190);
    
    return Qt::white;
}

const QString&ResGraphView::getLabelstring(const QString&nodeName)
{
    QMap<QString,QString>::ConstIterator nIt;
    nIt = m_LabelMap.find(nodeName);
    if (nIt!=m_LabelMap.end()) {
        return nIt.data();
    }
    trevTree::ConstIterator it1;
    it1 = m_Tree.find(nodeName);
    if (it1==m_Tree.end()) {
        return m_LabelMap[""];
    }

    m_LabelMap[nodeName]=it1.data().item->name().c_str();
    m_LabelMap[nodeName]+= "-";
    m_LabelMap[nodeName]+= it1.data().item->edition().asString().c_str();
    m_LabelMap[nodeName]+= ".";
    m_LabelMap[nodeName]+= it1.data().item->arch().asString().c_str();

    return m_LabelMap[nodeName];
}


bool ResGraphView::isRecommended(const QString&nodeName)const
{
    trevTree::ConstIterator it;
    it = m_Tree.find(nodeName);
    if (it==m_Tree.end())
        return false;

    zypp::solver::detail::ItemCapKind dueto = it.data().dueto;

    if (dueto.capKind == Dep::SUPPLEMENTS
	|| dueto.capKind == Dep::RECOMMENDS) {
	return true;
    }
    return false;
}


void ResGraphView::dumpRevtree()
{

    delete dotTmpFile;
    clear();
    dotOutput = "";
    QString filename = "/tmp/tmp.dot"; //zypp::filesystem::TmpFile().path().asString();
    dotTmpFile = new QFile(filename);

    if (!dotTmpFile->open(IO_ReadWrite)) {
        showText(i18n("Could not open tempfile %1 for writing.").arg(filename));
        return;
    }
    
    QTextStream stream(dotTmpFile);    

    stream << "digraph \"callgraph\" {\n";
    stream << "  bgcolor=\"transparent\";\n";
    int dir = globalDirection;
    stream << QString("  rankdir=\"");
    switch (dir) {
        case 3:
            stream << "TB";
        break;
        case 2:
            stream << "RL";
        break;
        case 1:
            stream << "BT";
        break;
        case 0:
        default:
            stream << "LR";
        break;
    }
    stream << "\";\n";

    //*stream << QString("  overlap=false;\n  splines=true;\n");

    ResGraphView::trevTree::ConstIterator it1;
    for (it1=m_Tree.begin();it1!=m_Tree.end();++it1) {
        stream << "  " << it1.key()
            << "[ "
            << "shape=box, "
            << "label=\""<<getLabelstring(it1.key())<<"\","
            << "];\n";
        for (int j=0;j<it1.data().targets.count();++j) {
            stream<<"  "<<it1.key().latin1()<< " "
                << "->"<<" "<<it1.data().targets[j].key
                << " [fontsize=10,style=\"solid\""
		  << " color=\"" << (isRecommended(it1.data().targets[j].key) ? "green" : "black") 
		<< "\"];\n";
        }
    }
    stream << "}\n"<<flush;

    renderProcess = new QProcess();
    connect(renderProcess, SIGNAL(finished(int)),
            this,          SLOT(dotExit()));
    connect(renderProcess, SIGNAL(error(QProcess::ProcessError)),
            this,          SLOT(dotError()));
    renderProcess->setEnvironment(QStringList() << "LANG=C");

    renderProcess->start("dot", QStringList() << filename << "-Tplain");
}

void ResGraphView::dotError()
{
        QString error = i18n("Could not start process \"%1\".").arg("dot");
        showText(error);
        renderProcess=0;
        //delete renderProcess;<
}

QString ResGraphView::toolTip(const QString&_nodename,bool full)const
{
    QString res = QString::null;
    trevTree::ConstIterator it;
    it = m_Tree.find(_nodename);
    if (it==m_Tree.end()) {
        return res;
    }
    QStringList sp = QStringList::split("\n",(QString)(it.data().item->description().c_str()));
    QString sm;
    if (sp.count()==0) {
        sm = it.data().item->description().c_str();
    } else {
        if (!full) {
            sm = sp[0]+"...";
        } else {
            for (int j = 0; j<sp.count(); ++j) {
                if (j>0) sm+="<br>";
                sm+=sp[j];
            }
        }
    }
    if (!full && sm.length()>50) {
        sm.truncate(47);
        sm+="...";
    }

    sm = QString::fromUtf8(sm);
    
    static QString csep = "</td><td>";
    static QString rend = "</td></tr>";
    static QString rstart = "<tr><td>";
    res = QString("<html><body>");

    if (!full) {
	QString edition = it.data().item->edition().asString().c_str();
	edition += ".";
	edition += it.data().item->arch().asString().c_str();
        res+=QString("<b>%1</b>").arg(it.data().item->name().c_str());
        res += i18n("<br>Kind: %1<br>Version: %2<br>Source: %3</html>")
            .arg(it.data().item->kind().asString().c_str())
            .arg(edition)
            .arg(it.data().item->repository().info().alias().c_str());
    } else {
        res+="<table><tr><th colspan=\"2\"></th></tr>";
        res+=rstart + i18n("<b>Name</b>%1%2%3").arg(csep).arg(it.data().item->name().c_str()).arg(rend);	
        res+=rstart + i18n("<b>Kind</b>%1%2%3").arg(csep).arg(it.data().item->kind().asString().c_str()).arg(rend);
	QString edition = it.data().item->edition().asString().c_str();
	edition += ".";
	edition += it.data().item->arch().asString().c_str();
        res+=rstart+i18n("<b>Version</b>%1%2%3").arg(csep).arg(edition).arg(rend);
        res+=rstart+i18n("<b>Source</b>%1%2%3").arg(csep).arg(it.data().item->repository().info().alias().c_str()).arg(rend);
        res+=rstart+i18n("<b>Description</b>%1%2%3").arg(csep).arg(sm).arg(rend);
        res+="</table></body></html>";
    }
    return res;
}

void ResGraphView::updateSizes(QSize s)
{
    if (!m_Canvas) return;
    if (s == QSize(0,0)) s = size();

    // the part of the canvas that should be visible
    int cWidth  = m_Canvas->width()  - 2*_xMargin + 100;
    int cHeight = m_Canvas->height() - 2*_yMargin + 100;

    // hide birds eye view if no overview needed
    if (((cWidth < s.width()) && cHeight < s.height())||m_NodeList.count()==0) {
      m_CompleteView->hide();
      return;
    }

    m_CompleteView->show();

    // first, assume use of 1/3 of width/height (possible larger)
    double zoom = .33 * s.width() / cWidth;
    if (zoom * cHeight < .33 * s.height()) zoom = .33 * s.height() / cHeight;

    // fit to widget size
    if (cWidth  * zoom  > s.width())   zoom = s.width() / (double)cWidth;
    if (cHeight * zoom  > s.height())  zoom = s.height() / (double)cHeight;

    // scale to never use full height/width
    zoom = zoom * 3/4;

    // at most a zoom of 1/3
    if (zoom > .33) zoom = .33;

    double cvZoomW = zoom;
    double cvZoomH = zoom;

    // show at least 1/20 of the frame
    if (cWidth  * cvZoomW  < s.width()/20)   cvZoomW = s.width() / (double)cWidth / 20;
    if (cHeight * cvZoomH  < s.height()/20)  cvZoomH = s.height() / (double)cHeight / 20;

    if (cvZoomW != _cvZoomW
	|| cvZoomH != _cvZoomH) {
      _cvZoomW = cvZoomW; 
      _cvZoomH = cvZoomH;    
	
      if (0) qDebug("Canvas Size: %dx%d, Visible: %dx%d, ZoomH: %f, ZoomW: %f",
            m_Canvas->width(), m_Canvas->height(),
            cWidth, cHeight, cvZoomH, cvZoomW);

      QWMatrix wm;
      wm.scale( _cvZoomW, _cvZoomH );
      m_CompleteView->setWorldMatrix(wm);

      // make it a little bigger to compensate for widget frame
      m_CompleteView->resize(int(cWidth * _cvZoomW) + 4,
                            int(cHeight * _cvZoomH) + 4);

      // update ZoomRect in completeView
      contentsMovingSlot(contentsX(), contentsY());
    }

    m_CompleteView->setContentsPos(int(_cvZoomW*(_xMargin-50)),
                  int(_cvZoomH*(_yMargin-50)));
    updateZoomerPos();
}

void ResGraphView::updateZoomerPos()
{
    int cvW = m_CompleteView->width();
    int cvH = m_CompleteView->height();
    int x = width()- cvW - verticalScrollBar()->width()    -2;
    int y = height()-cvH - horizontalScrollBar()->height() -2;

    QPoint oldZoomPos = m_CompleteView->pos();
    QPoint newZoomPos = QPoint(0,0);

#if 0
    ZoomPosition zp = _zoomPosition;
    if (zp == Auto) {
#else
    ZoomPosition zp = m_LastAutoPosition;
#endif
    QPoint tl1Pos = viewportToContents(QPoint(0,0));
    QPoint tl2Pos = viewportToContents(QPoint(cvW,cvH));
    QPoint tr1Pos = viewportToContents(QPoint(x,0));
    QPoint tr2Pos = viewportToContents(QPoint(x+cvW,cvH));
    QPoint bl1Pos = viewportToContents(QPoint(0,y));
    QPoint bl2Pos = viewportToContents(QPoint(cvW,y+cvH));
    QPoint br1Pos = viewportToContents(QPoint(x,y));
    QPoint br2Pos = viewportToContents(QPoint(x+cvW,y+cvH));
    int tlCols = m_Canvas->collisions(QRect(tl1Pos,tl2Pos)).count();
    int trCols = m_Canvas->collisions(QRect(tr1Pos,tr2Pos)).count();
    int blCols = m_Canvas->collisions(QRect(bl1Pos,bl2Pos)).count();
    int brCols = m_Canvas->collisions(QRect(br1Pos,br2Pos)).count();
    int minCols = tlCols;
    zp = m_LastAutoPosition;
    switch(zp) {
        case TopRight:    minCols = trCols; break;
        case BottomLeft:  minCols = blCols; break;
        case BottomRight: minCols = brCols; break;
        default:
        case TopLeft:     minCols = tlCols; break;
    }
    if (minCols > tlCols) { minCols = tlCols; zp = TopLeft; }
    if (minCols > trCols) { minCols = trCols; zp = TopRight; }
    if (minCols > blCols) { minCols = blCols; zp = BottomLeft; }
    if (minCols > brCols) { minCols = brCols; zp = BottomRight; }

    m_LastAutoPosition = zp;
#if 0
    }
#endif
    switch(zp) {
    case TopRight:
        newZoomPos = QPoint(x,0);
        break;
    case BottomLeft:
        newZoomPos = QPoint(0,y);
        break;
    case BottomRight:
        newZoomPos = QPoint(x,y);
        break;
    default:
    break;
    }
    if (newZoomPos != oldZoomPos) m_CompleteView->move(newZoomPos);
}

void ResGraphView::contentsMovingSlot(int x,int y)
{
    QRect z(int(x * _cvZoomW), int(y * _cvZoomH),
        int(visibleWidth() * _cvZoomW)-1, int(visibleHeight() * _cvZoomH)-1);
    if (0) qDebug("moving: (%d,%d) => (%d/%d - %dx%d)",
                x, y, z.x(), z.y(), z.width(), z.height());
    m_CompleteView->setZoomRect(z);
    if (!_noUpdateZoomerPos) {
        updateZoomerPos();
    }
}

void ResGraphView::zoomRectMoved(int dx,int dy)
{
  if (leftMargin()>0) dx = 0;
  if (topMargin()>0) dy = 0;
  _noUpdateZoomerPos = true;
  scrollBy(int(dx/_cvZoomW),int(dy/_cvZoomH));
  _noUpdateZoomerPos = false;
}

void ResGraphView::zoomRectMoveFinished()
{
#if 0
    if (_zoomPosition == Auto)
#endif
    updateZoomerPos();
}


bool ResGraphView::event(QEvent *event)
{
     if (event->type() == QEvent::ToolTip) {
         QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
	 QPoint cPos = viewportToContents(helpEvent->pos());
	 Q3CanvasItemList l = canvas()->collisions(cPos);
	 if (l.count() > 0) {
	     Q3CanvasItem* i = l.first();
	     if (i->rtti() == GRAPHTREE_LABEL) {
		 GraphTreeLabel*tl = (GraphTreeLabel*)i;
		 QString nm = tl->nodename();
		 QString tipStr = toolTip(nm);
		 if (tipStr.length()>0) {
		     QToolTip::showText(helpEvent->globalPos(), tipStr);
		 } else {
		     QToolTip::hideText();
		 }
	     }
	 }
     }
     return QWidget::event(event);
}

void ResGraphView::resizeEvent(QResizeEvent*e)
{
    Q3CanvasView::resizeEvent(e);
    if (m_Canvas) updateSizes(e->size());
}

void ResGraphView::makeSelected(GraphTreeLabel*gtl)
{
    if (m_Selected) {
        m_Selected->setSelected(false);
    }
    m_Selected=gtl;
    if (m_Marker) {
        m_Marker->hide();
        delete m_Marker;
        m_Marker=0;
    }
    if (gtl) {
        m_Marker = new GraphMark(gtl,m_Canvas);
        m_Marker->setZ(-1);
        m_Marker->show();
        m_Selected->setSelected(true);
    }
    m_Canvas->update();
    m_CompleteView->updateCurrentRect();
}

void ResGraphView::contentsMouseDoubleClickEvent ( QMouseEvent * e )
{
    setFocus();
    if (e->button() == Qt::LeftButton) {
        Q3CanvasItemList l = canvas()->collisions(e->pos());
        if (l.count()>0) {
            Q3CanvasItem* i = l.first();
            if (i->rtti()==GRAPHTREE_LABEL) {
		trevTree::ConstIterator it;
		it = m_Tree.find(((GraphTreeLabel*)i)->nodename());
		if (it!=m_Tree.end()) {
		    zypp::ResPool pool( zypp::getZYpp()->pool() );
		    const QCursor oldCursor = cursor ();
		    setCursor (Qt::WaitCursor);

		    QZyppSolverDialog *dialog = new QZyppSolverDialog(it.data().item);
		    dialog->setCaption(getLabelstring(((GraphTreeLabel*)i)->nodename()));
		    dialog->setMinimumSize ( 600, 600 );
		    setCursor (oldCursor);
		    dialog->show();
		    dialog->raise();
		    dialog->activateWindow();
		    dialog->selectItem(it.data().item);
		    pool.proxy().restoreState(); // Restore old state
		}
            }
        }
    }
}

void ResGraphView::contentsMousePressEvent ( QMouseEvent * e )
{
    setFocus();
    _isMoving = true;
    _lastPos = e->globalPos();
}

void ResGraphView::contentsMouseReleaseEvent ( QMouseEvent * e)
{
    _isMoving = false;
    updateZoomerPos();
    if (e->button() == Qt::LeftButton) {
        Q3CanvasItemList l = canvas()->collisions(e->pos());
        if (l.count()>0) {
            Q3CanvasItem* i = l.first();
            if (i->rtti()==GRAPHTREE_LABEL) {
                makeSelected( (GraphTreeLabel*)i);
		
		trevTree::ConstIterator it;
		it = m_Tree.find(((GraphTreeLabel*)i)->nodename());
		if (it!=m_Tree.end()) {
		    emit dispDetails(toolTip(((GraphTreeLabel*)i)->nodename(),true),
				     it.data().item);
		}
            }
        }
    }    
}

void ResGraphView::contentsMouseMoveEvent ( QMouseEvent * e )
{
    if (_isMoving) {
        int dx = e->globalPos().x() - _lastPos.x();
        int dy = e->globalPos().y() - _lastPos.y();
        _noUpdateZoomerPos = true;
        scrollBy(-dx, -dy);
        _noUpdateZoomerPos = false;
        _lastPos = e->globalPos();
    }
}

void ResGraphView::setNewDirection(int dir)
{
    if (dir<0)dir=3;
    else if (dir>3)dir=0;
    globalDirection = dir;
    dumpRevtree();
}

void ResGraphView::contentsContextMenuEvent(QContextMenuEvent* e)
{
    if (!m_Canvas) return;
    Q3CanvasItemList l = canvas()->collisions(e->pos());
    Q3CanvasItem* i = (l.count() == 0) ? 0 : l.first();
    trevTree::ConstIterator it;

    QAction * unselectItem = NULL;
    QAction * selectItem = NULL;
    QAction * displayDetails = NULL;
    QMenu popup;
    if (i && i->rtti()==GRAPHTREE_LABEL) {
        if (m_Selected == i) {
            unselectItem = popup.addAction(i18n("Unselect item"));
        } else {
            selectItem = popup.addAction(i18n("Select item"));
        }
        popup.insertSeparator();
        displayDetails = popup.addAction(i18n("Display details"));
        popup.insertSeparator();
    }
    QAction * rotateCCW = popup.addAction(i18n("Rotate counter-clockwise"));
    QAction * rotateCW  = popup.addAction(i18n("Rotate clockwise"));
    popup.insertSeparator();
    QAction * saveImage = popup.addAction(i18n("Save tree as png"));

    QAction * r = popup.exec(e->globalPos());
    if (r == NULL)
    {
        return;
    }
    else if (r == rotateCCW)
    {
        int dir = globalDirection;
        setNewDirection(++dir);
    }
    else if (r == rotateCW)
    {
        int dir = globalDirection;
        setNewDirection(--dir);
    }
    else if (r == saveImage)
    {
        QString fn = QFileDialog::getSaveFileName(
            "/home",
            "Images (*.png *.xpm *.jpg)",
            this,
            "save file dialog",
            "Choose a filename to save under" );
        if (!fn.isEmpty()) {
            if (m_Marker) {
                m_Marker->hide();
            }
            if (m_Selected) {
                m_Selected->setSelected(false);
            }
            QPixmap pix(m_Canvas->size());
            QPainter p(&pix);
            m_Canvas->drawArea( m_Canvas->rect(), &p );
            pix.save(fn,"PNG");
            if (m_Marker) {
                m_Marker->show();
            }
            if (m_Selected) {
                m_Selected->setSelected(true);
                m_Canvas->update();
                m_CompleteView->updateCurrentRect();
            }
        }
    }
    else if (r == unselectItem)
    {
        makeSelected(0);
    }
    else if (r == selectItem)
    {
        makeSelected((GraphTreeLabel*)i);
    }
    else if (r == displayDetails)
    {
        it = m_Tree.find(((GraphTreeLabel*)i)->nodename());
        if (it!=m_Tree.end()) {
            emit dispDetails(toolTip(((GraphTreeLabel*)i)->nodename(),true),
                             it.data().item);
        }
    }
}

void ResGraphView::slotClientException(const QString&what)
{
    QMessageBox::critical(0,"Critical",what, QMessageBox::Ok,
			  QMessageBox::Cancel);
}

void ResGraphView::selectItem(const QString & itemString) {
    _lastSelectedItem = itemString;
    QMap<QString,GraphTreeLabel*>::Iterator it;
    for ( it = m_NodeList.begin(); it != m_NodeList.end(); ++it ) {
	GraphTreeLabel*tlab = it.data();
	if (tlab->label() == itemString) break;
    }
    if (it!=m_NodeList.end()) {
	GraphTreeLabel*tlab = it.data();
	makeSelected(tlab);
	trevTree::ConstIterator it;
	it = m_Tree.find(tlab->nodename());
	if (it!=m_Tree.end()) {
	    emit dispDetails(toolTip(tlab->nodename(),true),
			     it.data().item);
	}
	setContentsPos (tlab->x() - int(visibleWidth() *_cvZoomW/2),
			tlab->y() - int(visibleHeight() *_cvZoomH/2));
    }
}

void ResGraphView::init()
{
    setCanvas(0);
    if (m_Canvas)
       delete m_Canvas;
    if (dotTmpFile)
       delete dotTmpFile;
    if (m_CompleteView)
       delete m_CompleteView;
    if (renderProcess)
       delete renderProcess;

    m_Canvas = 0L;
    dotTmpFile = 0;
    m_Selected = 0;
    renderProcess = 0;
    m_Marker = 0;
    m_CompleteView = new PannerView(this);

    m_CompleteView->setVScrollBarMode(Q3ScrollView::AlwaysOff);
    m_CompleteView->setHScrollBarMode(Q3ScrollView::AlwaysOff);
    m_CompleteView->raise();
    m_CompleteView->hide();
    connect(this, SIGNAL(contentsMoving(int,int)),
            this, SLOT(contentsMovingSlot(int,int)));
    connect(m_CompleteView, SIGNAL(zoomRectMoved(int,int)),
            this, SLOT(zoomRectMoved(int,int)));
    connect(m_CompleteView, SIGNAL(zoomRectMoveFinished()),
            this, SLOT(zoomRectMoveFinished()));
    m_LastAutoPosition = TopLeft;
    _isMoving = false;
    _noUpdateZoomerPos = false;
    m_LabelMap[""]="";
    m_Tree.clear();
    m_NodeList.clear();
    m_LabelMap.clear();
}

#include "resgraphview.moc"
