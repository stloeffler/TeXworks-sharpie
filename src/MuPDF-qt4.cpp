#include "MuPDF-qt4.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H

namespace MuPDF
{

typedef struct fz_qt4_draw_device_s fz_qt4_draw_device;

struct fz_qt4_draw_device_s
{
	fz_glyph_cache *cache;

	QPainter *painter;
	QStack<QPainterPath> * clipPaths; // needs to be a pointer; the object itself apparently can't live in a struct like this
};

/*
QColor fz_color_to_QColor(fz_colorspace *colorspace, float *color)
{
	float rgba[4];

	// Leave CMYK as is to (hopefully) avoid loss of precision / color
	// conversions when working with CMYK devices (e.g., printers)
	if (colorspace == fz_device_cmyk)
		return QColor::fromCmykF(color[0], color[1], color[2], color[3], 1.0 - color[4]);

	fz_convert_color(colorspace, color, fz_device_rgb, rgba);
	return QColor::fromRgbF(rgba[0], rgba[1], rgba[2], 1.0 - rgba[3]);
}
*/

QColor fz_color_to_QColor(fz_colorspace *colorspace, float *color, float alpha)
{
	float rgba[4];

	// Leave CMYK as is to (hopefully) avoid loss of precision / color
	// conversions when working with CMYK devices (e.g., printers)
	if (colorspace == fz_device_cmyk)
		return QColor::fromCmykF(color[0], color[1], color[2], color[3], alpha);

	fz_convert_color(colorspace, color, fz_device_rgb, rgba);
	return QColor::fromRgbF(rgba[0], rgba[1], rgba[2], alpha);
}


inline QTransform fz_matrix_to_QTransform(fz_matrix m)
{
	return QTransform(m.a, m.b, m.c, m.d, m.e, m.f);
}

QPainterPath fz_path_to_QPainterPath(fz_path * path, int even_odd)
{
	QPainterPath pp;
	int i;
	float x1, y1, x2, y2, x3, y3;
	
	pp.setFillRule(even_odd ? Qt::OddEvenFill : Qt::WindingFill);
	for (i = 0; i < path->len; ) {
		// Note: Always save the coordinates in temporary variables. Otherwise
		// the execution of i++ depends on the sequence the parameters are
		// passed to a function (usually in reverse!)
		// Note: The first item only defines the type, the v property has no
		// meaning; for subsequent items, the k property is meaningless
		switch (path->items[i++].k) {
			case FZ_MOVETO:
				x1 = path->items[i++].v;
				y1 = path->items[i++].v;
				pp.moveTo(x1, y1);
				break;
			case FZ_LINETO:
				x1 = path->items[i++].v;
				y1 = path->items[i++].v;
				pp.lineTo(x1, y1);
				break;
			case FZ_CURVETO:
				// **TODO:** Check if this is the right bezier according to pdf specs
				x1 = path->items[i++].v;
				y1 = path->items[i++].v;
				x2 = path->items[i++].v;
				y2 = path->items[i++].v;
				x3 = path->items[i++].v;
				y3 = path->items[i++].v;
				pp.cubicTo(x1, y1, x2, y2, x3, y3);
				break;
			case FZ_CLOSE_PATH:
				pp.closeSubpath();
				break;
		}
	}
	return pp;
}

QPainterPath FTOutline_to_QPainterPath(const FT_Outline & outline)
{
	// **TODO:** bits 2,5,6,7 of flags (drop out mode)

	// Processing runs through 3 steps for each contour:
	// 1) Find a suitable starting point on the curve (as the outline can start
	//    with off-curve bezier control points)
	// 2) Draw the lines and curves
	// 3) Close the contour (this must be handled separately as it can require
	//    wrapping to the first point(s))
	// Note that outline curves may come as 2nd or 3rd order Bezier splines, but
	// QPainterPath only understands 3rd order. Therefore, we must convert 2nd
	// order splines (see http://en.wikipedia.org/wiki/B%C3%A9zier_curve#Degree_elevation).
	int ic, ip;
	QPainterPath pp;
	
	
	for (ic = 0; ic < outline.n_contours; ++ic) {
		// **TODO:** saveguard ip
		// We construct our own variables points, tags, npoints which deal with
		// one contour only (i.e., point start at points[0], etc.) for
		// convenience
		FT_Vector* points;
		char* tags;
		int npoints;
		if (ic == 0) {
			points = outline.points;
			tags = outline.tags;
			npoints = outline.contours[0] + 1;
		}
		else {
			points = &(outline.points[outline.contours[ic - 1] + 1]);
			tags = &(outline.tags[outline.contours[ic - 1] + 1]);
			npoints = outline.contours[ic] - outline.contours[ic - 1];
		}

		// Find a suitable starting point (on the curve)
		if (tags[0] & 1) { // first point is on curve
			pp.moveTo(QPointF(points[0].x, points[0].y));
			ip = 1;
		}
		else if (tags[0] & 2) {
			// **TODO:** Third order bezier
			// Should not happen - 3rd order bezier should start with on-curve point
			continue;
		}
		else { // Second order bezier control point
			if (tags[1] & 1) { // the next point is on the curve - start there
				pp.moveTo(QPointF(points[1].x, points[1].y));
				ip = 2;
			}
			// **TODO:** Assert that tags[1] is 2nd order bezier point
			else { // the next point is a bezier control point as well - construct starting point inbetween
				pp.moveTo(0.5 * (QPointF(points[0].x, points[0].y) + QPointF(points[1].x, points[1].y)));
				ip = 1;
			}
		}
		
		while (ip < npoints) {
			QPointF p(points[ip].x, points[ip].y);
			if (tags[ip] & 1) { // point on curve
				pp.lineTo(p);
				++ip;
			}
			else if (tags[ip] & 2) { // 3rd order bezier control point off curve
				// We need two more points - otherwise we break out here and
				// handle wrapping separately
				if (ip + 2 >= npoints)
					break;
				// **TODO:** Assert that all points are 3rd order bezier points
				QPointF p1(points[ip + 0].x, points[ip + 0].y);
				QPointF p2(points[ip + 1].x, points[ip + 1].y);
				QPointF p3(points[ip + 2].x, points[ip + 2].y);
				pp.cubicTo(p1, p2, p3);
				ip += 3;
			}
			else { // 2nd order bezier control point off curve
				// We need one more point - otherwise we break out here and
				// handle wrapping separately
				if (ip + 1 >= npoints)
					break;
				// **TODO:** Assert that all points are 2nd order bezier points
				QPointF p0(pp.currentPosition());
				QPointF p1(points[ip].x, points[ip].y);
				QPointF p2(points[ip + 1].x, points[ip + 1].y);
				if (tags[ip + 1] & 1) {
					// Final point is on curve, therefore we have used it
					++ip;
				}
				else {
					// Final point is another bezier control point - 
					// construct point on the curve inbetween and don't
					// mark the following point as used
					p2 = 0.5 * (p1 + p2);
				}
				pp.cubicTo((p0 + 2 * p1) /3., (2 * p1 + p2) / 3., p2);
				++ip;
			}
		}
		
		// Close the path - this requires wrapping to the first point(s)!
		// Make sure we have a valid last point
		if (ip >= npoints)
			ip = npoints - 1;

		if (tags[ip] & 1) { // last point was on curve
			if (tags[0] & 1) // first point was on curve - draw a line
				pp.lineTo(QPointF(points[0].x, points[0].y));
			else { // first point was off curve bezier point
				if (tags[0] & 2) {
					// **TODO:** 3rd order bezier - can this happen?
				}
				else { // 2nd order bezier control point
					QPointF p0(pp.currentPosition());
					QPointF p1(points[0].x, points[0].y);
					QPointF p2;
					if (tags[1] & 1)
						p2 = QPointF(points[1].x, points[1].y);
					else
						p2 = QPointF(0.5 * (points[0].x + points[1].x), 0.5 * (points[0].y + points[1].y));
					pp.cubicTo((p0 + 2 * p1) /3., (2 * p1 + p2) / 3., p2);
				}
			}
		}
		else if (tags[ip] & 2) { // last point was off curve 3rd order bezier control point
			// **TODO:** Assert that the last two points were 3rd order bezier
			// control points and that the first point is on-curve
			if (ip == npoints - 2) {
				QPointF p1(points[ip + 0].x, points[ip + 0].y);
				QPointF p2(points[ip + 1].x, points[ip + 1].y);
				QPointF p3(points[0].x, points[0].y);
				pp.cubicTo(p1, p2, p3);
			}
		}
		else { // last point was off curve 2nd order bezier control point
			QPointF p0(pp.currentPosition());
			QPointF p1(points[ip].x, points[ip].y);
			if (tags[0] & 1) { // first point was on curve
				QPointF p2(points[0].x, points[0].y);
				pp.cubicTo((p0 + 2 * p1) /3., (2 * p1 + p2) / 3., p2);
			}
			else if (tags[0] & 2) { // first point was off curve 3rd order bezier point
				// **TODO:** this must not happen!
			}
			else { // first point was off curve 2nd order bezier point
				QPointF p2 = 0.5 * (p1 + QPoint(points[0].x, points[0].y));
				pp.cubicTo((p0 + 2 * p1) /3., (2 * p1 + p2) / 3., p2);
			}
		}
		pp.closeSubpath();
	}
	return pp;
}

#ifdef DEBUG
void print_fz_matrix(fz_matrix m)
{
	qDebug() << m.a << m.b;
	qDebug() << m.c << m.d;
	qDebug() << m.e << m.f;
}
#endif

/*
	int hints;
	int flags;

	void *user;
	void (*free_user)(void *);

	void (*fill_path)(void *, fz_path *, int even_odd, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroke_path)(void *, fz_path *, fz_stroke_state *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clip_path)(void *, fz_path *, fz_rect *rect, int even_odd, fz_matrix);
	void (*clip_stroke_path)(void *, fz_path *, fz_rect *rect, fz_stroke_state *, fz_matrix);

	void (*fill_text)(void *, fz_text *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroke_text)(void *, fz_text *, fz_stroke_state *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clip_text)(void *, fz_text *, fz_matrix, int accumulate);
	void (*clip_stroke_text)(void *, fz_text *, fz_stroke_state *, fz_matrix);
	void (*ignore_text)(void *, fz_text *, fz_matrix);

	void (*fill_shade)(void *, fz_shade *shd, fz_matrix ctm, float alpha);
	void (*fill_image)(void *, fz_pixmap *img, fz_matrix ctm, float alpha);
	void (*fill_image_mask)(void *, fz_pixmap *img, fz_matrix ctm, fz_colorspace *, float *color, float alpha);
	void (*clip_image_mask)(void *, fz_pixmap *img, fz_rect *rect, fz_matrix ctm);

	void (*pop_clip)(void *);

	void (*begin_mask)(void *, fz_rect, int luminosity, fz_colorspace *, float *bc);
	void (*end_mask)(void *);
	void (*begin_group)(void *, fz_rect, int isolated, int knockout, int blendmode, float alpha);
	void (*end_group)(void *);

	void (*begin_tile)(void *, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm);
	void (*end_tile)(void *);
*/



void fz_qt4_draw_free_user(void * user)
{
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	if (ddev->clipPaths)
		delete clipPaths;
	fz_free(ddev);
}

// Flood fill a path
void fz_qt4_draw_fill_path(void *user, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
#ifdef MU_DEBUG
	qDebug() << "fill_path";
#endif
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	ddev->painter->setTransform(fz_matrix_to_QTransform(ctm));
	
	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

	ddev->painter->setBrush(QBrush(fz_color_to_QColor(colorspace, color, alpha)));
	ddev->painter->setPen(QPen(Qt::NoPen));
	
	ddev->painter->drawPath(fz_path_to_QPainterPath(path, even_odd));
	
	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);
}

