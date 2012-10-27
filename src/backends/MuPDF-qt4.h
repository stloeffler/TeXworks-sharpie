/**
 * Copyright (C) 2011  Stefan Löffler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#ifndef MuPDF_QT4_HPP
#define MuPDF_QT4_HPP

#include <QtGui>

extern "C"
{
#include <fitz.h>
#include <mupdf.h>
}

namespace QtPDF {

namespace Backend {

namespace MuPDF
{

QRectF toRectF(const fz_rect r);
QRectF toRectF(const fz_bbox r);

inline QTransform fz_matrix_to_QTransform(fz_matrix m)
{
  return QTransform(m.a, m.b, m.c, m.d, m.e, m.f);
}

inline fz_matrix QTransform_to_fz_matrix(const QTransform & t)
{
  fz_matrix m;
  m.a = t.m11();
  m.b = t.m12();
  m.c = t.m21();
  m.d = t.m22();
  m.e = t.dx();
  m.f = t.dy();
  return m;
}


fz_device *fz_new_qt4_draw_device(fz_glyph_cache *cache, QPainter * painter);



}      // End MuPDF namespace
}      // End Backend namespace
}      // End QtPDF namespace
#endif // End header guard
