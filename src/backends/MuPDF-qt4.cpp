#include "MuPDF-qt4.h"
#include <ft2build.h>
#include FT_FREETYPE_H

namespace QtPDF {

namespace Backend {

namespace MuPDF
{

typedef struct fz_qt4_draw_device_s fz_qt4_draw_device;
typedef struct draw_surface_s draw_surface;

struct draw_surface_s
{
  enum Type { BASE, CLIP_PATH, IMAGE_MASK, TILING, MASK } type;
  QPainter * painter;
  QTransform globalTransform;

  // for CLIP_PATH
  QPainterPath clipPath;

  // for IMAGE_MASK
  QImage * mask;

  // for TILLING
  float xStep, yStep;
  QRectF area;
};

struct fz_qt4_draw_device_s
{
  fz_glyph_cache *cache;

  draw_surface * surface;
  QStack<draw_surface *> * surfaceStack;
};

void resetTransform(draw_surface * s)
{
  if (!s || !s->painter)
    return;
  s->painter->setTransform(s->globalTransform, false);
}

void setTransform(draw_surface * s, const QTransform & transform, const bool combine = false)
{
  if (!s || !s->painter)
    return;
  if (!combine)
    s->painter->setTransform(s->globalTransform, false);
  s->painter->setTransform(transform, true);
}

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

QRectF toRectF(const fz_rect r)
{
  return QRectF(QPointF(r.x0, r.y0), QPointF(r.x1, r.y1));
}

QRectF toRectF(const fz_bbox r)
{
  return QRectF(QPointF(r.x0, r.y0), QPointF(r.x1, r.y1));
}

fz_rect toFzRect(const QRectF r)
{
  fz_rect retVal;
  retVal.x0 = r.left();
  retVal.y0 = r.top();
  retVal.x1 = r.right();
  retVal.y1 = r.bottom();
  return retVal;
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




// Renders `text` to a list of QPainterPaths. The returned list contains one
// QPainterPath per glyph (combining all paths to a single, big one causes
// severe performance issues when painting due to fill winding rules)
QList<QPainterPath> render_text(fz_text *text)
{
  QList<QPainterPath> retVal;
  static QTransform rescale = QTransform::fromScale(1. / 64., 1. / 64.);

  if (text->font->ft_face) {
    FT_Face face = (FT_Face)text->font->ft_face;
    FT_Error fterr;

    for (int i = 0; i < text->len; ++i) {
      // **TODO:** see <mupdf>/fitz/res_font.c @ fz_render_ft_glyph
      // for substituted fonts, we should stretch the glyphs
      // fz_matrix trm = fz_adjust_ft_glyph_width(text->font, text->items[i].gid, text->trm);
      fz_matrix trm = text->trm;

      // Snippet taken from fz_render_ft_glyph (<mupdf>/fitz/res_font.c)
      // To avoid rounding errors when loading glyphs at small sizes, we load
      // it larger and scale it afterwards
      FT_Matrix m;
      FT_Vector v;
      m.xx = trm.a * 64; /* should be 65536 */
      m.yx = trm.b * 64;
      m.xy = trm.c * 64;
      m.yy = trm.d * 64;
      v.x = trm.e * 64;
      v.y = trm.f * 64;
      fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); /* should be 64, 64 */
      if (fterr)
        qDebug() << "freetype setting character size:" << ft_error_string(fterr);
      FT_Set_Transform(face, &m, &v);
      // FIXME: font->ft_italic, font->ft_bold, font->ft_hint
      // End snippet from fz_render_ft_glyph

      // **TODO:** http://www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#FT_Load_Glyph
      // **TODO:** Maybe cache QPainterPaths for common glyphs?
      fterr = FT_Load_Glyph(face, text->items[i].gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
      if (fterr)
        qDebug() << "freetype load glyph (gid" << text->items[i].gid << "):" << ft_error_string(fterr);

      if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        QPainterPath pp(FTOutline_to_QPainterPath(face->glyph->outline));

        pp.translate(64 * text->items[i].x, 64 * text->items[i].y);
        retVal << pp * rescale;
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

  return retVal;
}



void fz_qt4_draw_free_user(void * user)
{
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  // **TODO:** If the stack is not empty, each individual surface should be
  // freed (e.g., the QPainter or the attached QPaintDevice, unless they are
  // shared with BASE)
  if (ddev->surfaceStack)
    delete ddev->surfaceStack;
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
  ddev->surface->painter->save();
  setTransform(ddev->surface, fz_matrix_to_QTransform(ctm));

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

  ddev->surface->painter->setBrush(QBrush(fz_color_to_QColor(colorspace, color, alpha)));
  ddev->surface->painter->setPen(QPen(Qt::NoPen));

  ddev->surface->painter->drawPath(fz_path_to_QPainterPath(path, even_odd));

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);

  ddev->surface->painter->restore();
}

QPen fz_stroke_state_to_QPen(fz_stroke_state *stroke, fz_colorspace *colorspace, float *color, float alpha)
{
  QPen pen(fz_color_to_QColor(colorspace, color, alpha));

  pen.setWidthF(stroke->linewidth);
  // **TODO:** Miter in Qt is different from miter in PDF.
  // The miter limit in PDF imposes a strict limit above which the whole join
  // is converted into a bevel join.
  // The miter limit in Qt simply clips the join at a certain distance (which
  // is (roughly) half the PDF limit. It does not convert the join to a bevel
  // join, however, and thus gives a different visual appearance;
  // The SvgMiterJoin seems to behave just like the PDF version, though
  // **TODO:** Check miter definition in http://www.w3.org/TR/SVGMobile12/
  // **TODO:** Check since which version SvgMiterJoin is available in Qt
  pen.setMiterLimit(stroke->miterlimit);
  switch (stroke->linejoin) { // defined in <mupdf>/draw/draw_path.c
    case 0:
      pen.setJoinStyle(Qt::SvgMiterJoin);
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
    float linewidth = stroke->linewidth;
    // **TODO:** For linewidth == 0, Qt paints the line 1px wide regardless
    // of magnification! This means that the dash pattern does not scale
    // with magnification as it should (should it?). However, linewidth == 0
    // is not recommended by the pdf specs (as its appearance is device-
    // dependent) and dash patterns for 0-width lines are not specified.
    if (linewidth == 0.0f)
      linewidth = 1;
    for (int i = 0; i < stroke->dash_len; ++i)
      dashPattern[i] = stroke->dash_list[i] / linewidth;
    // Qt requires an even number of entries in dashPattern; if the PDF
    // gives us an odd number, we simply cycle through the hold list
    if (dashPattern.size() % 2 == 1)
      dashPattern << dashPattern;
    pen.setDashPattern(dashPattern);
    pen.setDashOffset(stroke->dash_phase / linewidth);
  }
  // **TODO:** stroke->dash_cap, stroke->end_cap
  // According to PDF specs, there is just one line cap setting valid for all
  // (start, dash, end) caps. Why does MuPDF have three? Can they be different?
  switch (stroke->start_cap) {
    default:
    case 0:
      pen.setCapStyle(Qt::FlatCap);
      break;
    case 1:
      pen.setCapStyle(Qt::RoundCap);
      break;
    case 2:
      pen.setCapStyle(Qt::SquareCap);
      break;
  }

  return pen;
}

// Stroke a path
void fz_qt4_draw_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
  fz_colorspace *colorspace, float *color, float alpha)
{
#ifdef MU_DEBUG
  qDebug() << "stroke_path";
#endif
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

  ddev->surface->painter->save();
  setTransform(ddev->surface, fz_matrix_to_QTransform(ctm));

  ddev->surface->painter->setBrush(Qt::NoBrush);
  ddev->surface->painter->setPen(fz_stroke_state_to_QPen(stroke, colorspace, color, alpha));

  ddev->surface->painter->drawPath(fz_path_to_QPainterPath(path, 0));

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);

  ddev->surface->painter->restore();
}

// Fill (clip path) with shade (?)
void fz_qt4_draw_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
//	static int round = -1;
//	if (++round > 0)
//		return;

