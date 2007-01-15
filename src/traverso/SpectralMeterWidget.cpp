/*
    Copyright (C) 2006 Nicola Doebelin

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

    $Id: SpectralMeterWidget.cpp,v 1.12 2007/01/15 23:53:28 r_sijrier Exp $
*/

#include "SpectralMeterWidget.h"
#include <Config.h>
#include <PluginChain.h>
#include <SpectralMeter.h>
#include <Command.h>
#include <ProjectManager.h>
#include <Project.h>
#include <AudioDevice.h>
#include <InputEngine.h>
#include <Song.h>
#include <ContextPointer.h>

#include <QtGui>

#include <math.h>
#include <limits.h>

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"

#define SMOOTH_FACTOR 0.97
#define DB_FLOOR -140.0

static const float DEFAULT_VAL = -999.0f;
static const int UPDATE_INTERVAL = 40;
static const int FONT_SIZE = 7;
static const uint MAX_SAMPLES = UINT_MAX;


SpectralMeterWidget::SpectralMeterWidget(QWidget* parent)
	: ViewPort(parent)
{
	setMinimumWidth(40);
	setMinimumHeight(10);
	
	// We paint all our pixels ourselves, so no need to let Qt
	// erase and fill it for us prior to the paintEvent.
	// @ Nicola : This is where the high load comes from!
//         setAttribute(Qt::WA_OpaquePaintEvent);
	
	m_item = new SpectralMeterItem(this);
	
	QGraphicsScene* scene = new QGraphicsScene(this);
	setScene(scene);
	scene->addItem(m_item);
	m_item->setPos(0,0);
	
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

SpectralMeterWidget::~SpectralMeterWidget()
{
}

void SpectralMeterWidget::resizeEvent( QResizeEvent *  )
{
	m_item->resize();
}

void SpectralMeterWidget::get_pointed_context_items(QList<ContextItem* > &list)
{
	printf("SpectralMeterWidget::get_pointed_view_items\n");
	QList<QGraphicsItem *> itemlist = items(cpointer().x(), cpointer().y());
	foreach(QGraphicsItem* item, itemlist) {
		list.append((ViewItem*)item);
	}
	
	printf("itemlist size is %d\n", itemlist.size());
}



SpectralMeterItem::SpectralMeterItem(SpectralMeterWidget* widget)
	: ViewItem(0, 0)
	, m_widget(widget)
	, m_meter(0)
{

	m_config = new SpectralMeterConfigWidget(m_widget);
	load_configuration();
	
	upper_freq_log = log10(upper_freq);
	lower_freq_log = log10(lower_freq);
	sample_rate = audiodevice().get_sample_rate();
	show_average = false;
	sample_weight = 1;

	QFontMetrics fm(QFont("Bitstream Vera Sans", FONT_SIZE));
	margin_l = 5;
	margin_r = fm.width("-XX") + 5;
	margin_t = fm.ascent()/2 + 5;
	margin_b = fm.ascent() + fm.descent() + 10;
	

	for (int i = 0; i < 4; ++i) {
		m_freq_labels.push_back(10.0f * pow(10.0,i));
		m_freq_labels.push_back(20.0f * pow(10.0,i));
		m_freq_labels.push_back(30.0f * pow(10.0,i));
		m_freq_labels.push_back(40.0f * pow(10.0,i));
		m_freq_labels.push_back(50.0f * pow(10.0,i));
		m_freq_labels.push_back(60.0f * pow(10.0,i));
		m_freq_labels.push_back(70.0f * pow(10.0,i));
		m_freq_labels.push_back(80.0f * pow(10.0,i));
		m_freq_labels.push_back(90.0f * pow(10.0,i));
	}

	connect(m_config, SIGNAL(configChanged()), this, SLOT(load_configuration()));

	// Connections to core:
	connect(&pm(), SIGNAL(projectLoaded(Project*)), this, SLOT(set_project(Project*)));
	connect(&timer, SIGNAL(timeout()), this, SLOT(update_data()));
}

SpectralMeterItem::~SpectralMeterItem()
{
// 	delete m_config;
}

void SpectralMeterItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);
	
	painter->drawPixmap(0, 0, bgPixmap);

	// draw the bars
	if (m_spectrum.size()) {
		QPen pen;
		pen.setColor(QColor(80, 80, 120));
		pen.setWidth(bar_width);
		pen.setCapStyle(Qt::FlatCap);
		painter->setClipRegion(m_rect);
		painter->setPen(pen);

		QPointF pt;

		// draw the freq bands
		for (uint i = 0; i < (uint)m_spectrum.size(); ++i) {
			if (m_spectrum.at(i) < lower_db) {
				continue;
			}
			pt.setX(bar_offset + i * m_rect.width() / num_bands);
			pt.setY(db2ypos(m_spectrum.at(i)));
			painter->drawLine(QPointF(pt.x(), m_widget->height()), pt);
		}

		// draw the average line if requested
		if (show_average) {
			painter->setPen(Qt::red);
			QPointF po(bar_offset, db2ypos(m_avg_db.at(0)));
			for (uint i = 0; i < (uint)m_avg_db.size(); ++i) {
				pt.setX(m_map_idx2xpos.at(i));
				pt.setY(db2ypos(m_avg_db.at(i)));
				painter->drawLine(po, pt);
				po = pt;
			}
		}
	}
}

