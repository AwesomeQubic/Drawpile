/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2021 Calle Laakkonen

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

#include "layerlist.h"
#include "net/envelopebuilder.h"
#include "../rustpile/rustpile.h"

#include <QDebug>
#include <QImage>
#include <QStringList>
#include <QRegularExpression>

namespace canvas {

LayerListModel::LayerListModel(QObject *parent)
	: QAbstractItemModel(parent), m_aclstate(nullptr),
	  m_rootLayerCount(0), m_defaultLayer(0), m_autoselectAny(true)
{
}

QVariant LayerListModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid())
		return QVariant();

	const LayerListItem &item = m_items.at(index.internalId());

	switch(role) {
	case Qt::DisplayRole: return QVariant::fromValue(item);
	case TitleRole:
	case Qt::EditRole: return item.title;
	case IdRole: return item.id;
	case IsDefaultRole: return item.id == m_defaultLayer;
	case IsLockedRole: return m_aclstate && m_aclstate->isLayerLocked(item.id);
	case IsFixedRole: return item.fixed;
	}

	return QVariant();
}

Qt::ItemFlags LayerListModel::flags(const QModelIndex& index) const
{
	if(!index.isValid())
		return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled;

	return Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

Qt::DropActions LayerListModel::supportedDropActions() const
{
	return Qt::MoveAction;
}

QStringList LayerListModel::mimeTypes() const {
		return QStringList() << "application/x-qt-image";
}

QMimeData *LayerListModel::mimeData(const QModelIndexList& indexes) const
{
	return new LayerMimeData(this, indexes[0].data().value<LayerListItem>().id);
}

bool LayerListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
	Q_UNUSED(action);
	Q_UNUSED(column);
	Q_UNUSED(parent);

	const LayerMimeData *ldata = qobject_cast<const LayerMimeData*>(data);
	if(ldata && ldata->source() == this) {
		// note: if row is -1, the item was dropped on the parent element, which in the
		// case of the list view means the empty area below the items.
		handleMoveLayer(indexOf(ldata->layerId()), row<0 ? m_items.count() : row);
	} else {
		// TODO support new layer drops
		qWarning("External layer drag&drop not supported");
	}
	return false;
}

void LayerListModel::handleMoveLayer(int oldIdx, int newIdx)
{
	// Need at least two layers for this to make sense
	const int count = m_items.count();
	if(count < 2)
		return;

	// If we're moving the layer to a higher index, take into
	// account that all previous indexes shift down by one.
	int adjustedNewIdx = newIdx > oldIdx ? newIdx - 1 : newIdx;

	if(oldIdx < 0 || oldIdx >= count || adjustedNewIdx < 0 || adjustedNewIdx >= count) {
		// This can happen when a layer is deleted while someone is drag&dropping it
		qWarning("Whoops, can't move layer from %d to %d because it was just deleted!", oldIdx, newIdx);
		return;
	}

	QVector<uint16_t> layers;
	layers.reserve(count * 2);
	for(const LayerListItem &li : qAsConst(m_items)) {
		layers.append(li.id);
		layers.append(0);
	}

	qInfo() << "old order" << layers;

	layers.move(2*oldIdx, 2*adjustedNewIdx);
	layers.move(2*oldIdx+1, 2*adjustedNewIdx+1);

	qInfo() << "new order" << layers;

	// Layers are shown topmost first in the list but
	// are sent bottom first in the protocol.
	std::reverse(layers.begin(), layers.end());



	Q_ASSERT(m_aclstate);
	net::EnvelopeBuilder eb;
	rustpile::write_layerorder(eb, m_aclstate->localUserId(), layers.constData(), layers.size());
	emit layerCommand(eb.toEnvelope());
}

int LayerListModel::indexOf(uint16_t id) const
{
	for(int i=0;i<m_items.size();++i)
		if(m_items.at(i).id == id)
			return i;
	return -1;
}

QModelIndex LayerListModel::layerIndex(uint16_t id)
{
	int i = indexOf(id);
	if(i>=0) {
		return createIndex(m_items.at(i).relIndex, 0, i);
	}
	return QModelIndex();
}

int LayerListModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return m_items.at(parent.internalId()).children;
	}

	return m_rootLayerCount;
}

int LayerListModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 1;
}

QModelIndex LayerListModel::parent(const QModelIndex &index) const
{
	if(!index.isValid())
		return QModelIndex();

	int seek = index.internalId();

	const int right = m_items.at(seek).right;
	while(--seek >= 0) {
		if(m_items.at(seek).right > right) {
			return createIndex(m_items.at(seek).relIndex, 0, seek);
		}
	}

	return QModelIndex();
}

