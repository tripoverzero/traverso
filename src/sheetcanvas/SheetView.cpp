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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-11  USA.

*/


#include <QScrollBar>

#include "InputEngine.h"
#include "Sheet.h"
#include "SnapList.h"
#include "Track.h"
#include "ContextPointer.h"
#include "Themer.h"

#include "SheetView.h"
#include "SheetWidget.h"
#include "TrackView.h"
#include "TrackPanelView.h"
#include "Cursors.h"
#include "ClipsViewPort.h"
#include "TimeLineViewPort.h"
#include "TimeLineView.h"
#include "TrackPanelViewPort.h"

#include "AddRemove.h"
#include "Zoom.h"
#include "PlayHeadMove.h"
#include "WorkCursorMove.h"

#include "AudioDevice.h"
		
#include <Debugger.h>

class Shuttle : public Command
{
public :
	Shuttle(SheetView* sv) : Command("Shuttle"), m_sv(sv) {}

	int begin_hold() {
		m_sv->update_shuttle_factor();
		m_sv->start_shuttle(true);
		return 1;
	}

	int finish_hold() {
		m_sv->start_shuttle(false);
		return 1;
	}
	
	int jog() {
		m_sv->update_shuttle_factor();
		return 1;
	}

private :
	SheetView*	m_sv;
};


static bool smallerTrackView(const TrackView* left, const TrackView* right )
{
	return left->get_track()->get_sort_index() < right->get_track()->get_sort_index();
}

SheetView::SheetView(SheetWidget* sheetwidget, 
	ClipsViewPort* viewPort,
	TrackPanelViewPort* tpvp,
	TimeLineViewPort* tlvp,
	Sheet* sheet)
	: ViewItem(0, sheet)
{
	setZValue(1);
	
	m_sheet = sheet;
	m_clipsViewPort = viewPort;
	m_tpvp = tpvp;
	m_tlvp = tlvp;
	m_vScrollBar = sheetwidget->m_vScrollBar;
	m_hScrollBar = sheetwidget->m_hScrollBar;
	m_actOnPlayHead = true;
	m_viewportReady = false;
	
	m_clipsViewPort->scene()->addItem(this);
	
	m_playCursor = new PlayHead(this, m_sheet, m_clipsViewPort);
	m_workCursor = new WorkCursor(this, m_sheet);
	
	connect(m_sheet, SIGNAL(workingPosChanged()), m_workCursor, SLOT(update_position()));
	connect(m_sheet, SIGNAL(transportStarted()), this, SLOT(follow_play_head()));
	connect(m_sheet, SIGNAL(transportPosSet()), this, SLOT(transport_position_set()));
	connect(m_sheet, SIGNAL(workingPosChanged()), this, SLOT(stop_follow_play_head()));
	
	m_clipsViewPort->scene()->addItem(m_playCursor);
	m_clipsViewPort->scene()->addItem(m_workCursor);
	
	m_clipsViewPort->setSceneRect(0, 0, MAX_CANVAS_WIDTH, MAX_CANVAS_HEIGHT);
	m_tlvp->setSceneRect(0, -TIMELINE_HEIGHT, MAX_CANVAS_WIDTH, 0);
	m_tpvp->setSceneRect(-200, 0, 0, MAX_CANVAS_HEIGHT);
	
	// Set up the viewports scale factor, and our timeref_scalefactor / m_peakCacheZoomFactor
	// Needed for our childs TrackView, AudioClipView, TimeLines MarkerViews etc which are created below.
	scale_factor_changed();
	
	sheet_mode_changed();
	
	connect(m_sheet, SIGNAL(hzoomChanged()), this, SLOT(scale_factor_changed()));
	connect(m_sheet, SIGNAL(tempFollowChanged(bool)), this, SLOT(set_follow_state(bool)));
	connect(m_sheet, SIGNAL(trackAdded(Track*)), this, SLOT(add_new_trackview(Track*)));
	connect(m_sheet, SIGNAL(trackRemoved(Track*)), this, SLOT(remove_trackview(Track*)));
	connect(m_sheet, SIGNAL(lastFramePositionChanged()), this, SLOT(update_scrollbars()));
	connect(m_sheet, SIGNAL(modeChanged()), this, SLOT(sheet_mode_changed()));
	connect(&m_shuttletimer, SIGNAL(timeout()), this, SLOT (update_shuttle()));
	connect(m_hScrollBar, SIGNAL(sliderMoved(int)), this, SLOT(stop_follow_play_head()));
	connect(m_hScrollBar, SIGNAL(actionTriggered(int)), this, SLOT(hscrollbar_action(int)));
	connect(m_hScrollBar, SIGNAL(valueChanged(int)), this, SLOT(hscrollbar_value_changed(int)));
	connect(m_vScrollBar, SIGNAL(valueChanged(int)), m_clipsViewPort->verticalScrollBar(), SLOT(setValue(int)));
	
	m_shuttleCurve = new Curve(0);
	m_shuttleCurve->set_sheet(m_sheet);
	m_dragShuttleCurve = new Curve(0);
	m_dragShuttleCurve->set_sheet(m_sheet);
	
	// Use these variables to fine tune the scroll behavior
	float whens[7] = {0.0, 0.2, 0.3, 0.4, 0.6, 0.9, 1.2};
	float values[7] = {0.0, 0.15, 0.3, 0.8, 0.95, 1.5, 8.0};
	
	// Use these variables to fine tune the scroll during drag behavior
	float dragWhens[7] =  {0.0, 0.9, 0.94, 0.98, 1.0, 1.1, 1.3};
	float dragValues[7] = {0.0, 0.0, 0.2,  0.5,  0.85,  1.1,  2.0};
	
	for (int i=0; i<7; ++i) {
		AddRemove* cmd = (AddRemove*) m_dragShuttleCurve->add_node(new CurveNode(m_dragShuttleCurve, dragWhens[i], dragValues[i]), false);
		cmd->set_instantanious(true);
		Command::process_command(cmd);
		
		cmd = (AddRemove*) m_shuttleCurve->add_node(new CurveNode(m_shuttleCurve, whens[i], values[i]), false);
		cmd->set_instantanious(true);
		Command::process_command(cmd);
	}
}