// Stroke a path
void fz_qt4_draw_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
#ifdef MU_DEBUG
	qDebug() << "stroke_path";
#endif
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	QPen pen(fz_color_to_QColor(colorspace, color, alpha));
	
	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

	ddev->painter->setTransform(fz_matrix_to_QTransform(ctm));

	pen.setWidthF(stroke->linewidth);
	pen.setMiterLimit(stroke->miterlimit);
	switch (stroke->linejoin) { // defined in <mupdf>/draw/draw_path.c
		case 0:
			pen.setJoinStyle(Qt::MiterJoin);
			break;
		case 1:
			pen.setJoinStyle(Qt::RoundJoin);
			break;
		case 2:
		default:
			pen.setJoinStyle(Qt::BevelJoin);
			break;
	}
	if (stroke->dash_len > 0) {
		QVector<qreal> dashPattern(stroke->dash_len);
		for (int i = 0; i < stroke->dash_len; ++i)
			dashPattern[i] = stroke->dash_list[i] / stroke->linewidth;
		pen.setDashPattern(dashPattern);
		pen.setDashOffset(stroke->dash_phase / stroke->linewidth);
	}
	// **TODO:** stroke->start_cap, stroke->dash_cap, stroke->end_cap

	ddev->painter->setBrush(Qt::NoBrush);
	ddev->painter->setPen(pen);
	
	ddev->painter->drawPath(fz_path_to_QPainterPath(path, 0));
	
	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);
}

