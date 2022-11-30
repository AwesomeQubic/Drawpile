/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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
#include "canvassaverrunnable.h"
#include "canvas/paintengine.h"
#include "drawdance/drawcontextpool.h"
#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

extern "C" {
#include <dpengine/save.h>
}

namespace {

QString makeTemporaryPath(QString path)
{
	QFileInfo info{path};
	QString templateName = info.dir().filePath(
		info.baseName() + ".XXXXXX." + info.completeSuffix());
	QTemporaryFile tempFile{templateName};
	if(tempFile.open()) {
		return tempFile.fileName();
	} else {
		qWarning("Can't open temporary template '%s', writing to '%s' instead",
			qUtf8Printable(templateName), qUtf8Printable(path));
		return QString{};
	}
}

DP_SaveResult save(const canvas::PaintEngine *pe, QString path)
{
	QByteArray pathBytes = path.toUtf8();
	qDebug("Saving to '%s'", pathBytes.constData());
	drawdance::DrawContext dc = drawdance::DrawContextPool::acquire();
	return DP_save(pe->canvasState().get(), dc.get(), pathBytes.constData());
}

}

CanvasSaverRunnable::CanvasSaverRunnable(const canvas::PaintEngine *pe, const QString &filename, QObject *parent)
	: QObject(parent),
	  m_pe(pe),
	  m_filename(filename)
{
}

void CanvasSaverRunnable::run()
{
	DP_SaveResult result;
	QString tempPath = makeTemporaryPath(m_filename);
	if(tempPath.isEmpty()) {
		result = save(m_pe, m_filename);
	} else {
		result = save(m_pe, tempPath);
		if(result != DP_SAVE_RESULT_SUCCESS) {
			QFile::remove(tempPath);
		} else {
			qDebug("Renaming temporary '%s' to '%s'", qUtf8Printable(tempPath),
				qUtf8Printable(m_filename));
			QFile::remove(m_filename); // Qt won't rename over existing files.
			if(!QFile::rename(tempPath, m_filename)) {
				emit saveComplete(tr("Error moving temporary file %1 to %2.")
					.arg(tempPath, m_filename));
				return;
			}
		}
	}

	switch(result) {
    case DP_SAVE_RESULT_SUCCESS:
		emit saveComplete(QString{});
		break;
    case DP_SAVE_RESULT_BAD_ARGUMENTS:
		emit saveComplete(tr("Bad arguments, this is probably a bug in Drawpile."));
		break;
    case DP_SAVE_RESULT_NO_EXTENSION:
		emit saveComplete(tr("No file extension given."));
		break;
    case DP_SAVE_RESULT_UNKNOWN_FORMAT:
		emit saveComplete(tr("Unsupported format."));
		break;
	case DP_SAVE_RESULT_FLATTEN_ERROR:
		emit saveComplete(tr("Couldn't merge the canvas into a flat image."));
		break;
    case DP_SAVE_RESULT_OPEN_ERROR:
		emit saveComplete(tr("Couldn't open file for writing."));
		break;
    case DP_SAVE_RESULT_WRITE_ERROR:
		emit saveComplete(tr("Save operation failed, but the file might have been partially written."));
		break;
	default:
		emit saveComplete(tr("Unknown error."));
		break;
	}
}
