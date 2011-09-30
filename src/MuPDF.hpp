/**
 * Copyright (C) 2011  Charlie Sharpsteen
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
#ifndef MuPDF_HPP
#define MuPDF_HPP

#include <QtCore>
#include <QImage>

namespace MuPDF
{

extern "C"
{
#include <fitz.h>
#include <mupdf.h>
}

class Document;
class Page;

class Document
{
  friend class Page;

  // The pdf_xref is the main MuPDF object that represents a Document. Calls
  // that use it may have to be protected by a mutex.
  pdf_xref *_pdf_data;
  fz_glyph_cache *_glyph_cache;
  int _numPages;

public:

  Document(QString fileName);
  ~Document();

  int numPages();
  Page *page(int at);

};


class Page
{
  Document *_parent;
  // The pdf_page is the main MuPDF object that represents a Page.
  fz_display_list *_page;
  // Keep as a Fitz object rather than QRect as it is used in rendering ops.
  fz_rect _bbox;
  QSizeF _size;
  qreal _rotate;
  const int _n;

public:

  Page(Document *parent, int at);
  ~Page();

  int pageNum();
  qreal rotate();
  QSizeF pageSizeF();

  QImage renderToImage(double xres, double yres);

};



}      // End MuPDF namespace
#endif // End header guard
