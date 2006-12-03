/*
Copyright (C) 2006 Remon Sijrier 

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

$Id: CurveNodeView.h,v 1.2 2006/12/01 13:58:45 r_sijrier Exp $
*/

#ifndef CURVE_NODE_VIEW_H
#define CURVE_NODE_VIEW_H

#include "ViewItem.h"

class CurveNode;

class CurveNodeView : public ViewItem
{
	Q_OBJECT

public:
	CurveNodeView(SongView* sv, ViewItem* parentViewItem, CurveNode* node);
	~CurveNodeView();
	
	enum {Type = UserType + 9};
	
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	int type() const;
	void calculate_bounding_rect();
	
	void set_color(QColor color);
	CurveNode* get_node() const {return m_node;}
	
private:
	CurveNode*	m_node;
	QColor		m_color;

public slots:
	void update_pos();
};


inline int CurveNodeView::type() const {return Type;}

#endif

//eof
 
 