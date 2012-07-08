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

#include <MuPDF.hpp>
#include <MuPDF-qt4.hpp>

namespace MuPDF
{

class MuPDFLocaleResetter
{
  char * _locale;
public:
  MuPDFLocaleResetter() { _locale = setlocale(LC_NUMERIC, NULL); setlocale(LC_NUMERIC, "C"); }
  ~MuPDFLocaleResetter() { setlocale(LC_NUMERIC, _locale); }
};


Document::Document(QString fileName):
  _pdf_data(NULL),
  _glyph_cache(fz_new_glyph_cache()),
  _numPages(-1)
{
  MuPDFLocaleResetter lr;

  // NOTE: The next two calls can fail---we need to check for that
  fz_stream *pdf_file = fz_open_file(fileName.toLocal8Bit().data());
  pdf_open_xref_with_stream(&_pdf_data, pdf_file, NULL);
  fz_close(pdf_file);

  // NOTE: This can also fail.
  pdf_load_page_tree(_pdf_data);

  _numPages = pdf_count_pages(_pdf_data);

}

Document::~Document()
{
  if( _pdf_data ){
    pdf_free_xref(_pdf_data);
    _pdf_data = NULL;
  }

  fz_free_glyph_cache(_glyph_cache);
}

int Document::numPages() { return _numPages; }
Page *Document::page(int at){ return new Page(this, at); }


// Page Class

Page::Page(Document *parent, int at):
  _page(NULL),
  _parent(parent),
  _n(at)
{
  pdf_page *page_data;
  pdf_load_page(&page_data, _parent->_pdf_data, _n);

  qreal _rotate = qreal(page_data->rotate);

  _bbox = page_data->mediabox;
  _size = QSizeF(qreal(_bbox.x1 - _bbox.x0), qreal(_bbox.y1 - _bbox.y0));

  // Here we parse the page into an intermediate format---this is a key
  // operation that Poppler lacks, it allows multiple renders to be made
  // without re-parsing the page.
  //
  // This is also time-intensive. It takes Poppler ~500 ms to create page
  // objects for the entire PGF Manual. MuPDF takes ~1000 ms, but only ~200 if
  // this step is omitted. May be useful to move this into a render function
  // but it will require us to keep page_data around.
  _page = fz_new_display_list();
  fz_device *dev = fz_new_list_device(_page);
  pdf_run_page(_parent->_pdf_data, page_data, dev, fz_identity);

  fz_free_device(dev);
  pdf_free_page(page_data);
}

Page::~Page() {
  if( _page )
    fz_free_display_list(_page);
}

int Page::pageNum() { return _n; }
qreal Page::rotate() { return _rotate; }
QSizeF Page::pageSizeF()   { return _size; }

QImage Page::renderToImage(double xres, double yres)
{
  // Set up the transformation matrix for the page. Really, we just start with
  // an identity matrix and scale it using the xres, yres inputs.
  fz_matrix render_trans = fz_identity;
//  render_trans = fz_concat(render_trans, fz_translate(0, -_bbox.y1));
  render_trans = fz_concat(render_trans, fz_scale(xres / 72.0f, yres / 72.0f));
  render_trans = fz_concat(render_trans, fz_rotate(_rotate));

  fz_bbox render_bbox = fz_round_rect(fz_transform_rect(render_trans, _bbox));

  // NOTE: Using fz_device_bgr or fz_device_rbg may depend on platform endianness.
  fz_pixmap *mu_image = fz_new_pixmap_with_rect(fz_device_bgr, render_bbox);
  // Flush to white.
  fz_clear_pixmap_with_color(mu_image, 255);
  
  QImage rendered_image(render_bbox.x1 - render_bbox.x0, render_bbox.y1 - render_bbox.y0, QImage::Format_ARGB32);
  QPainter painter;
  
  fz_device *renderer = fz_new_qt4_draw_device(_parent->_glyph_cache, &painter);

  // Actually start rendering the page.
  painter.begin(&rendered_image);
  painter.setRenderHint(QPainter::Antialiasing);

  // fill page with white background
  painter.fillRect(rendered_image.rect(), Qt::white);

  fz_execute_display_list(_page, renderer, render_trans, render_bbox);
  painter.end();

  // Dispose of unneeded items.
  fz_free_device(renderer);

  return rendered_image.mirrored();
}


} // End MuPDF namespace
