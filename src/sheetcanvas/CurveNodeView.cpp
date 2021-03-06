/*
Copyright (C) 2006-2007 Remon Sijrier

This file is part of Traverso

Traverso is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.

*/

#include "CurveNodeView.h"
#include "SheetView.h"

#include <QPainter>
#include <QPen>
#include <CurveNode.h>
#include <Themer.h>
#include <Curve.h>
#include "CurveView.h"

#include <Debugger.h>

CurveNodeView::CurveNodeView( SheetView * sv, CurveView* curveview, CurveNode * node, Curve* guicurve)
	: ViewItem(curveview, 0)
	, CurveNode(guicurve, node->get_when(), node->get_value())
	, m_node(node)
{
	PENTERCONS;
	m_sv = sv;
	m_curveview = curveview;
	int size = themer()->get_property("CurveNode:diameter", 6).toInt();
	setCursor(themer()->get_cursor("CurveNode"));
	m_boundingRect = QRectF(0, 0, size, size);
	load_theme_data();
	
	connect(m_node->m_curve, SIGNAL(nodePositionChanged()), this, SLOT(update_pos()));
}

CurveNodeView::~ CurveNodeView( )
{
	PENTERDES;
}

void CurveNodeView::paint( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget )
{
	Q_UNUSED(option);
	Q_UNUSED(widget);
	
	painter->save();
	
	QPen pen;
	pen.setColor(m_color);
	
	QPointF mapped = mapToParent(QPointF(0, 0));
	int x = (int) mapped.x();
	int y = (int) mapped.y();
	int heightadjust = 0;
	int widthadjust = 0;
	
	if ( (y + m_boundingRect.height()) > (int) m_parentViewItem->boundingRect().height() ) {
		heightadjust = y - (int)m_parentViewItem->boundingRect().height() + (int) m_boundingRect.height();
	}
	
	if ( (x + m_boundingRect.width()) > (int) m_parentViewItem->boundingRect().width() ) {
		widthadjust = x - (int) m_parentViewItem->boundingRect().width() + (int) m_boundingRect.width();
	}
		
// 	printf("widthadjust is %d, heightadjust = %d\n", widthadjust, heightadjust);
	if (x > 0) x = 0;
	if (y > 0) y = 0;
	
	QRectF rect = m_boundingRect.adjusted(- x, - y, - widthadjust, -heightadjust);
	painter->setClipRect(rect);
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(pen);
	
	QPainterPath path;
	path.addEllipse(m_boundingRect);
	
	painter->fillPath(path, QBrush(m_color));
	
	painter->restore();
} 


void CurveNodeView::calculate_bounding_rect()
{
	update_pos();
}


void CurveNodeView::set_color(QColor color)
{
	m_color = color;
}

void CurveNodeView::update_pos( )
{
	qreal halfwidth = (m_boundingRect.width() / 2);
	qreal parentheight = m_parentViewItem->boundingRect().height();
	qreal when = ((TimeRef(m_node->get_when()) - m_curveview->get_start_offset()) / m_sv->timeref_scalefactor) - halfwidth;
	qreal value = parentheight - (m_node->get_value() * parentheight + halfwidth);
	setPos(when, value);
		
	set_when_and_value((m_node->get_when() / m_sv->timeref_scalefactor), m_node->get_value());
}

void CurveNodeView::set_selected( )
{
	int size = themer()->get_property("CurveNode:diameter", 6).toInt();
	m_boundingRect.setWidth(size + 1);
	m_boundingRect.setHeight(size + 1);
	update_pos();
}

void CurveNodeView::reset_size( )
{
	int size = themer()->get_property("CurveNode:diameter", 6).toInt();
	m_boundingRect.setWidth(size);
	m_boundingRect.setHeight(size);
	update_pos();
}

void CurveNodeView::load_theme_data()
{
	m_color = themer()->get_color("CurveNode:default");
	calculate_bounding_rect();
}