  // **TODO:** Implement!
  qDebug() << "fill_shade [TODO]";
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  setTransform(ddev->surface, fz_matrix_to_QTransform(ctm));

  // Get the rectangle to paint in
  // Note: controlPointRect() is significantly faster than boundingRect(),
  // according to Qt docs
  QRectF rect(ddev->surface->painter->clipPath().controlPointRect());
  if (rect.isEmpty()) {
    // Without valid clipping path, take the whole page rect
    // FIXME: Does this work with a globalTransform?
    QRectF deviceRect(0, 0, ddev->surface->painter->device()->width(), ddev->surface->painter->device()->height());
    rect = fz_matrix_to_QTransform(ctm).mapRect(ddev->surface->painter->deviceTransform().inverted().mapRect(deviceRect));
  }

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

  // **TODO:** Use shade->bbox, shade->matrix, shade->extend
  if (shade->use_background)
    ddev->surface->painter->fillRect(rect, fz_color_to_QColor(shade->colorspace, shade->background, alpha));

  // **TODO:** Check if shading is really done only if shade->use_function == 1
  // (see <mupdf>/draw/draw_mesh.c @ fz_paint_shade()
  if (!shade->use_function)
    return;

  QTransform t(fz_matrix_to_QTransform(shade->matrix));
  switch (shade->type) {
    case FZ_LINEAR:
      {
        QLinearGradient gradient;
        // If shade->extend[] == 0, add a dummy stop with the same color
        // but full transparency
        const qreal offset = 1e-6;
        QColor stop;
        for (int i = 0; i < 256; ++i)
          gradient.setColorAt((1. - 2. * offset) * i / 255. + offset, fz_color_to_QColor(shade->colorspace, shade->function[i], alpha));

        stop = fz_color_to_QColor(shade->colorspace, shade->function[0], alpha);
        if (shade->extend[0] == 0)
          stop.setAlphaF(0);
        gradient.setColorAt(0, stop);
        stop = fz_color_to_QColor(shade->colorspace, shade->function[255], alpha);
        if (shade->extend[1] == 0)
          stop.setAlphaF(0);
        gradient.setColorAt(1, stop);

        gradient.setStart(t.map(QPointF(shade->mesh[0], shade->mesh[1])));
        gradient.setFinalStop(t.map(QPointF(shade->mesh[3], shade->mesh[4])));
        ddev->surface->painter->fillRect(rect, gradient);
      }
      break;
    case FZ_RADIAL:
      {
        QRadialGradient gradient;
        // If shade->extend[] == 0, add a dummy stop with the same color
        // but full transparency
        const qreal offset = 1e-6;
        QColor stop;
        for (int i = 0; i < 256; ++i)
          gradient.setColorAt((1. - 2. * offset) * i / 255. + offset, fz_color_to_QColor(shade->colorspace, shade->function[i], alpha));

        stop = fz_color_to_QColor(shade->colorspace, shade->function[0], alpha);
        if (shade->extend[0] == 0)
          stop.setAlphaF(0);
        gradient.setColorAt(0, stop);
        stop = fz_color_to_QColor(shade->colorspace, shade->function[255], alpha);
        if (shade->extend[1] == 0)
          stop.setAlphaF(0);
        gradient.setColorAt(1, stop);

        // **TODO:** Handle radial shading that is not rotationally
        // symmetric (i.e., scaled differently in x and y (elliptical
        // pattern, possibly rotated))

        gradient.setFocalPoint(t.map(QPointF(shade->mesh[0], shade->mesh[1])));
        gradient.setCenter(t.map(QPointF(shade->mesh[3], shade->mesh[4])));
        gradient.setRadius(shade->mesh[5]);

        ddev->surface->painter->fillRect(rect, gradient);
      }
      break;
    case FZ_MESH:
      qDebug() << "[TODO] Mesh shading";
      // **TODO:** Mesh shading
      break;
  }

/*
struct fz_shade_s
{
  int refs;

  fz_rect bbox;		/ * can be fz_infinite_rect * /
  fz_colorspace *colorspace;

  fz_matrix matrix;	/ * matrix from pattern dict * /
  int use_background;	/ * background color for fills but not 'sh' * /
  float background[FZ_MAX_COLORS];

  int use_function;
  float function[256][FZ_MAX_COLORS + 1];

  int type; / * linear, radial, mesh * /
  int extend[2];

  int mesh_len;
  int mesh_cap;
  float *mesh; / * [x y 0], [x y r], [x y t] or [x y c1 ... cn] * /
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
//return;
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  // Always push & pop paths without transformation or else recursive calls
  // would mess transformations up real bad
  QTransform oldT(ddev->surface->painter->transform());
  resetTransform(ddev->surface);

  draw_surface * oldSurface = ddev->surface;
  ddev->surfaceStack->push(oldSurface);
  ddev->surface = new draw_surface;
  ddev->surface->globalTransform = oldSurface->globalTransform;
  ddev->surface->painter = oldSurface->painter;
  ddev->surface->type = draw_surface::CLIP_PATH;
  QTransform t(fz_matrix_to_QTransform(ctm));


  // **TODO:** rect
/*
#ifdef MU_DEBUG
  // Draw the clip path (disable clipping before and reenable it afterwards if
  // necessary)
  if (ddev->surface->painter->hasClipping()) {
    QPainterPath oldClipPath = ddev->surface->painter->clipPath();
    ddev->surface->painter->setClipping(false);
    ddev->surface->painter->setPen(QPen(2));
    ddev->surface->painter->setBrush(Qt::NoBrush);
    ddev->surface->painter->drawPath(oldClipPath.intersected(t.map(fz_path_to_QPainterPath(path, even_odd))));
    ddev->surface->painter->setClipPath(oldClipPath);
  }
  else {
    ddev->surface->painter->drawPath(t.map(fz_path_to_QPainterPath(path, even_odd)));
  }
#endif
*/
  ddev->surface->painter->setClipPath(t.map(fz_path_to_QPainterPath(path, even_odd)), Qt::IntersectClip);
  ddev->surface->clipPath = ddev->surface->painter->clipPath();