SheetView::~SheetView()
{
	delete m_dragShuttleCurve;
	delete m_shuttleCurve;
}
		
void SheetView::scale_factor_changed( )
{
	timeref_scalefactor = qint64(m_sheet->get_hzoom() * (UNIVERSAL_SAMPLE_RATE / 44100));
	m_tlvp->scale_factor_changed();
	
	layout_tracks();
}

void SheetView::sheet_mode_changed()
{
	int mode = m_sheet->get_mode();
	m_clipsViewPort->set_current_mode(mode);
	m_tlvp->set_current_mode(mode);
	m_tpvp->set_current_mode(mode);
}

TrackView* SheetView::get_trackview_under( QPointF point )
{
	TrackView* view = 0;
	QList<QGraphicsItem*> views = m_clipsViewPort->items(m_clipsViewPort->mapFromScene(point));
	
	for (int i=0; i<views.size(); ++i) {
		view = dynamic_cast<TrackView*>(views.at(i));
		if (view) {
			return view;
		}
	}
	return  0;
	
}

void SheetView::add_new_trackview(Track* track)
{
	TrackView* view = new TrackView(this, track);
	m_trackViews.append(view);
	
	int sortIndex = track->get_sort_index();
	
	if (sortIndex < 0) {
		sortIndex = m_trackViews.size();
		track->set_sort_index(sortIndex);
	} else {
		foreach(TrackView* view, m_trackViews) {
			if (view->get_track()->get_sort_index() == sortIndex) {
				sortIndex = m_trackViews.size();
				track->set_sort_index(sortIndex);
				break;
			}
		}
	}
	
	qSort(m_trackViews.begin(), m_trackViews.end(), smallerTrackView);
	
	for(int i=0; i<m_trackViews.size(); ++i) {
		m_trackViews.at(i)->get_track()->set_sort_index(i);
	}
	
	if (m_trackViews.size() > 1) {
		int height = m_trackViews.at(m_trackViews.size()-2)->get_track()->get_height();
		m_trackViews.at(m_trackViews.size()-1)->get_track()->set_height(height);
	}
	
	layout_tracks();
}