void SpectralMeterItem::resize()
{
	PENTER;
	
	prepareGeometryChange();
	

	// Make the axis labels disappear when the widget becomes too small
	int x = 0, y = 0, w = m_widget->width(), h = m_widget->height();
	m_boundingRectangle = QRectF(0, 0, w, h);
	
	if (m_widget->width() >= 200) {
		x = margin_l;
		w -= (margin_l + margin_r);
	}

	if (m_widget->height() >= 120) {
		y = margin_t;
		h -= (margin_t + margin_b);
	}

	
	m_rect.setRect(x, y, w, h);

	// update the vectors mapping indices and frequencies to widget coordinates
	update_freq_map();

	// re-calculate the bar width
	update_barwidth();

	// re-draw the background pixmap
	update_background();
}

void SpectralMeterItem::update_background()
{
	// draw the background image
	bgPixmap = QPixmap((int)m_boundingRectangle.width(), (int)m_boundingRectangle.height());
	bgPixmap.fill(QColor(246, 246, 255));

	QPainter painter(&bgPixmap);
	painter.fillRect(m_rect, QColor(241, 250, 255));
	painter.setFont(QFont("Bitstream Vera Sans", FONT_SIZE));
	QFontMetrics fm(QFont("Bitstream Vera Sans", FONT_SIZE));

	QString spm;

	// draw horizontal lines + labels
	for (float i = upper_db; i >= lower_db; i -= 10.0f) {
		float f = db2ypos(i);

		painter.setPen(QColor(205,223,255));
		painter.drawLine(QPointF(m_rect.x(), f), QPointF(m_rect.right(), f));

		painter.setPen(QColor(  0,  0,  0));
		spm.sprintf("%2.0f", i);
		painter.drawText(m_rect.right() + 1, (int)f + fm.ascent()/2, spm);
	}

	// draw frequency labels and tickmarks
	float last_pos = 1.0;
	for (int i = 0; i < m_freq_labels.size(); ++i) {
		// check if we have space to draw the labels by checking if the
		// m_rect is borderless
		if (!m_rect.top()) {
			break;
		}

		float f = freq2xpos(m_freq_labels.at(i));

		// check if the freq is in the visible range
		if (!f) {
			continue;
		}

		spm.sprintf("%2.0f", m_freq_labels.at(i));
		float s = (float)fm.width(spm)/2.0f;


		// draw text only if there is enough space for it
		if (((f - s) > last_pos) && ((f + s) < float(m_boundingRectangle.width()-1))) {
			painter.setPen(Qt::black);
			painter.drawText(QPointF(f - s, m_boundingRectangle.height() - fm.descent() - 3), spm);
			last_pos = f + s + 1.0;
		} else {
			painter.setPen(QColor(150, 150, 150));
		}

		painter.drawLine(QPointF(f, m_rect.bottom()), QPointF(f, m_rect.bottom() + 3));
	}
}

void SpectralMeterItem::update_data()
{
	if (!m_meter) {
		return;
	}

	// if no data was available, return, so we _only_ update the widget when
	// it needs to be!
 	if (m_meter->get_data(specl, specr) == 0) {
 		return;
 	}

	// process the data
	reduce_bands();

	// paint the widget
	update();
}