// Fill (clip path) with shade (?)
void fz_qt4_draw_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	// **TODO:** Implement!
	qDebug() << "fill_shade [TODO]";
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	ddev->painter->setTransform(fz_matrix_to_QTransform(ctm));

	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

/*
	ddev->painter->setBrush(QBrush(fz_color_to_QColor(colorspace, color, alpha)));
	ddev->painter->setPen(QPen(Qt::NoPen));
	
	ddev->painter->drawPath(fz_path_to_QPainterPath(path, even_odd));
*/

	qDebug() << shade->mesh_len;
	for (int i = 0; i < shade->mesh_len; ++i)
		qDebug() << shade->mesh[i];

/*
struct fz_shade_s
{
	int refs;

	fz_rect bbox;		/* can be fz_infinite_rect * /
	fz_colorspace *colorspace;

	fz_matrix matrix;	/* matrix from pattern dict * /
	int use_background;	/* background color for fills but not 'sh' * /
	float background[FZ_MAX_COLORS];

	int use_function;
	float function[256][FZ_MAX_COLORS + 1];

	int type; /* linear, radial, mesh * /
	int extend[2];

	int mesh_len;
	int mesh_cap;
	float *mesh; /* [x y 0], [x y r], [x y t] or [x y c1 ... cn] * /
};
*/

	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);
}

