/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "timeline.h"
#include "titlewidget.h"
#include "canvas/timelinemodel.h"
#include "widgets/timelinewidget.h"
#include "net/envelopebuilder.h"
#include "../rustpile/rustpile.h"

#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>

namespace docks {

Timeline::Timeline(QWidget *parent)
	: QDockWidget(tr("Timeline"), parent)
{
	m_widget = new widgets::TimelineWidget(this);
	connect(m_widget, &widgets::TimelineWidget::timelineEditCommand, this, &Timeline::timelineEditCommand);
	m_widget->setMinimumHeight(40);
	setWidget(m_widget);

	// Create the title bar widget
	auto *titlebar = new TitleWidget(this);
	setTitleBarWidget(titlebar);

	m_useTimeline = new QCheckBox(tr("Use manual timeline"));
	connect(m_useTimeline, &QCheckBox::clicked, this, &Timeline::onUseTimelineClicked);

	titlebar->addCustomWidget(m_useTimeline);
	titlebar->addStretch();
	titlebar->addCustomWidget(new QLabel(tr("FPS:")));

	m_fps = new QSpinBox;
	m_fps->setMinimum(1);
	m_fps->setMaximum(99);
	connect(m_fps, QOverload<int>::of(&QSpinBox::valueChanged), this, &Timeline::onFpsChanged);

	titlebar->addCustomWidget(m_fps);
}

void Timeline::setTimeline(canvas::TimelineModel *model)
{
	m_widget->setModel(model);
}

void Timeline::setUseTimeline(bool useTimeline)
{
	m_useTimeline->setChecked(useTimeline);
}

void Timeline::setFps(int fps)
{
	m_fps->blockSignals(true);
	m_fps->setValue(fps);
	m_fps->blockSignals(false);
}

void Timeline::onUseTimelineClicked()
{
	net::EnvelopeBuilder eb;
	rustpile::write_setmetadataint(
		eb,
		0,
		uint8_t(rustpile::MetadataInt::UseTimeline),
		m_useTimeline->isChecked()
	);
	emit timelineEditCommand(eb.toEnvelope());
}

void Timeline::onFpsChanged()
{
	// TODO debounce
	net::EnvelopeBuilder eb;
	rustpile::write_setmetadataint(
		eb,
		0,
		uint8_t(rustpile::MetadataInt::Framerate),
		m_fps->value()
	);
	emit timelineEditCommand(eb.toEnvelope());
}

}