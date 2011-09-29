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

namespace MuPDF
{

extern "C"
{
#include <fitz.h>
#include <mupdf.h>
}


class Document
{
  // The pdf_xref is the main MuPDF object that represents a Document. Calls
  // that use it may have to be protected by a mutex.
  pdf_xref *_pdf_data;
  int _numPages;

public:

  Document(QString fileName):
    _pdf_data(NULL),
    _numPages(-1)
  {
    // NOTE: The next two calls can fail---we need to check for that
    fz_stream *pdf_file = fz_open_file(fileName.toLocal8Bit().data());
    pdf_open_xref_with_stream(&_pdf_data, pdf_file, NULL);
    fz_close(pdf_file);

    // NOTE: This can also fail.
    pdf_load_page_tree(_pdf_data);

    _numPages = pdf_count_pages(_pdf_data);
  }

  ~Document()
  {
    if( _pdf_data ){
      pdf_free_xref(_pdf_data);
      _pdf_data = NULL;
    }
  }

  int numPages() { return _numPages; }

};


} // End MuPDF namespace

#endif // End header guard