QModelIndex LayerListModel::index(int row, int column, const QModelIndex &parent) const
{
	if(m_items.isEmpty() || row < 0 || column != 0)
		return QModelIndex();

	int cursor;

	if(parent.isValid()) {
		cursor = parent.internalId();
		if(row >= m_items.at(cursor).children)
			return QModelIndex();

		cursor += 1; // point to the first child element

	} else {
		if(row >= m_rootLayerCount)
			return QModelIndex();

		cursor = 0;
	}

	int next = m_items.at(cursor).right + 1;

	int i = 0;
	while(i < row) {
		while(cursor < m_items.size() && m_items.at(cursor).left < next)
			++cursor;

		if(cursor == m_items.size() || m_items.at(cursor).left > next)
			return QModelIndex();

		next = m_items.at(cursor).right + 1;
		++i;
	}

#if 0
	qInfo("index(row=%d), parent=%d (%d), relIndex=%d, cursor=%d, left=%d, right=%d",
		  row,
		  parent.row(), int(parent.internalId()),
		  m_items.at(cursor).relIndex, cursor,
		  m_items.at(cursor).left, m_items.at(cursor).right
		  );
#endif

	Q_ASSERT(m_items.at(cursor).relIndex == row);

	return createIndex(row, column, cursor);
}

void LayerListModel::setLayers(const QVector<LayerListItem> &items)
{
	// See if there are any new layers we should autoselect
	int autoselect = -1;

	const uint8_t localUser = m_aclstate ? m_aclstate->localUserId() : 0;

	if(m_items.size() < items.size()) {
		for(const LayerListItem &newItem : items) {
			// O(n²) loop but the number of layers is typically small enough that
			// it doesn't matter
			bool isNew = true;
			for(const LayerListItem &oldItem : qAsConst(m_items)) {
				if(oldItem.id == newItem.id) {
					isNew = false;
					break;
				}
			}
			if(!isNew)
				continue;

			// Autoselection rules:
			// 1. If we haven't participated yet, and there is a default layer,
			//    only select the default layer
			// 2. If we haven't participated in the session yet, select any new layer
			// 3. Otherwise, select any new layer that was created by us
			// TODO implement the other rules
			if(
					newItem.creatorId() == localUser ||
					(m_autoselectAny && (
						 (m_defaultLayer>0 && newItem.id == m_defaultLayer)
						 || m_defaultLayer==0
						 )
					 )
				) {
				autoselect = newItem.id;
				break;
			}
		}
	}

	// Count root layers
	int rootLayers = 0;
	if(!items.isEmpty()) {
		++rootLayers;
		int next = items[0].right + 1;
		for(int i=1;i<items.length();++i) {
			if(items[i].left == next) {
				++rootLayers;
				next = items[i].right + 1;
			}
		}
	}

	beginResetModel();
	m_rootLayerCount = rootLayers;
	m_items = items;
	endResetModel();

	if(autoselect>=0)
		emit autoSelectRequest(autoselect);
}

void LayerListModel::setDefaultLayer(uint16_t id)
{
#if 0 // FIXME
	const int oldIdx = indexOf(m_defaultLayer);
	if(oldIdx >= 0) {
		emit dataChanged(index(oldIdx), index(oldIdx), QVector<int>() << IsDefaultRole);
	}

	m_defaultLayer = id;
	const int newIdx = indexOf(id);
	if(newIdx >= 0) {
		emit dataChanged(index(newIdx), index(newIdx), QVector<int>() << IsDefaultRole);
	}
#endif
}

QStringList LayerMimeData::formats() const
{
	return QStringList() << "application/x-qt-image";
}

QVariant LayerMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const
{
	Q_UNUSED(mimeType);
	if(type==QVariant::Image) {
		if(m_source->m_getlayerfn) {
			return m_source->m_getlayerfn(m_id);
		}
	}

	return QVariant();
}

int LayerListModel::getAvailableLayerId() const
{
	Q_ASSERT(m_aclstate);

	const int prefix = int(m_aclstate->localUserId()) << 8;
	QList<int> takenIds;
	for(const LayerListItem &item : m_items) {
		if((item.id & 0xff00) == prefix)
			takenIds.append(item.id);
	}

	for(int i=0;i<256;++i) {
		int id = prefix | i;
		if(!takenIds.contains(id))
			return id;
	}

	return 0;
}

QString LayerListModel::getAvailableLayerName(QString basename) const
{
	// Return a layer name of format "basename n" where n is one bigger than the
	// biggest suffix number of layers named "basename n".

	// First, strip suffix number from the basename (if it exists)

	QRegularExpression suffixNumRe("(\\d+)$");
	{
		auto m = suffixNumRe.match(basename);
		if(m.hasMatch()) {
			basename = basename.mid(0, m.capturedStart()).trimmed();
		}
	}

	// Find the biggest suffix in the layer stack
	int suffix = 0;
	for(const LayerListItem &l : m_items) {
		auto m = suffixNumRe.match(l.title);
		if(m.hasMatch()) {
			if(l.title.startsWith(basename)) {
				suffix = qMax(suffix, m.captured(1).toInt());
			}
		}
	}

	// Make unique name
	return QString("%2 %1").arg(suffix+1).arg(basename);
}

uint8_t LayerListItem::attributeFlags() const
{
	return (censored ? rustpile::LayerAttributesMessage_FLAGS_CENSOR : 0) |
		   (fixed ? rustpile::LayerAttributesMessage_FLAGS_FIXED : 0) |
			(isolated ? rustpile::LayerAttributesMessage_FLAGS_ISOLATED : 0)
			;
}

}