void SpectralMeterItem::set_project(Project *project)
{
	if (project) {
		connect(project, SIGNAL(currentSongChanged(Song *)), this, SLOT(set_song(Song*)));
		m_project = project;
	} else {
		timer.stop();
	}
}

void SpectralMeterItem::set_song(Song *song)
{
	PluginChain* chain = song->get_plugin_chain();
	
	connect(song, SIGNAL(transferStarted()), this, SLOT(transfer_started()));
	connect(song, SIGNAL(transferStopped()), this, SLOT(transfer_stopped()));

	foreach(Plugin* plugin, chain->get_plugin_list()) {
		// Nicola: qobject_cast didn't have the behaviour I thought
		// it would have, so I switched it to dynamic_cast!
		m_meter = dynamic_cast<SpectralMeter*>(plugin);
		
		if (m_meter) {
			timer.start(UPDATE_INTERVAL);
			return;
		}
	}
	
	m_meter = new SpectralMeter();
	m_meter->init();
	ie().process_command( chain->add_plugin(m_meter, false) );

	timer.start(UPDATE_INTERVAL);
}

// most of the calculations is done here:
//	1. the gain values must be converted to db
//	2. since we don't want to show every frequency bin, we reduce the number of freqs
//		and split them into bands evenly distributed on a log10 x-scale.
//		This is pretty complicated and is the reason for most of the weired code below
//	3. calculate the average db levels
void SpectralMeterItem::reduce_bands()
{
	// check if we have to update some variables
	if ((m_spectrum.size() != (int)num_bands) 
		|| (fft_size != (uint)qMin(specl.size(), specr.size())) 
		|| ((uint)m_map_idx2freq.size() != fft_size)) {
			update_layout();
	}

	// calculate the sample weight for the average curve
	double sweight = 1.0 / (double)sample_weight;
	double oweight = 1.0 - sweight;

	// used for smooth falloff
	float hist = DB_FLOOR + (m_spectrum.at(0) - DB_FLOOR) * SMOOTH_FACTOR;

	bool skip = false;

	// Convert fft results to dB. Heavily re-arranged version of the following function:
	// db = (20 * log10(2 * sqrt(r_a^2 + i_a^2) / N) + 20 * log10(2 * sqrt(r_b^2 + i_b^2) / N)) / 2
	// with (r_a^2 + i_a^2) and (r_b^2 + i_b^2) given in specl and specr vectors
	m_spectrum[0] = DB_FLOOR + (m_spectrum.at(0) - DB_FLOOR) * SMOOTH_FACTOR;

	// loop through the fft vectors
	for (uint i = 0, j = 0; i < fft_size; ++i) {
		float freq = m_map_idx2freq.at(i);

		if (freq < (float)lower_freq) {
			// We are still below the lowest displayed frequency
			skip = true;
		} else {
			skip = false;
		}

		if (freq >= m_bands.at(j)) {
			// we entered the freq range of the next band
			++j;
			if (j >= (uint)m_spectrum.size()) {
				// We are above the highest displayed frequency
				skip = true;
			} else {
				// move to the next band and fill it with the smooth falloff value as default
				hist = DB_FLOOR + (m_spectrum.at(j) - DB_FLOOR) * SMOOTH_FACTOR;
				m_spectrum[j] = hist;
			}
		}

		// calculate the db value of the current bin
		float val = 5.0 * (log10(specl.at(i) * specr.at(i)) + xfactor);

		// fill the average sample curve
		if (show_average) {
			if (i < (uint)m_avg_db.size()) {
				double dv = val * sweight + m_avg_db.at(i) * oweight;
				m_avg_db[i] = dv;
			}
		}

		// write back the actual db value to the current freq band
		if (!skip) {
			m_spectrum[j] = qMax(val, m_spectrum.at(j));
		}
	}

	// progress the sample weighting for the average curve
	if ((show_average) && (sample_weight < (MAX_SAMPLES - 1))) {
		++sample_weight;
	}
}