  ddev->surface->painter->setTransform(oldT);
}

// New clip path from path outline (?)
void fz_qt4_draw_clip_stroke_path(void *user, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
  // **TODO:** Is this actually possible in PDFs?
#ifdef MU_DEBUG
  qDebug() << "clip_stroke_path [TODO]";
#endif
//	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;

  // QPainterPathStroker

//	qDebug() << fz_path_to_QPainterPath(path, 0);
}


void fz_qt4_draw_pop_clip(void *user)
{
#ifdef MU_DEBUG
  qDebug() << "pop_clip";
#endif
//return;
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  QPainterPath clipPath;
  QImage * img;

  if (ddev->surfaceStack->isEmpty()) {
    qDebug() << "[WAR] Clipping stack empty";
    return;
  }

  switch (ddev->surface->type) {
    case draw_surface::CLIP_PATH:
    {
      delete ddev->surface;
      ddev->surface = ddev->surfaceStack->pop();

      if (ddev->surface->clipPath.isEmpty())
        ddev->surface->painter->setClipping(false);
      else {
        // Always push & pop paths without transformation or else recursive calls
        // would mess transformations up real bad
        QTransform oldT(ddev->surface->painter->transform());
        resetTransform(ddev->surface);
        ddev->surface->painter->setClipPath(ddev->surface->clipPath);
        ddev->surface->painter->setTransform(oldT);
      }
      break;
    }
    case draw_surface::IMAGE_MASK:
    {
      draw_surface * oldSurface = ddev->surface;

      oldSurface->painter->end();
      img = static_cast<QImage*>(oldSurface->painter->device());

      // If oldSurface->mask is valid and of the correct size, apply it
      // to the image
      if (oldSurface->mask && img->width() == oldSurface->mask->width() && img->height() == oldSurface->mask->height()) {
        uchar * imgBits, * maskBits;
        imgBits = img->bits();
        // TODO: Possibly use oldSurface->mask->constBits iff Qt >= 4.7
        maskBits = oldSurface->mask->bits();

        for (int i = 0; i < img->width() * img->height(); ++i) {
          // Note: Since img has format QImage::Format_ARGB32_Premultiplied,
          // we need to multiply all channels by the mask's alpha channel
          imgBits[4 * i + 0] = imgBits[4 * i + 0] * maskBits[4 * i + 3] / 255;
          imgBits[4 * i + 1] = imgBits[4 * i + 1] * maskBits[4 * i + 3] / 255;
          imgBits[4 * i + 2] = imgBits[4 * i + 2] * maskBits[4 * i + 3] / 255;
          imgBits[4 * i + 3] = imgBits[4 * i + 3] * maskBits[4 * i + 3] / 255;
        }
      }

      ddev->surface = ddev->surfaceStack->pop();

      if (ddev->surface->clipPath.isEmpty())
        ddev->surface->painter->setClipping(false);
      else {
        // Always push & pop paths without transformation or else recursive calls
        // would mess transformations up real bad
        QTransform oldT(ddev->surface->painter->transform());
        resetTransform(ddev->surface);
        ddev->surface->painter->setClipPath(ddev->surface->clipPath);
        ddev->surface->painter->setTransform(oldT);
      }

      ddev->surface->painter->drawImage(QPointF(0, 0), *img);

      // was save()'d in clip_image_mask
      ddev->surface->painter->restore();

      delete img;
      delete oldSurface->mask;
      delete oldSurface;
      break;
    }
    default:
      qDebug() << "[WAR] PopClip without PushClip";
      break;
  }


}

void fz_qt4_draw_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
  // **TODO:** Implement!
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  // Always push & pop paths without transformation or else recursive calls
  // would mess transformations up real bad
  QTransform oldT(ddev->surface->painter->transform());
  draw_surface * oldSurface = ddev->surface;
  resetTransform(ddev->surface);
  ddev->surfaceStack->push(oldSurface);