////////////////////////////////////////////////////////////////////////////////
// Clipping
////////////////////////////////////////////////////////////////////////////////

// New clip path from filled path (?)
void fz_qt4_draw_clip_path(void *user, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm)
{
	qDebug() << "clip_path";
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	ddev->clipPaths->push(ddev->painter->clipPath());
	ddev->painter->setClipPath(fz_path_to_QPainterPath(path, even_odd));
}

// New clip path from path outline (?)
void fz_qt4_draw_clip_stroke_path(void *user, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
#ifdef MU_DEBUG
	qDebug() << "clip_stroke_path [TODO]";
#endif
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	
	qDebug() << fz_path_to_QPainterPath(path, 0);
}


void fz_qt4_draw_pop_clip(void *user)
{
#ifdef MU_DEBUG
	qDebug() << "pop_clip";
#endif
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	QPainterPath clipPath;
	
	if (!ddev->clipPaths->isEmpty())
		clipPath = ddev->clipPaths->pop();
	
	if (clipPath.isEmpty())
		ddev->painter->setClipping(false);
	else
		ddev->painter->setClipPath(clipPath);
}

////////////////////////////////////////////////////////////////////////////////
// Text
////////////////////////////////////////////////////////////////////////////////

void fz_qt4_draw_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	// **TODO:** Implement!
	qDebug() << "draw_fill_text [TODO]";
	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
	ddev->painter->setTransform(fz_matrix_to_QTransform(ctm));
//	ddev->painter->setTransform(fz_matrix_to_QTransform(text->trm), true);
	
	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

	ddev->painter->setBrush(QBrush(fz_color_to_QColor(colorspace, color, alpha)));
	ddev->painter->setPen(QPen(Qt::NoPen));

	if (text->font->ft_face) {
		// **TODO:** see <mupdf>/fitz/res_font.c @ fz_render_ft_glyph
		// **TODO:** Anti-aliasing, hinting, etc.
		FT_Face face = (FT_Face)text->font->ft_face;
		FT_Error fterr;


		// Snippet taken from fz_render_ft_glyph (<mupdf>/fitz/res_font.c)
		// To avoid rounding errors when loading glyphs at small sizes, we load
		// it larger and scale it afterwards
		FT_Matrix m;
		FT_Vector v;
		m.xx = text->trm.a * 64; /* should be 65536 */
		m.yx = text->trm.b * 64;
		m.xy = text->trm.c * 64;
		m.yy = text->trm.d * 64;
		v.x = text->trm.e * 64;
		v.y = text->trm.f * 64;
		fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
		if (fterr)
			qDebug() << "freetype setting character size:" << ft_error_string(fterr);
		FT_Set_Transform(face, &m, &v);
		// End snippet from fz_render_ft_glyph

		for (int i = 0; i < text->len; ++i) {
			// **TODO:** http://www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#FT_Load_Glyph
			// **TODO:** Maybe cache QPainterPaths for common glyphs?
			fterr = FT_Load_Glyph(face, text->items[i].gid, FT_LOAD_NO_BITMAP);
			if (fterr)
				qDebug() << "freetype load glyph (gid" << text->items[i].gid << "):" << ft_error_string(fterr);

			if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
				QPainterPath pp(FTOutline_to_QPainterPath(face->glyph->outline));

				QTransform tt = ddev->painter->transform();
				ddev->painter->translate(text->items[i].x, text->items[i].y);
				ddev->painter->scale(1. / 64., 1. / 64.);
				ddev->painter->drawPath(pp);
				ddev->painter->setTransform(tt);
			}
			else {
				// **TODO:** Bitmap fonts
			}
		}
	}
	else if (text->font->t3procs) {
		// **TODO:** Type3 fonts
	}
	else
		qDebug() << "assert: uninitialized font structure";

