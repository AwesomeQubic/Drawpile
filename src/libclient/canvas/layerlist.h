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
#ifndef DP_NET_LAYERLIST_H
#define DP_NET_LAYERLIST_H

#include "acl.h"

#include <QAbstractItemModel>
#include <QMimeData>
#include <QVector>

#include <functional>

namespace net {
	class Envelope;
}

namespace rustpile {
	enum class Blendmode : uint8_t;
}

namespace canvas {

struct LayerListItem {
	//! Layer ID
	// Note: normally, layer ID range is from 0 to 0xffff, but internal
	// layers use values outside that range. However, internal layers are not
	// shown in the layer list.
	uint16_t id;
	
	//! Layer title
	QString title;
	
	//! Layer opacity
	float opacity;
	
	//! Blending mode
	rustpile::Blendmode blend;

	//! Layer hidden flag (local only)
	bool hidden;

	//! Layer is flagged for censoring
	bool censored;

	//! This is a fixed background/foreground layer
	bool fixed;

	//! Isolated (not pass-through) group?
	bool isolated;

	//! Is this a layer group?
	bool group;

	//! Number of child layers
	uint16_t children;

	//! Index in parent group
	uint16_t relIndex;

	//! Left index (MPTT)
	int left;

	//! Right index (MPTT)
	int right;

	//! Get the LayerAttributes flags as a bitfield
	uint8_t attributeFlags() const;

	//! Get the ID of the user who created this layer
	uint8_t creatorId() const { return uint8_t((id & 0xff00) >> 8); }
};

}

Q_DECLARE_TYPEINFO(canvas::LayerListItem, Q_MOVABLE_TYPE);

namespace canvas {

typedef std::function<QImage(int id)> GetLayerFunction;

class LayerListModel : public QAbstractItemModel {
	Q_OBJECT
	friend class LayerMimeData;
public:
	enum LayerListRoles {
		IdRole = Qt::UserRole + 1,
		TitleRole,
		IsDefaultRole,
		IsLockedRole,
		IsFixedRole
	};

	LayerListModel(QObject *parent=nullptr);
	
	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	Qt::DropActions supportedDropActions() const override;
	QStringList mimeTypes() const override;
	QMimeData *mimeData(const QModelIndexList& indexes) const override;
	bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
	QModelIndex index(int row, int column, const QModelIndex &parent=QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;

	QModelIndex layerIndex(uint16_t id);
	const QVector<LayerListItem> &layerItems() const { return m_items; }

	void previewOpacityChange(uint16_t id, float opacity) { emit layerOpacityPreview(id, opacity); }

	void setLayerGetter(GetLayerFunction fn) { m_getlayerfn = fn; }
	void setAclState(AclState *state) { m_aclstate = state; }

	/**
	 * Enable/disable any (not just own) layer autoselect requests
	 *
	 * When the local user hasn't yet drawn anything, any newly created layer
	 * should be selected.
	 */
	void setAutoselectAny(bool autoselect) { m_autoselectAny = autoselect; }

	/**
	 * @brief Get the default layer to select when logging in
	 * Zero means no default.
	 */
	uint16_t defaultLayer() const { return m_defaultLayer; }
	void setDefaultLayer(uint16_t id);

	/**
	 * @brief Find a free layer ID
	 * @return layer ID or 0 if all are taken
	 */
	int getAvailableLayerId() const;

	/**
	 * @brief Find a unique name for a layer
	 * @param basename
	 * @return unique name
	 */
	QString getAvailableLayerName(QString basename) const;

public slots:
	void setLayers(const QVector<LayerListItem> &items);

signals:
	void layersReordered();

	//! A new layer was created that should be automatically selected
	void autoSelectRequest(int);

	//! Emitted when layers are manually reordered
	void layerCommand(const net::Envelope &envelope);

	//! Request local change of layer opacity for preview purpose
	void layerOpacityPreview(int id, float opacity);

private:
	void handleMoveLayer(int idx, int afterIdx);

	int indexOf(uint16_t id) const;

	QVector<LayerListItem> m_items;
	GetLayerFunction m_getlayerfn;
	AclState *m_aclstate;
	int m_rootLayerCount;
	uint16_t m_defaultLayer;
	bool m_autoselectAny;
};

/**
 * A specialization of QMimeData for passing layers around inside
 * the application.
 */
class LayerMimeData : public QMimeData
{
Q_OBJECT
public:
	LayerMimeData(const LayerListModel *source, uint16_t id)
		: QMimeData(), m_source(source), m_id(id) {}

	const LayerListModel *source() const { return m_source; }

	uint16_t layerId() const { return m_id; }

	QStringList formats() const;

protected:
	QVariant retrieveData(const QString& mimeType, QVariant::Type type) const;

private:
	const LayerListModel *m_source;
	uint16_t m_id;
};

}

Q_DECLARE_METATYPE(canvas::LayerListItem)

#endif