  ddev->surface = new draw_surface;
  ddev->surface->painter = oldSurface->painter;
  ddev->surface->type = draw_surface::CLIP_PATH;
  ddev->surface->globalTransform = oldSurface->globalTransform;
  QTransform t(fz_matrix_to_QTransform(ctm));

  // **TODO:** accumulate
/*
#ifdef MU_DEBUG
  // Draw the clip path (disable clipping before and reenable it afterwards if
  // necessary)
  if (ddev->painter->hasClipping()) {
    QPainterPath oldClipPath = ddev->painter->clipPath();
    ddev->painter->setClipping(false);
    ddev->painter->setPen(QPen(2));
    ddev->painter->setBrush(Qt::NoBrush);
    ddev->painter->drawPath(oldClipPath.intersected(t.map(fz_path_to_QPainterPath(path, even_odd))));
    ddev->painter->setClipPath(oldClipPath);
  }
  else {
    ddev->painter->drawPath(t.map(fz_path_to_QPainterPath(path, even_odd)));
  }
#endif
*/

  QPainterPath clipPath;
  QList<QPainterPath> textPPs(render_text(text));
  // **TODO:** Should this be addPath() or union()?
  foreach (QPainterPath textPP, textPPs)
    clipPath.addPath(textPP);

  ddev->surface->painter->setClipPath(t.map(clipPath), Qt::IntersectClip);
  ddev->surface->clipPath = ddev->surface->painter->clipPath();

  ddev->surface->painter->setTransform(oldT);
}

void fz_qt4_draw_clip_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
  // **TODO:** Is this actually possible in PDFs?
  qDebug() << "clip_stroke_text [TODO]";
//	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;

  // QPainterPathStroker
}

