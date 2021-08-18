/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#include "core/brushmask.h"

namespace brushes {

template<typename T> static T square(T x) { return x*x; }

paintcore::BrushMask makeRoundPixelBrushMask(int diameter, uchar opacity)
{
	const qreal radius = diameter/2.0;
	const qreal rr = square(radius);

	QVector<uchar> data(square(diameter), 0);
	uchar *ptr = data.data();

	const qreal offset = 0.5;
	for(int y=0;y<diameter;++y) {
		const qreal yy = square(y-radius+offset);
		for(int x=0;x<diameter;++x,++ptr) {
			const qreal xx = square(x-radius+offset);

			if(yy+xx <= rr)
				*ptr = opacity;
		}
	}
	return paintcore::BrushMask(diameter, data);
}

paintcore::BrushMask makeSquarePixelBrushMask(int diameter, uchar opacity)
{
	return paintcore::BrushMask(diameter, QVector<uchar>(square(diameter), opacity));
}

}