void SheetView::remove_trackview(Track* track)
{
	foreach(TrackView* view, m_trackViews) {
		if (view->get_track() == track) {
			TrackPanelView* tpv = view->get_trackpanel_view();
			scene()->removeItem(tpv);
			scene()->removeItem(view);
			m_trackViews.removeAll(view);
			delete view;
			delete tpv;
			break;
		}
	}
	
	for(int i=0; i<m_trackViews.size(); ++i) {
		m_trackViews.at(i)->get_track()->set_sort_index(i);
	}
	
	layout_tracks();
}

void SheetView::update_scrollbars()
{
	int width = (int)(m_sheet->get_last_location() / timeref_scalefactor) - (m_clipsViewPort->width() / 4);
	if (width < m_clipsViewPort->width() / 4) {
		width = m_clipsViewPort->width() / 4;
	}
	
	m_hScrollBar->setRange(0, width);
	m_hScrollBar->setSingleStep(m_clipsViewPort->width() / 10);
	m_hScrollBar->setPageStep(m_clipsViewPort->width());
	
	m_vScrollBar->setRange(0, m_sceneHeight - m_clipsViewPort->height() / 2);
	m_vScrollBar->setSingleStep(m_clipsViewPort->height() / 10);
	m_vScrollBar->setPageStep(m_clipsViewPort->height());
	
	m_playCursor->set_bounding_rect(QRectF(0, 0, 4, m_vScrollBar->maximum() + m_clipsViewPort->height()));
	m_playCursor->update_position();
	m_workCursor->set_bounding_rect(QRectF(0, 0, 1, m_vScrollBar->maximum() + m_clipsViewPort->height()));
	m_workCursor->update_position();
	
	set_snap_range(m_hScrollBar->value());
}

void SheetView::hscrollbar_value_changed(int value)
{
	// This slot is called when the hscrollbar value changes,
	// which can be due shuttling or playhead scrolling the page.
	// In that very case, we do NOT set the hscrollbar value AGAIN
	// but in case of a non-shuttle command, we call ie().jog to give the 
	// command the opportunity to update (Gain-cursor position for example) 
	// itself for the changed viewport / mouse coordinates.
	// FIXME This is NOT a solution to set hold-cursors at the correct
	// position in the viewport when it's scrolled programatically !!!!!
	if (ie().is_holding()) {
		Shuttle* s = dynamic_cast<Shuttle*>(ie().get_holding_command());
		if (!s) {
			ie().jog();
		}
	} else {
		m_clipsViewPort->horizontalScrollBar()->setValue(value);
	}
	
	set_snap_range(m_hScrollBar->value());
}

void SheetView::vzoom(qreal factor)
{
	PENTER;
	for (int i=0; i<m_trackViews.size(); ++i) {
		TrackView* view = m_trackViews.at(i);
		Track* track = view->get_track();
		int height = track->get_height();
		height = (int) (height * factor);
		if (height > m_trackMaximumHeight) {
			height = m_trackMaximumHeight;
		} else if (height < m_trackMinimumHeight) {
			height = m_trackMinimumHeight;
		}
		track->set_height(height);
	}
	
	layout_tracks();
}

void SheetView::hzoom(qreal factor)
{
	PENTER;
	m_sheet->set_hzoom(m_sheet->get_hzoom() * factor);
	center();
}