void fz_qt4_draw_clip_image_mask(void *user, fz_pixmap *image, fz_rect *rect, fz_matrix ctm)
{
  uchar * maskData;
#ifdef MU_DEBUG
  qDebug() << "clip_image_mask";
#endif
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  draw_surface * oldSurface = ddev->surface;
  ddev->surfaceStack->push(oldSurface);
  ddev->surface = new draw_surface;
  ddev->surface->painter = new QPainter();
  ddev->surface->type = draw_surface::IMAGE_MASK;

  fz_bbox bbox;
  bbox = fz_round_rect(fz_transform_rect(ctm, fz_unit_rect));
  if (rect)
    bbox = fz_intersect_bbox(bbox, fz_round_rect(*rect));

  oldSurface->painter->save();
  // Ensure we're painting at the right position in pop_clip
  setTransform(oldSurface, QTransform::fromTranslate(bbox.x0, bbox.y0), true);

  maskData = new uchar[4 * image->w * image->h];
  for (int i = 0; i < image->w * image->h; ++i) {
    maskData[4 * i + 0] = 0;
    maskData[4 * i + 1] = 0;
    maskData[4 * i + 2] = 0;
    maskData[4 * i + 3] = image->samples[i];
  }

  // Create a QImage that shares data with the fz_pixmap.
  QImage tmp_image(maskData, image->w, image->h, QImage::Format_ARGB32_Premultiplied);

  // Transform mask so it can be painted directly in pop_clip
  ddev->surface->mask = new QImage(tmp_image.mirrored().scaled(QSize(bbox.x1 - bbox.x0, bbox.y1 - bbox.y0), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

  delete[] maskData;

  QImage * img = new QImage(bbox.x1 - bbox.x0, bbox.y1 - bbox.y0, QImage::Format_ARGB32_Premultiplied);

  // Set background to be transparent white (with Format_ARGB32_Premultiplied,
  // this is actually identical to Qt::transparent)
  img->fill(QColor(255, 255, 255, 0));

  ddev->surface->painter->begin(img);

  // Shift everything in the new painter so that we only need to store a rect
  // of size bbox
  ddev->surface->globalTransform = oldSurface->globalTransform;
  ddev->surface->globalTransform.translate(-bbox.x0, -bbox.y0);
  resetTransform(ddev->surface);
}

void fz_qt4_draw_fill_image(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  fz_pixmap * converted = NULL;

  // what about alpha masks? <mupdf>/draw/draw_device.c @ fz_draw_fill_image()
  if (image->w == 0 || image->h == 0)
    return;

  qDebug() << "draw_fill_image";

//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

  // Convert colorspace if necessary (QImage can only handle fz_device_bgr)
  if (image->colorspace != fz_device_bgr) {
    converted = fz_new_pixmap_with_rect(fz_device_bgr, fz_bound_pixmap(image));
    fz_convert_pixmap(image, converted);
    image = converted;
  }
  // Create a QImage that shares data with the fz_pixmap.
  QImage tmp_image(image->samples, image->w, image->h, QImage::Format_ARGB32);

  // Set the transformation matrix
  // Note: the rotation/scale part works on normalized image coordinates and
  // is upside-down (as usual with pdf coordinates)

  ddev->surface->painter->save();
  setTransform(ddev->surface, fz_matrix_to_QTransform(ctm).scale(1./image->w, -1./image->h).translate(0, -image->h));
  ddev->surface->painter->drawImage(QPointF(0, 0), tmp_image);
  ddev->surface->painter->restore();

//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);

  if (converted)
    fz_drop_pixmap(converted);
}

void fz_qt4_draw_begin_mask(void *user, fz_rect area, int luminosity, fz_colorspace *colorspace, float *colorfv)
{
#ifdef MU_DEBUG
  qDebug() << "begin_mask";
#endif
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  draw_surface * oldSurface = ddev->surface;
  fz_bbox bbox;

  // For whatever reason, MuPDF seems to assume an identity CTM in begin_mask
  resetTransform(oldSurface);

  ddev->surfaceStack->push(oldSurface);

  // Create a new surface on which the mask can be painted
  ddev->surface = new draw_surface;
  ddev->surface->painter = new QPainter();
  ddev->surface->type = draw_surface::MASK;
  ddev->surface->area = toRectF(area);

  bbox = fz_round_rect(area);

  // We don't support an alpha channel for the mask for now (if it is actually
  // necessary, it should be fairly easy to add)
  QImage * img = new QImage(bbox.x1 - bbox.x0, bbox.y1 - bbox.y0, QImage::Format_RGB32);
//	QImage * img = new QImage(oldSurface->painter->device()->width(), oldSurface->painter->device()->height(), QImage::Format_RGB32);

  if (luminosity) {
    float bc;
    if (!colorspace)
      colorspace = fz_device_gray;
    fz_convert_color(colorspace, colorfv, fz_device_gray, &bc);
    unsigned char bcByte = (unsigned char)(bc * 255);
    img->fill(bcByte << 16 | bcByte << 8 | bcByte);
  }
  else {
    // Set background to be transparent black
    img->fill(Qt::transparent);
  }

  ddev->surface->painter->begin(img);

  // Shift (and scale) everything in the new painter so that we only need to
  // store a rect of size bbox and can paint to it with the normal functions
  // (i.e., it does not come out upside down)
  ddev->surface->globalTransform = oldSurface->globalTransform;
  ddev->surface->globalTransform.translate(0, bbox.y1 - bbox.y0);
  ddev->surface->globalTransform.scale(1, -1);
  ddev->surface->globalTransform.translate(-bbox.x0, -bbox.y0);

  resetTransform(ddev->surface);
}

void fz_qt4_draw_end_mask(void *user)
{
  // We'll pop the mask from the surface stack, convert it to MuPDF's internal
  // data structures, and then use fz_qt4_draw_clip_image_mask() to actually
  // use it for masking future drawing operations.
#ifdef MU_DEBUG
  qDebug() << "end_mask";
#endif
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  if (ddev->surface->type != draw_surface::MASK || ddev->surfaceStack->isEmpty()) {
    qDebug() << "end_mask without begin_mask()!";
    return;
  }

  // Get the mask image
  QImage * img = static_cast<QImage*>(ddev->surface->painter->device());

  // Convert the mask rect to an fz_rect
  fz_rect rect = toFzRect(ddev->surface->area);
  // Construct a CTM matrix that transforms a unit rectangle to the mask rect
  fz_matrix ctm;
  ctm.a = ddev->surface->area.width();
  ctm.b = 0;
  ctm.c = 0;
  ctm.d = ddev->surface->area.height();
  ctm.e = ddev->surface->area.left();
  ctm.f = ddev->surface->area.top();

  ddev->surface->painter->end();

  // Convert painted mask to a grayscale fz_pixmap
  // TODO: Use constBits() iff Qt >= 4.7
  unsigned char * imgData = img->bits();
  unsigned char * samples = new unsigned char[img->width() * img->height()];
  float colorfv[4];
  for (int i = 0; i < img->width() * img->height(); ++i) {
    float bc;
    colorfv[0] = imgData[4 * i + 0] / 255.;
    colorfv[1] = imgData[4 * i + 1] / 255.;
    colorfv[2] = imgData[4 * i + 2] / 255.;
    colorfv[3] = imgData[4 * i + 3] / 255.;
    fz_convert_color(fz_device_bgr, colorfv, fz_device_gray, &bc);
    samples[i] = (unsigned char)(255 * bc);
  }
  fz_pixmap * mask = fz_new_pixmap_with_data(fz_device_gray, img->width(), img->height(), samples);

  // Remove mask surface from stack
  delete img;
  delete ddev->surface;
  ddev->surface = ddev->surfaceStack->pop();

  if (ddev->surface->clipPath.isEmpty())
    ddev->surface->painter->setClipping(false);
  else {
    // Always push & pop paths without transformation or else recursive calls
    // would mess transformations up real bad
    QTransform oldT(ddev->surface->painter->transform());
    resetTransform(ddev->surface);
    ddev->surface->painter->setClipPath(ddev->surface->clipPath);
    ddev->surface->painter->setTransform(oldT);
  }

  fz_qt4_draw_clip_image_mask(user, mask, &rect, ctm);

  // Clean up
  fz_drop_pixmap(mask);
  delete[] samples;
}

void fz_qt4_draw_begin_group(void *user, fz_rect area, int isolated, int knockout, int blendmode, float alpha)
{
  // **TODO:** Implement!
  qDebug() << "begin_group [TODO]";
//	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
}

void fz_qt4_draw_end_group(void *user)
{
  // **TODO:** Implement!
  qDebug() << "end_group [TODO]";
//	fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
}



////////////////////////////////////////////////////////////////////////////////
// Text
////////////////////////////////////////////////////////////////////////////////

/*
// From <mupdf>/fitz/res_font.c
static fz_matrix
fz_adjust_ft_glyph_width(fz_font *font, int gid, fz_matrix trm)
{
  / * Fudge the font matrix to stretch the glyph if we've substituted the font. * /
  if (font->ft_substitute && gid < font->width_count)
  {
    FT_Error fterr;
    int subw;
    int realw;
    float scale;

    / * TODO: use FT_Get_Advance * /
    fterr = FT_Set_Char_Size((FT_Face)(font->ft_face), 1000, 1000, 72, 72);
    if (fterr)
//			fz_warn("freetype setting character size: %s", ft_error_string(fterr));
      qDebug() << QString::fromUtf8("freetype setting character size: %1").arg(QString::fromUtf8(ft_error_string(fterr)));

    fterr = FT_Load_Glyph((FT_Face)(font->ft_face), gid,
      FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM);
    if (fterr)
//			fz_warn("freetype failed to load glyph: %s", ft_error_string(fterr));
      qDebug() << QString::fromUtf8("freetype failed to load glyph: %1").arg(QString::fromUtf8(ft_error_string(fterr)));

    realw = ((FT_Face)font->ft_face)->glyph->metrics.horiAdvance;
    subw = font->width_table[gid];
    if (realw)
      scale = (float) subw / realw;
    else
      scale = 1;

    return fz_concat(fz_scale(scale, 1), trm);
  }

  return trm;
}
// End snippet
*/




void fz_qt4_draw_fill_text(void *user, fz_text *text, fz_matrix ctm,
  fz_colorspace *colorspace, float *color, float alpha)
{
#ifdef MU_DEBUG
  qDebug() << "draw_fill_text";
#endif
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;

  ddev->surface->painter->save();
  setTransform(ddev->surface, fz_matrix_to_QTransform(ctm));

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

  ddev->surface->painter->setBrush(QBrush(fz_color_to_QColor(colorspace, color, alpha)));
  ddev->surface->painter->setPen(QPen(Qt::NoPen));

/*
  if (text->font->ft_face) {
    // **TODO:** see <mupdf>/fitz/res_font.c @ fz_render_ft_glyph
    // **TODO:** Anti-aliasing, hinting, etc.
    FT_Face face = (FT_Face)text->font->ft_face;
    FT_Error fterr;


QList<QPainterPath> textPPs;

    for (int i = 0; i < text->len; ++i) {
      // **TODO:** see <mupdf>/fitz/res_font.c @ fz_render_ft_glyph
      // for substituted fonts, we should stretch the glyphs
      // fz_matrix trm = fz_adjust_ft_glyph_width(text->font, text->items[i].gid, text->trm);
      fz_matrix trm = text->trm;


    // Snippet taken from fz_render_ft_glyph (<mupdf>/fitz/res_font.c)
    // To avoid rounding errors when loading glyphs at small sizes, we load
    // it larger and scale it afterwards
    FT_Matrix m;
    FT_Vector v;
    m.xx = trm.a * 64; / * should be 65536 * /
    m.yx = trm.b * 64;
    m.xy = trm.c * 64;
    m.yy = trm.d * 64;
    v.x = trm.e * 64;
    v.y = trm.f * 64;
    fterr = FT_Set_Char_Size(face, 65536, 65536, 72, 72); / * should be 64, 64 * /
    if (fterr)
      qDebug() << "freetype setting character size:" << ft_error_string(fterr);
    FT_Set_Transform(face, &m, &v);
    // FIXME: font->ft_italic, font->ft_bold, font->ft_hint
    // End snippet from fz_render_ft_glyph



      // **TODO:** http://www.freetype.org/freetype2/docs/reference/ft2-base_interface.html#FT_Load_Glyph
      // **TODO:** Maybe cache QPainterPaths for common glyphs?
      fterr = FT_Load_Glyph(face, text->items[i].gid, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING);
      if (fterr)
        qDebug() << "freetype load glyph (gid" << text->items[i].gid << "):" << ft_error_string(fterr);

      if (face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
        QPainterPath pp(FTOutline_to_QPainterPath(face->glyph->outline));


/ *
        QTransform tt = ddev->painter->transform();

//				ddev->painter->setTransform((fz_matrix_to_QTransform(text->trm).translate(text->items[i].x, text->items[i].y) * fz_matrix_to_QTransform(ctm)).scale(1. / 64., 1. / 64.));

        ddev->painter->translate(text->items[i].x, text->items[i].y);
        ddev->painter->scale(1. / 64., 1. / 64.);
        ddev->painter->drawPath(pp);
        ddev->painter->setTransform(tt);
* /

        pp.translate(64 * text->items[i].x, 64 * text->items[i].y);
//				pp.scale(1. / 64., 1. / 64.);
        textPPs << pp;


      }
      else {
        // **TODO:** Bitmap fonts
      }
    }

ddev->painter->scale(1. / 64., 1. / 64.);
//ddev->painter->drawPath(textPP);
foreach (QPainterPath textPP, textPPs)
  ddev->painter->drawPath(textPP);

  }
  else if (text->font->t3procs) {
    // **TODO:** Type3 fonts
  }
  else
    qDebug() << "assert: uninitialized font structure";
*/

  QList<QPainterPath> textPPs(render_text(text));
  foreach (QPainterPath textPP, textPPs)
    ddev->surface->painter->drawPath(textPP);










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
  int gid; / * -1 for one gid to many ucs mappings * /
  int ucs; / * -1 for one ucs to many gid mappings * /
};
struct fz_font_s
{
  int refs;
  char name[32];

  void *ft_face; / * has an FT_Face if used * /
  int ft_substitute; / * ... substitute metrics * /
  int ft_bold; / * ... synthesize bold * /
  int ft_italic; / * ... synthesize italic * /
  int ft_hint; / * ... force hinting for DynaLab fonts * /

  / * origin of font data * /
  char *ft_file;
  unsigned char *ft_data;
  int ft_size;

  fz_matrix t3matrix;
  fz_obj *t3resources;
  fz_buffer **t3procs; / * has 256 entries if used * /
  float *t3widths; / * has 256 entries if used * /
  void *t3xref; / * a pdf_xref for the callback * /
  fz_error (*t3run)(void *xref, fz_obj *resources, fz_buffer *contents,
    struct fz_device_s *dev, fz_matrix ctm);

  fz_rect bbox;

  / * substitute metrics * /
  int width_count;
  int *width_table;
};
*/

  ddev->surface->painter->restore();


  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);
}

