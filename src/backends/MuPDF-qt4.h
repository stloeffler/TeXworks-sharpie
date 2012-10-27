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

namespace MuPDF
{

extern "C"
{
#include <fitz.h>
#include <mupdf.h>
}

fz_device *fz_new_qt4_draw_device(fz_glyph_cache *cache, QPainter * painter);



}      // End MuPDF namespace
#endif // End header guard