void SheetView::layout_tracks()
{
	if (m_trackViews.isEmpty() || !m_viewportReady) return;
	
	int verticalposition = m_trackTopIndent;
	for (int i=0; i<m_trackViews.size(); ++i) {
		TrackView* view = m_trackViews.at(i);
		view->calculate_bounding_rect();
		view->move_to(0, verticalposition);
		verticalposition += (view->get_track()->get_height() + m_trackSeperatingHeight);
	}
	
	m_sceneHeight = verticalposition;
	update_scrollbars();
}


Command* SheetView::center()
{
	PENTER2;
	TimeRef centerX;
	if (m_sheet->is_transport_rolling() && m_actOnPlayHead) { 
		centerX = m_sheet->get_transport_location();
	} else {
		centerX = m_sheet->get_work_location();
	}
	
	int x = qRound(centerX / timeref_scalefactor);
	set_hscrollbar_value(x - m_clipsViewPort->width() / 2);
	return (Command*) 0;
}


void SheetView::transport_position_set()
{
	if (!m_sheet->is_transport_rolling())
		center_playhead();
}


void SheetView::stop_follow_play_head()
{
	m_sheet->set_temp_follow_state(false);
}


void SheetView::follow_play_head()
{
	m_sheet->set_temp_follow_state(true);
}


void SheetView::set_follow_state(bool state)
{
	if (state) {
		m_actOnPlayHead = true;
		m_playCursor->enable_follow();
		m_playCursor->update_position();
	} else {
		m_actOnPlayHead = false;
		m_playCursor->disable_follow();
	}
}


Command* SheetView::shuttle()
{
 	return new Shuttle(this);
}


void SheetView::start_shuttle(bool start, bool drag)
{
	if (start) {
		m_shuttletimer.start(40);
		m_dragShuttle = drag;
		m_shuttleYfactor = m_shuttleXfactor = 0;
		stop_follow_play_head();
	} else {
		m_shuttletimer.stop();
	}
}

void SheetView::set_shuttle_factor_values(int x, int y)
{
	m_shuttleXfactor = x;
	m_shuttleYfactor = y;
}


void SheetView::update_shuttle_factor()
{
	float vec[2];
	int direction = 1;
	
	float normalizedX = (float) cpointer().x() / m_clipsViewPort->width();
	
	if (normalizedX < 0.5) {
		normalizedX = 0.5 - normalizedX;
		normalizedX *= 2;
		direction = -1;
	} else if (normalizedX > 0.5) {
		normalizedX = normalizedX - 0.5;
		normalizedX *= 2;
		if (normalizedX > 1.0) {
			normalizedX *= 1.15;
		}
	}
	
	if (m_dragShuttle) {
		m_dragShuttleCurve->get_vector(normalizedX, normalizedX + 0.01, vec, 2);
	} else {
		m_shuttleCurve->get_vector(normalizedX, normalizedX + 0.01, vec, 2);
	}
	
	if (direction > 0) {
		m_shuttleXfactor = (int) (vec[0] * 30);
	} else {
		m_shuttleXfactor = (int) (vec[0] * -30);
	}
	
	direction = 1;
	float normalizedY = (float) cpointer().y() / m_clipsViewPort->height();
	
	if (normalizedY < 0) normalizedY = 0;
	if (normalizedY > 1) normalizedY = 1;
	
	if (normalizedY > 0.35 && normalizedY < 0.65) {
		normalizedY = 0;
	} else if (normalizedY < 0.5) {
		normalizedY = 0.5 - normalizedY;
		direction = -1;
	} else if (normalizedY > 0.5) {
		normalizedY = normalizedY - 0.5;
	}
	
	normalizedY *= 2;
	
	if (m_dragShuttle) {
		m_dragShuttleCurve->get_vector(normalizedY, normalizedY + 0.01, vec, 2);
	} else {
		m_shuttleCurve->get_vector(normalizedY, normalizedY + 0.01, vec, 2);
	}
	
	int yscale;
	
	if (m_trackViews.size()) {
		yscale = int(mean_track_height() / 10);
	} else {
		yscale = int(m_clipsViewPort->viewport()->height() / 10);
	}
	
	if (direction > 0) {
		m_shuttleYfactor = (int) (vec[0] * yscale);
	} else {
		m_shuttleYfactor = (int) (vec[0] * -yscale);
	}
	
	if (m_dragShuttle) {
		m_shuttleYfactor *= 4;
	}
}