void fz_qt4_draw_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
  fz_colorspace *colorspace, float *color, float alpha)
{
  qDebug() << "draw_stroke_text";

  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;

  ddev->surface->painter->save();
  setTransform(ddev->surface, fz_matrix_to_QTransform(ctm));

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_begin(dev);

  ddev->surface->painter->setBrush(Qt::NoBrush);
  ddev->surface->painter->setPen(fz_stroke_state_to_QPen(stroke, colorspace, color, alpha));

  QList<QPainterPath> textPPs(render_text(text));
  foreach (QPainterPath textPP, textPPs)
    ddev->surface->painter->drawPath(textPP);

  ddev->surface->painter->restore();

  // **TODO:** knockout
//	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
//		fz_knockout_end(dev);
}

void fz_qt4_draw_ignore_text(void *user, fz_text *text, fz_matrix ctm)
{
  qDebug() << "draw_ignore_text";
}

void fz_qt4_draw_fill_image_mask(void *user, fz_pixmap *image, fz_matrix ctm,
  fz_colorspace *colorspace, float *color, float alpha)
{
  qDebug() << "draw_fill_image_mask";
  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;

  // Very similar to fz_qt4_draw_fill_image
  // 1) An appropriately sized image is created and filled with `color`
  // 2) `image` acts as alpha channel

  uchar * data = new uchar[4 * image->w * image->h];
  QColor baseColor = fz_color_to_QColor(colorspace, color, alpha);
  for (int i = 0; i < image->w * image->h; ++i) {
    data[4 * i + 0] = baseColor.blue();
    data[4 * i + 1] = baseColor.green();
    data[4 * i + 2] = baseColor.red();
    data[4 * i + 3] = baseColor.alpha() * image->samples[i] / 255;
  }

  QImage tmp_image(data, image->w, image->h, QImage::Format_ARGB32);

  // Set the transformation matrix
  // Note: the rotation/scale part works on normalized image coordinates and
  // is upside-down (as usual with pdf coordinates)
  ddev->surface->painter->save();
  setTransform(ddev->surface, fz_matrix_to_QTransform(ctm).scale(1./image->w, -1./image->h).translate(0, -image->h));
  ddev->surface->painter->drawImage(QPointF(0, 0), tmp_image);
  ddev->surface->painter->restore();

  delete[] data;
}