/*
struct fz_text_s
{
	fz_font *font;
	fz_matrix trm;
	int wmode;
	int len, cap;
	fz_text_item *items;
};
struct fz_text_item_s
{
	float x, y;
	int gid; /* -1 for one gid to many ucs mappings * /
	int ucs; /* -1 for one ucs to many gid mappings * /
};
struct fz_font_s
{
	int refs;
	char name[32];

	void *ft_face; /* has an FT_Face if used * /
	int ft_substitute; /* ... substitute metrics * /
	int ft_bold; /* ... synthesize bold * /
	int ft_italic; /* ... synthesize italic * /
	int ft_hint; /* ... force hinting for DynaLab fonts * /

	/* origin of font data * /
	char *ft_file;
	unsigned char *ft_data;
	int ft_size;

	fz_matrix t3matrix;
	fz_obj *t3resources;
	fz_buffer **t3procs; /* has 256 entries if used * /
	float *t3widths; /* has 256 entries if used * /
	void *t3xref; /* a pdf_xref for the callback * /
	fz_error (*t3run)(void *xref, fz_obj *resources, fz_buffer *contents,
		struct fz_device_s *dev, fz_matrix ctm);

	fz_rect bbox;

	/* substitute metrics * /
	int width_count;
	int *width_table;
};
*/


	
	// **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);
}

fz_device *fz_new_qt4_draw_device(fz_glyph_cache *cache, QPainter * painter)
{
	fz_device *dev;
	fz_qt4_draw_device *ddev = (fz_qt4_draw_device*)fz_malloc(sizeof(fz_qt4_draw_device));
	ddev->painter = painter;
	ddev->clipPaths = new QStack<QPainterPath>();

	qDebug() << "new device";


	dev = fz_new_device(ddev);
	dev->free_user = fz_qt4_draw_free_user;

	dev->fill_path = fz_qt4_draw_fill_path;
	dev->stroke_path = fz_qt4_draw_stroke_path;
	dev->clip_path = fz_qt4_draw_clip_path;
	dev->clip_stroke_path = fz_qt4_draw_clip_stroke_path;

	dev->fill_text = fz_qt4_draw_fill_text;
/*	dev->stroke_text = fz_qt4_draw_stroke_text;
	dev->clip_text = fz_qt4_draw_clip_text;
	dev->clip_stroke_text = fz_qt4_draw_clip_stroke_text;
	dev->ignore_text = fz_qt4_draw_ignore_text;

	dev->fill_image_mask = fz_qt4_draw_fill_image_mask;
	dev->clip_image_mask = fz_qt4_draw_clip_image_mask;
	dev->fill_image = fz_qt4_draw_fill_image;
*/
	dev->fill_shade = fz_qt4_draw_fill_shade;
	dev->pop_clip = fz_qt4_draw_pop_clip;
/*

	dev->begin_mask = fz_qt4_draw_begin_mask;
	dev->end_mask = fz_qt4_draw_end_mask;
	dev->begin_group = fz_qt4_draw_begin_group;
	dev->end_group = fz_qt4_draw_end_group;

	dev->begin_tile = fz_qt4_draw_begin_tile;
	dev->end_tile = fz_qt4_draw_end_tile;
*/
	return dev;
}

} // End MuPDF namespace