int SheetView::mean_track_height()
{
	int total =0;
	int mean = 0;
	
	foreach(TrackView* view, m_trackViews) {
		total += view->get_height();
	}
	
	mean = total / m_trackViews.size();
	
	return mean;
}

void SheetView::update_shuttle()
{
	int x = m_clipsViewPort->horizontalScrollBar()->value() + m_shuttleXfactor;
	set_hscrollbar_value(x);
	
	int y = m_clipsViewPort->verticalScrollBar()->value() + m_shuttleYfactor;
	set_vscrollbar_value(y);
	
	if (m_shuttleXfactor != 0 || m_shuttleYfactor != 0) {
		ie().jog();
	}
}


Command* SheetView::goto_begin()
{
	stop_follow_play_head();
	m_sheet->set_work_at(TimeRef());
	center();
	return (Command*) 0;
}


Command* SheetView::goto_end()
{
	stop_follow_play_head();
	TimeRef lastlocation = m_sheet->get_last_location();
	m_sheet->set_work_at(lastlocation);
	center();
	return (Command*) 0;
}


TrackPanelViewPort* SheetView::get_trackpanel_view_port( ) const
{
	return m_tpvp;
}

ClipsViewPort * SheetView::get_clips_viewport() const
{
	return m_clipsViewPort;
}


Command * SheetView::touch( )
{
	QPointF point = m_clipsViewPort->mapToScene(QPoint(cpointer().on_first_input_event_x(), cpointer().on_first_input_event_y()));
	m_sheet->set_work_at(TimeRef(point.x() * timeref_scalefactor));

	return 0;
}

Command * SheetView::touch_play_cursor( )
{
	QPointF point = m_clipsViewPort->mapToScene(QPoint(cpointer().on_first_input_event_x(), cpointer().on_first_input_event_y()));
	m_sheet->set_transport_pos(TimeRef(point.x() * timeref_scalefactor));

	return 0;
}

Command * SheetView::play_to_begin( )
{
	m_sheet->set_transport_pos(TimeRef());

	return 0;
}

Command * SheetView::play_cursor_move( )
{
	return new PlayHeadMove(m_playCursor, this);
}

Command * SheetView::work_cursor_move( )
{
	return new WorkCursorMove(m_playCursor, this);
}

void SheetView::set_snap_range(int start)
{
// 	printf("SheetView::set_snap_range\n");
	m_sheet->get_snap_list()->set_range(TimeRef(start * timeref_scalefactor),
					TimeRef((start + m_clipsViewPort->viewport()->width()) * timeref_scalefactor),
					timeref_scalefactor);
}

Command* SheetView::scroll_up( )
{
	PENTER3;
	set_vscrollbar_value(m_clipsViewPort->verticalScrollBar()->value() - int(mean_track_height() * 0.75));
	return (Command*) 0;
}

Command* SheetView::scroll_down( )
{
	PENTER3;
	set_vscrollbar_value(m_clipsViewPort->verticalScrollBar()->value() + int(mean_track_height() * 0.75));
	return (Command*) 0;
}

Command* SheetView::scroll_right()
{
	PENTER3;
	stop_follow_play_head();
	set_hscrollbar_value(m_clipsViewPort->horizontalScrollBar()->value() + 50);
	return (Command*) 0;
}


