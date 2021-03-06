/*
Copyright (C) 2005-2007 Remon Sijrier 

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

$Id: TrackPanelView.h,v 1.1 2008/01/21 16:17:30 r_sijrier Exp $
*/

#ifndef TRACK_PANEL_VIEW_H
#define TRACK_PANEL_VIEW_H

#include "ViewItem.h"

class Track;
class TrackView;
class TrackPanelViewPort;
class PanelLed;
class TrackPanelView;

class TrackPanelGain : public ViewItem
{
	Q_OBJECT

public:
	TrackPanelGain(TrackPanelView* parent, Track* track);
	TrackPanelGain(){}

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void set_width(int width);

public slots:
	Command* gain_increment();
	Command* gain_decrement();
	
private:
	Track* m_track;
};

class TrackPanelPan : public ViewItem
{
	Q_OBJECT
	
public:
	TrackPanelPan(TrackPanelView* parent, Track* track);
	TrackPanelPan(){}
	

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void set_width(int width);

public slots:
	Command* pan_left();
	Command* pan_right();

private:
	Track* m_track;
};



class TrackPanelLed : public ViewItem
{
	Q_OBJECT
public:
	TrackPanelLed(TrackPanelView* view, const QString& name, const QString& toggleslot);
	
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void set_bounding_rect(QRectF rect);

private:
	Track* m_track;
        QString m_name;
	QString m_toggleslot;
	bool m_isOn;

public slots:
        void ison_changed(bool isOn);
	
	Command* toggle();
};

class TrackPanelBus : public ViewItem
{
	Q_OBJECT
public:
	TrackPanelBus(TrackPanelView* view, Track* track, int busType);
	TrackPanelBus(){}
	
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	
	enum { BUSIN, BUSOUT };

private:
	Track*	m_track;
        int	m_type;
	QString m_busName;
	QPixmap m_pix;

public slots:
        void bus_changed();
};


class TrackPanelView : public ViewItem
{
	Q_OBJECT

public:
	TrackPanelView(TrackView* trackView);
	~TrackPanelView();

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	void calculate_bounding_rect();
	
	Track* get_track() const {return m_track;}
	
private:
	Track*			m_track;
	TrackView*		m_tv;
	TrackPanelViewPort*	m_viewPort;
	TrackPanelGain*		m_gainView;
	TrackPanelPan*		m_panView;
	
	TrackPanelLed* muteLed;
	TrackPanelLed* soloLed;
	TrackPanelLed* recLed;
	
	TrackPanelBus*	inBus;
	TrackPanelBus*	outBus;

	void draw_panel_track_name(QPainter* painter);
	void layout_panel_items();

private slots:
	void update_gain();
	void update_pan();
	void update_track_name();
};


#endif

//eof
 