void fz_qt4_draw_begin_tile(void *user, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
  qDebug() << "draw_begin_tile [TODO]";

  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  draw_surface * oldSurface = ddev->surface;
  fz_bbox bbox;

  oldSurface->painter->save();
  setTransform(oldSurface, fz_matrix_to_QTransform(ctm));

  ddev->surfaceStack->push(oldSurface);

  ddev->surface = new draw_surface;
  ddev->surface->painter = new QPainter();
  ddev->surface->type = draw_surface::TILING;
  ddev->surface->area = toRectF(area);
  ddev->surface->xStep = xstep;
  ddev->surface->yStep = ystep;

  bbox = fz_round_rect(fz_transform_rect(ctm, view));

  QImage * img = new QImage(bbox.x1 - bbox.x0, bbox.y1 - bbox.y0, QImage::Format_ARGB32_Premultiplied);

  // Set background to be transparent white (with Format_ARGB32_Premultiplied,
  // this is actually identical to Qt::transparent)
  img->fill(QColor(255, 255, 255, 0));

  ddev->surface->painter->begin(img);

  // Shift everything in the new painter so that we only need to store a rect
  // of size bbox
  ddev->surface->globalTransform = oldSurface->globalTransform;
  ddev->surface->globalTransform.translate(-bbox.x0, -bbox.y0);
  resetTransform(ddev->surface);

}