// call this function if the size, number of bands, ranges etc. changed.
// it re-calculates some variables
void SpectralMeterItem::update_layout()
{
	timer.stop();

	// recalculate a couple of variables
	fft_size = qMin(specl.size(), specr.size());		// number of frequencies
	xfactor = 4.0f * log10(2.0f / float(fft_size));	// a constant factor for conversion to dB
	upper_freq_log = log10(upper_freq);
	lower_freq_log = log10(lower_freq);
	freq_step = (upper_freq_log - lower_freq_log)/(num_bands);
	sample_weight = 1;

	// recreate the vector containing the levels and frequency bands
	m_spectrum.fill(DEFAULT_VAL, num_bands);
	m_avg_db.fill(DEFAULT_VAL, fft_size);

	// recreate the vector containing border frequencies of the freq bands
	m_bands.clear();
	for (uint i = 0; i < num_bands; ++i) {
		m_bands.push_back(pow(10.0, lower_freq_log + (i+1)*freq_step));
	}

	// update related stuff
	update_barwidth();
	update_freq_map();

	timer.start(UPDATE_INTERVAL);
}

// converts db-values into widget y-coordinates
float SpectralMeterItem::db2ypos(float f)
{
	return ((f - upper_db) * m_rect.height()/(lower_db - upper_db)) + m_rect.top();
}

// converts frequencies into widget x-coordinates
float SpectralMeterItem::freq2xpos(float f)
{
	if ((f < lower_freq) || (f > upper_freq)) {
		return 0.0;
	}

	float d = log10(f) - lower_freq_log;
	return (float)margin_l + d * m_rect.width() / (upper_freq_log - lower_freq_log);
}

// re-calculates the bar width of the frequency bands
void SpectralMeterItem::update_barwidth()
{
	int i = num_bands < 128 ? 2 : 0;
	bar_width = int(0.5 + (float)m_rect.width() / (float)num_bands) - i;
	bar_width = bar_width < 1 ? 1 : bar_width;
	bar_offset = int((float)m_rect.width() / (2.0f * num_bands));
	bar_offset += m_rect.x();
}

// updates a vector mapping fft indices (0, ..., fft_size) to widget x-positions
// and one mapping fft indices to frequency
void SpectralMeterItem::update_freq_map()
{
	m_map_idx2xpos.clear();
	m_map_idx2freq.clear();
	for (uint i = 0; i < fft_size; ++i) {
		float freq = float(i+1) * (float)sample_rate / (2.0f * fft_size);
		m_map_idx2freq.push_back(freq);
		m_map_idx2xpos.push_back(freq2xpos(freq));
	}
}

// opens the properties dialog
Command* SpectralMeterItem::edit_properties()
{
	if (!m_meter) {
		return 0;
	}

	m_config->show();
	
	return 0;
}

// is called upon closing the properties dialog
void SpectralMeterItem::load_configuration()
{
	upper_freq = config().get_property("SpectralMeter", "UpperFrequenty", 22050).toInt();
	lower_freq = config().get_property("SpectralMeter", "LowerFrequenty", 20).toInt();
	num_bands = config().get_property("SpectralMeter", "NumberOfBands", 16).toInt();
	upper_db = config().get_property("SpectralMeter", "UpperdB", 0).toInt();
	lower_db = config().get_property("SpectralMeter", "LowerdB", -90).toInt();
	show_average = config().get_property("SpectralMeter", "ShowAvarage", 0).toInt();
	
	if (m_meter) {
		m_meter->set_fr_size(config().get_property("SpectralMeter", "FFTSize", 2048).toInt());
		m_meter->set_windowing_function(config().get_property("SpectralMeter", "WindowingFunction", 1).toInt());
		m_meter->init();
	}

	update_layout();
	update_background();
}

void SpectralMeterItem::transfer_started()
{
	// restarts the average curve
	sample_weight = 1;
}

void SpectralMeterItem::transfer_stopped()
{

}

Command* SpectralMeterItem::set_mode()
{
	show_average = !show_average;
	update_layout();
	return 0;
}

Command* SpectralMeterItem::reset()
{
	sample_weight = 1;
	return 0;
}

