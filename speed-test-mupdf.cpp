
#include <QTime>
#include <QImage>
#include <QDebug>


extern "C" {
#include <fitz.h>
#include <mupdf.h>
}

#define PROFILING(msg) qDebug() << (msg) << stopWatch.restart() << "ms";


int main()
{
	QTime stopWatch;
	int dpi;
	fz_glyph_cache *glyphcache;
	fz_device *dev;
	fz_error error;
	
	stopWatch.start();

	glyphcache = fz_new_glyph_cache();
	PROFILING("Initialization");


	pdf_xref *xref;
	error = pdf_open_xref(&xref, "pgfmanual.pdf", NULL);
	error = pdf_load_page_tree(xref);
	PROFILING("Load document");

	pdf_page *page;
	fz_display_list *list;
	list = fz_new_display_list();
	error = pdf_load_page(&page, xref, 0);
	dev = fz_new_list_device(list);
	error = pdf_run_page(xref, page, dev, fz_identity);
	fz_free_device(dev);
	PROFILING("Load page");

	qDebug() << "";
	qDebug() << "Render full page";
	qDebug() << "[dpi] [ms]";

	for (dpi = 100; dpi <= 1200; dpi += 100) {
		stopWatch.restart();
		QImage tmp;

		{
			float zoom;
			fz_matrix ctm;
			fz_bbox bbox;
			fz_pixmap *pix;

			zoom = dpi / 72;
			ctm = fz_translate(0, -page->mediabox.y1);
			ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
			ctm = fz_concat(ctm, fz_rotate(page->rotate));
			bbox = fz_round_rect(fz_transform_rect(ctm, page->mediabox));

			/* TODO: banded rendering and multi-page ppm */

			pix = fz_new_pixmap_with_rect(fz_device_bgr, bbox);
			fz_clear_pixmap_with_color(pix, 255);

			dev = fz_new_draw_device(glyphcache, pix);
			fz_execute_display_list(list, dev, ctm, bbox);
			fz_free_device(dev);

			QImage buffer(pix->samples, pix->w, pix->h, QImage::Format_ARGB32);
			tmp = buffer.copy();

			fz_drop_pixmap(pix);
		}

		qDebug() << dpi << stopWatch.restart();
	}
	return 0;
}