Command* SheetView::scroll_left()
{
	PENTER3;
	stop_follow_play_head();
	set_hscrollbar_value(m_clipsViewPort->horizontalScrollBar()->value() - 50);
	return (Command*) 0;
}

int SheetView::hscrollbar_value() const
{
	return m_clipsViewPort->horizontalScrollBar()->value();
}

void SheetView::hscrollbar_action(int action)
{
	if (action == QAbstractSlider::SliderPageStepAdd || action == QAbstractSlider::SliderPageStepSub) {
		stop_follow_play_head();
	}
}

int SheetView::vscrollbar_value() const
{
	return m_clipsViewPort->verticalScrollBar()->value();
}

void SheetView::load_theme_data()
{
	m_trackSeperatingHeight = themer()->get_property("Sheet:track:seperatingheight", 0).toInt();
	m_trackMinimumHeight = themer()->get_property("Sheet:track:minimumheight", 16).toInt();
	m_trackMaximumHeight = themer()->get_property("Sheet:track:maximumheight", 300).toInt();
	m_trackTopIndent = themer()->get_property("Sheet:track:topindent", 6).toInt();
	
	m_clipsViewPort->setBackgroundBrush(themer()->get_color("Sheet:background"));
	m_tpvp->setBackgroundBrush(themer()->get_color("TrackPanel:background"));

	layout_tracks();
}

Command * SheetView::add_marker()
{
	return m_tlvp->get_timeline_view()->add_marker();
}

Command * SheetView::add_marker_at_playhead()
{
	return m_tlvp->get_timeline_view()->add_marker_at_playhead();
}

Command * SheetView::playhead_to_workcursor( )
{
	TimeRef worklocation = m_sheet->get_work_location();

	m_sheet->set_transport_pos(worklocation);
	
	if (!m_sheet->is_transport_rolling()) {
		center();
	}

	return (Command*) 0;
}

Command * SheetView::center_playhead( )
{
	TimeRef centerX = m_sheet->get_transport_location();
	set_hscrollbar_value(int(centerX / timeref_scalefactor - m_clipsViewPort->width() / 2));
	
	follow_play_head();

	return (Command*) 0;
}

void SheetView::set_hscrollbar_value(int value)
{
	m_clipsViewPort->horizontalScrollBar()->setValue(value);
	m_hScrollBar->setValue(value);
	m_sheet->set_scrollbar_xy(m_hScrollBar->value(), m_vScrollBar->value());
}

void SheetView::set_vscrollbar_value(int value)
{
	if (value > m_vScrollBar->maximum()) {
		value = m_vScrollBar->maximum();
	}
	m_clipsViewPort->verticalScrollBar()->setValue(value);
	m_vScrollBar->setValue(value);
	m_sheet->set_scrollbar_xy(m_hScrollBar->value(), m_vScrollBar->value());
}

void SheetView::clipviewport_resize_event()
{
	// Once the ClipViewPort has been initialed, and _resized_
	// only _then_ we know _our_ size, at which time it makes 
	// sense to populate the view with tracks.
	if (!m_viewportReady) {
		
		// fill the view with trackviews, add_new_trackview()
		// doesn't yet layout the new tracks.
		foreach(Track* track, m_sheet->get_tracks()) {
			add_new_trackview(track);
		}
	
		// layout_track() now will do it's work when it is called
		m_viewportReady = true;
		
		// this will call layout_tracks() for us too
		// which will continue now, due m_viewportReady is true now
		load_theme_data();
		
		// Everything is in place to scroll to the last position
		// we were at, at closing this view.
		int x, y;
		m_sheet->get_scrollbar_xy(x, y);
		set_hscrollbar_value(x);
		set_vscrollbar_value(y);
	} else {
		// if the viewport was resized, and everything was setup allready
		// it suffices to recalculate the scrollbar values.
		update_scrollbars();
	}
	
}