Command* SpectralMeterItem::show_export_widget()
{
	// check if all requirements are met
	if ((!show_average) || (!m_avg_db.size()) || (!m_project)) {
		printf("No average data available.");
		return 0;
	}

	// check if there actually is data to export
	int s = qMin(m_map_idx2freq.size(), m_avg_db.size());
	if (!s) {
		printf("No average data available.");
		return 0;
	}

	QString fn = QFileDialog::getSaveFileName (0, tr("Export average dB curve"), m_project->get_root_dir());

	// if aborted exit here
	if (fn.isEmpty()) {
		return 0;
	}

	QFile file(fn);

	// check if the selected file can be opened for writing
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		printf("Could not open file for writing.");
		return 0;
	}

	QTextStream out(&file);
	QString separator = " ";
	QString str;

	// export the data
	// bin 0 contains no useful data (freq is 0 Hz anyway)
	for (int i = 0; i < s; ++i) {
		out << str.sprintf("%.6f %.6f\n", m_map_idx2freq.at(i), m_avg_db.at(i));
	}

	return 0;
}


/*******************************************/
/*        SpectralMeterConfWidget          */
/*******************************************/

SpectralMeterConfigWidget::SpectralMeterConfigWidget( QWidget * parent )
	: QDialog(parent)
{
	setupUi(this);
	groupBoxAdvanced->hide();
	
	load_configuration();
	
	connect(buttonAdvanced, SIGNAL(toggled(bool)), this, SLOT(advancedButton_toggled(bool)));
}

void SpectralMeterConfigWidget::on_buttonApply_clicked()
{
	save_configuration();
	emit configChanged();
}

void SpectralMeterConfigWidget::on_buttonClose_clicked( )
{
	hide();
}

void SpectralMeterConfigWidget::advancedButton_toggled(bool b)
{
	if (b) {
		groupBoxAdvanced->show();
	} else {
		groupBoxAdvanced->hide();
	}
}

void SpectralMeterConfigWidget::save_configuration( )
{
	config().set_property(	"SpectralMeter",
				"UpperFrequenty",
				qMax(spinBoxLowerFreq->value(), spinBoxUpperFreq->value()) );
	config().set_property(	"SpectralMeter",
				"LowerFrequenty",
				qMin(spinBoxLowerFreq->value(), spinBoxUpperFreq->value()) );
	config().set_property(	"SpectralMeter",
				"UpperdB",
				qMax(spinBoxUpperDb->value(), spinBoxLowerDb->value()) );
	config().set_property(	"SpectralMeter",
				"LowerdB",
				qMin(spinBoxUpperDb->value(), spinBoxLowerDb->value()) );
	config().set_property("SpectralMeter", "NumberOfBands", spinBoxNumBands->value() );
	config().set_property("SpectralMeter", "ShowAvarage", checkBoxAverage->isChecked() );
	
	config().set_property("SpectralMeter", "FFTSize", comboBoxFftSize->currentText().toInt() );
	config().set_property("SpectralMeter", "WindowingFunction", comboBoxWindowing->currentIndex() );
	config().save();
}

void SpectralMeterConfigWidget::load_configuration( )
{
	int value;
	value = config().get_property("SpectralMeter", "UpperFrequenty", 22050).toInt();
	spinBoxUpperFreq->setValue(value);
	value = config().get_property("SpectralMeter", "LowerFrequenty", 20).toInt();
	spinBoxLowerFreq->setValue(value);
	value = config().get_property("SpectralMeter", "UpperdB", 0).toInt();
	spinBoxUpperDb->setValue(value);
	value = config().get_property("SpectralMeter", "LowerdB", -90).toInt();
	spinBoxLowerDb->setValue(value);
	value = config().get_property("SpectralMeter", "NumberOfBands", 16).toInt();
	spinBoxNumBands->setValue(value);
	value = config().get_property("SpectralMeter", "ShowAvarage", 0).toInt();
	checkBoxAverage->setChecked(value);
	value = config().get_property("SpectralMeter", "FFTSize", 2048).toInt();
	QString str;
	str = QString("%1").arg(value);
	int idx = comboBoxFftSize->findText(str);
	idx = idx == -1 ? 3 : idx;
	comboBoxFftSize->setCurrentIndex(idx);
	value = config().get_property("SpectralMeter", "WindowingFunction", 1).toInt();
	comboBoxWindowing->setCurrentIndex(value);
}


//eof