void fz_qt4_draw_end_tile(void *user)
{
  qDebug() << "draw_end_tile";

  fz_qt4_draw_device * ddev = (fz_qt4_draw_device*)user;
  if (ddev->surfaceStack->isEmpty()) {
    qDebug() << "[WAR] Painter stack empty";
    return;
  }
  if (ddev->surface->type != draw_surface::TILING) {
    qDebug() << "[WAR] EndTile without BeginTile";
    return;
  }

  draw_surface * tilingSurface = ddev->surface;

  QImage * img = static_cast<QImage*>(tilingSurface->painter->device());
  tilingSurface->painter->end();
  delete tilingSurface->painter;
  ddev->surface = ddev->surfaceStack->pop();

  if (!img)
    return;

  for (float y = tilingSurface->area.top(); y < tilingSurface->area.bottom(); y += tilingSurface->yStep) {
    for (float x = tilingSurface->area.left(); x < tilingSurface->area.right(); x += tilingSurface->xStep) {
      // Note: we need to specify the size explicitly, as it is already
      // scaled to the final size; if we wouldn't do this, it would be
      // transformed again according to ddev->painter->transform(), which
      // we need to avoid
      ddev->surface->painter->drawImage(QRectF(x, y, tilingSurface->xStep, tilingSurface->yStep), *img);
    }
  }

  delete tilingSurface;
  delete img;

  ddev->surface->painter->restore();
}

fz_device *fz_new_qt4_draw_device(fz_glyph_cache *cache, QPainter * painter)
{
  fz_device *dev;
  fz_qt4_draw_device *ddev = (fz_qt4_draw_device*)fz_malloc(sizeof(fz_qt4_draw_device));
  ddev->surface = new draw_surface;
  ddev->surface->type = draw_surface::BASE;
  ddev->surface->painter = painter;

//	ddev->clipPaths = new QStack<QPainterPath>();
//	ddev->painterStack = new QStack<QPainter*>();
  ddev->surfaceStack = new QStack<draw_surface*>();

  qDebug() << "new device";


  dev = fz_new_device(ddev);

  dev->free_user = fz_qt4_draw_free_user;

  dev->fill_path = fz_qt4_draw_fill_path;
  dev->stroke_path = fz_qt4_draw_stroke_path;
  dev->clip_path = fz_qt4_draw_clip_path;
  dev->clip_stroke_path = fz_qt4_draw_clip_stroke_path;

  dev->fill_text = fz_qt4_draw_fill_text;
  dev->stroke_text = fz_qt4_draw_stroke_text;
  dev->clip_text = fz_qt4_draw_clip_text;
  dev->clip_stroke_text = fz_qt4_draw_clip_stroke_text;
  dev->ignore_text = fz_qt4_draw_ignore_text;

  dev->fill_image_mask = fz_qt4_draw_fill_image_mask;
  dev->clip_image_mask = fz_qt4_draw_clip_image_mask;
  dev->fill_image = fz_qt4_draw_fill_image;

  dev->fill_shade = fz_qt4_draw_fill_shade;
  dev->pop_clip = fz_qt4_draw_pop_clip;

  dev->begin_mask = fz_qt4_draw_begin_mask;
  dev->end_mask = fz_qt4_draw_end_mask;
  dev->begin_group = fz_qt4_draw_begin_group;
  dev->end_group = fz_qt4_draw_end_group;

  dev->begin_tile = fz_qt4_draw_begin_tile;
  dev->end_tile = fz_qt4_draw_end_tile;

  return dev;
}

}      // End MuPDF namespace
}      // End Backend namespace
}      // End QtPDF namespace
